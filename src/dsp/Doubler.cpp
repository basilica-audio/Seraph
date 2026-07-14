#include "Doubler.h"

namespace
{
    constexpr float minDelaySamples = 0.0f;
    // Generous headroom above the largest base delay (29 ms) + worst-case
    // modulation depth (~20 ms at maxDetuneCents and the slowest LFO rate)
    // so setDelay()/popSample() never has to clamp against the delay line's
    // own capacity in normal operation; the runtime clamp below is purely a
    // defensive backstop.
    constexpr float maxDelayLineMs = 150.0f;
}

void Doubler::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    const auto maxDelaySamples = static_cast<int> (std::ceil (sampleRate * maxDelayLineMs / 1000.0)) + 4;

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (auto& delayLine : delayLines)
    {
        delayLine.prepare (monoSpec);
        delayLine.setMaximumDelayInSamples (maxDelaySamples);
    }

    amountSmoothed.reset (sampleRate, smoothingTimeSeconds);
    amountSmoothed.setCurrentAndTargetValue (lastAmount01);
    detuneSmoothed.reset (sampleRate, smoothingTimeSeconds);
    detuneSmoothed.setCurrentAndTargetValue (lastDetuneCents);
    widthSmoothed.reset (sampleRate, smoothingTimeSeconds);
    widthSmoothed.setCurrentAndTargetValue (lastWidth01);

    reset();
}

void Doubler::reset()
{
    for (auto& delayLine : delayLines)
        delayLine.reset();

    for (size_t voice = 0; voice < static_cast<size_t> (numVoices); ++voice)
        phases[voice] = voiceConfigs[voice].startPhase;
}

void Doubler::setAmountProportion (float newAmount01)
{
    lastAmount01 = newAmount01;
    amountSmoothed.setTargetValue (newAmount01);
}

void Doubler::setDetuneCents (float newDetuneCents)
{
    lastDetuneCents = newDetuneCents;
    detuneSmoothed.setTargetValue (newDetuneCents);
}

void Doubler::setWidthProportion (float newWidth01)
{
    lastWidth01 = newWidth01;
    widthSmoothed.setTargetValue (newWidth01);
}

void Doubler::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0 || numChannels == 0)
        return;

    const auto amount01 = juce::jlimit (0.0f, 1.0f, amountSmoothed.skip (static_cast<int> (numSamples)));
    const auto detuneCents = juce::jlimit (0.0f, maxDetuneCents, detuneSmoothed.skip (static_cast<int> (numSamples)));
    const auto width01 = juce::jlimit (0.0f, 1.0f, widthSmoothed.skip (static_cast<int> (numSamples)));

    const bool bypassed = amount01 <= 0.0f;

    // Peak instantaneous pitch-ratio deviation each voice's sinusoidal delay
    // modulation should produce, converted from cents. For a delay
    // modulated as delaySec(t) = depthSec * sin(2*pi*rate*t), the playback
    // rate is 1 - d(delaySec)/dt, whose peak deviation from 1 is
    // depthSec * 2*pi*rate - solving that for depthSec given a target peak
    // ratio deviation gives the depth used below.
    const auto maxPitchRatioDeviation = std::pow (2.0f, detuneCents / 1200.0f) - 1.0f;

    // Gain compensation so that decorrelated voices summed together don't
    // build up loudness as numVoices grows: at width == 0 (all voices
    // centered) and numVoices == 2 this reduces to the original v0.1
    // 0.5*(voiceA+voiceB) behaviour exactly; scaling by 2/numVoices keeps
    // the same overall level as voice count changes.
    constexpr auto voiceGainCompensation = 2.0f / static_cast<float> (numVoices);

    const auto maxDelaySamples = static_cast<float> (juce::jmax (1, delayLines[0].getMaximumDelayInSamples()));

    struct VoiceRuntime
    {
        float depthSamples;
        float baseDelaySamples;
        double phaseIncrement;
        float leftGain;
        float rightGain;
    };

    std::array<VoiceRuntime, numVoices> runtime {};

    for (size_t voice = 0; voice < static_cast<size_t> (numVoices); ++voice)
    {
        const auto& config = voiceConfigs[voice];

        const auto depthSec = maxPitchRatioDeviation / (juce::MathConstants<float>::twoPi * config.lfoRateHz);
        runtime[voice].depthSamples = depthSec * static_cast<float> (sampleRate);
        runtime[voice].baseDelaySamples = config.baseDelayMs * 0.001f * static_cast<float> (sampleRate);
        runtime[voice].phaseIncrement = juce::MathConstants<double>::twoPi * static_cast<double> (config.lfoRateHz) / sampleRate;

        const auto pan = config.panSpread * width01;
        runtime[voice].leftGain = 0.5f * (1.0f - pan);
        runtime[voice].rightGain = 0.5f * (1.0f + pan);
    }

    auto* left = block.getChannelPointer (0);
    auto* right = numChannels > 1 ? block.getChannelPointer (1) : nullptr;

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        const auto monoSource = right != nullptr ? 0.5f * (left[sample] + right[sample]) : left[sample];

        float leftSum = 0.0f;
        float rightSum = 0.0f;
        float monoSum = 0.0f;

        for (size_t voice = 0; voice < static_cast<size_t> (numVoices); ++voice)
        {
            auto& delayLine = delayLines[voice];
            auto& rt = runtime[voice];

            const auto delaySamples = juce::jlimit (
                minDelaySamples, maxDelaySamples, rt.baseDelaySamples + rt.depthSamples * static_cast<float> (std::sin (phases[voice])));

            delayLine.pushSample (0, monoSource);
            const auto voiceOutput = delayLine.popSample (0, delaySamples);

            phases[voice] += rt.phaseIncrement;
            if (phases[voice] >= juce::MathConstants<double>::twoPi)
                phases[voice] -= juce::MathConstants<double>::twoPi;

            if (bypassed)
                continue;

            leftSum += voiceOutput * rt.leftGain;
            rightSum += voiceOutput * rt.rightGain;
            monoSum += voiceOutput;
        }

        if (bypassed)
            continue;

        if (right != nullptr)
        {
            left[sample] += amount01 * voiceGainCompensation * leftSum;
            right[sample] += amount01 * voiceGainCompensation * rightSum;
        }
        else
        {
            // Mono buffers ignore width entirely (documented behaviour):
            // every voice is summed at its centered (0.5) gain regardless
            // of pan spread, matching the v0.1 unpanned mono path.
            left[sample] += amount01 * voiceGainCompensation * 0.5f * monoSum;
        }
    }
}
