#include "Doubler.h"

namespace
{
    constexpr float minDelaySamples = 0.0f;
    // Generous headroom above base delay (23 ms) + worst-case modulation
    // depth (~20 ms at maxDetuneCents and the slower LFO rate) so
    // setDelay()/popSample() never has to clamp against the delay line's own
    // capacity in normal operation; the runtime clamp below is purely a
    // defensive backstop.
    constexpr float maxDelayLineMs = 150.0f;

    float lerp (float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }
}

void Doubler::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    const auto maxDelaySamples = static_cast<int> (std::ceil (sampleRate * maxDelayLineMs / 1000.0)) + 4;

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    delayLineA.prepare (monoSpec);
    delayLineA.setMaximumDelayInSamples (maxDelaySamples);
    delayLineB.prepare (monoSpec);
    delayLineB.setMaximumDelayInSamples (maxDelaySamples);

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
    delayLineA.reset();
    delayLineB.reset();
    phaseA = 0.0;
    phaseB = juce::MathConstants<double>::pi;
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

    const auto depthSecA = maxPitchRatioDeviation / (juce::MathConstants<float>::twoPi * lfoRateHzA);
    const auto depthSecB = maxPitchRatioDeviation / (juce::MathConstants<float>::twoPi * lfoRateHzB);

    const auto depthSamplesA = depthSecA * static_cast<float> (sampleRate);
    const auto depthSamplesB = depthSecB * static_cast<float> (sampleRate);

    const auto baseDelaySamplesA = baseDelayMsA * 0.001f * static_cast<float> (sampleRate);
    const auto baseDelaySamplesB = baseDelayMsB * 0.001f * static_cast<float> (sampleRate);

    const auto maxDelaySamples = static_cast<float> (juce::jmax (1, delayLineA.getMaximumDelayInSamples()));

    const auto phaseIncrementA = juce::MathConstants<double>::twoPi * static_cast<double> (lfoRateHzA) / sampleRate;
    const auto phaseIncrementB = juce::MathConstants<double>::twoPi * static_cast<double> (lfoRateHzB) / sampleRate;

    const auto leftGainA = lerp (0.5f, 1.0f, width01);
    const auto rightGainA = lerp (0.5f, 0.0f, width01);
    const auto leftGainB = lerp (0.5f, 0.0f, width01);
    const auto rightGainB = lerp (0.5f, 1.0f, width01);

    auto* left = block.getChannelPointer (0);
    auto* right = numChannels > 1 ? block.getChannelPointer (1) : nullptr;

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        const auto monoSource = right != nullptr ? 0.5f * (left[sample] + right[sample]) : left[sample];

        const auto delaySamplesA = juce::jlimit (
            minDelaySamples, maxDelaySamples, baseDelaySamplesA + depthSamplesA * static_cast<float> (std::sin (phaseA)));
        const auto delaySamplesB = juce::jlimit (
            minDelaySamples, maxDelaySamples, baseDelaySamplesB + depthSamplesB * static_cast<float> (std::sin (phaseB)));

        delayLineA.pushSample (0, monoSource);
        delayLineB.pushSample (0, monoSource);

        const auto voiceA = delayLineA.popSample (0, delaySamplesA);
        const auto voiceB = delayLineB.popSample (0, delaySamplesB);

        phaseA += phaseIncrementA;
        if (phaseA >= juce::MathConstants<double>::twoPi)
            phaseA -= juce::MathConstants<double>::twoPi;

        phaseB += phaseIncrementB;
        if (phaseB >= juce::MathConstants<double>::twoPi)
            phaseB -= juce::MathConstants<double>::twoPi;

        if (bypassed)
            continue;

        if (right != nullptr)
        {
            left[sample] += amount01 * (voiceA * leftGainA + voiceB * leftGainB);
            right[sample] += amount01 * (voiceA * rightGainA + voiceB * rightGainB);
        }
        else
        {
            left[sample] += amount01 * 0.5f * (voiceA + voiceB);
        }
    }
}
