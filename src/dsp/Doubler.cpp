#include "Doubler.h"

namespace
{
    constexpr float minDelaySamples = 0.0f;
    // Generous headroom above the largest base delay (24 ms, v0.2.0) +
    // worst-case modulation depth (~20 ms at maxDetuneCents and the slowest
    // LFO rate) so setDelay()/popSample() never has to clamp against the
    // delay line's own capacity in normal operation; the runtime clamp below
    // is purely a defensive backstop. Also covers the Shift path's base
    // pre-delay plus the humaniser's +/- 10 ms timing drift.
    constexpr float maxDelayLineMs = 150.0f;
}

void Doubler::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    const auto maxDelaySamples = static_cast<int> (std::ceil (sampleRate * maxDelayLineMs / 1000.0)) + 4;
    const auto maximumBlockSize = static_cast<int> (juce::jmax (juce::uint32 (1), spec.maximumBlockSize));

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (auto& delayLine : delayLines)
    {
        delayLine.prepare (monoSpec);
        delayLine.setMaximumDelayInSamples (maxDelaySamples);
    }

    for (auto& delayLine : shiftPreDelays)
    {
        delayLine.prepare (monoSpec);
        delayLine.setMaximumDelayInSamples (maxDelaySamples);
    }

    for (size_t voice = 0; voice < static_cast<size_t> (numVoices); ++voice)
    {
        microShifters[voice].prepare (sampleRate, voiceConfigs[voice].baseDelayMs);
        spectralShifters[voice].prepare (sampleRate, maximumBlockSize);
        spectralShifters[voice].setFormantPreserveEnabled (formantPreserve);
        humanizers[voice].prepare (sampleRate, static_cast<int> (voice));
        voiceScratch[voice].assign (static_cast<size_t> (maximumBlockSize), 0.0f);
    }

    monoScratch.assign (static_cast<size_t> (maximumBlockSize), 0.0f);

    amountSmoothed.reset (sampleRate, smoothingTimeSeconds);
    amountSmoothed.setCurrentAndTargetValue (lastAmount01);
    detuneSmoothed.reset (sampleRate, smoothingTimeSeconds);
    detuneSmoothed.setCurrentAndTargetValue (lastDetuneCents);
    widthSmoothed.reset (sampleRate, smoothingTimeSeconds);
    widthSmoothed.setCurrentAndTargetValue (lastWidth01);
    humanizeSmoothed.reset (sampleRate, smoothingTimeSeconds);
    humanizeSmoothed.setCurrentAndTargetValue (lastHumanize01);

    fadeIncrement = static_cast<float> (1.0 / juce::jmax (1.0, modeFadeSeconds * sampleRate));

    prepared = true;

    reset();
}

void Doubler::reset()
{
    for (auto& delayLine : delayLines)
        delayLine.reset();

    for (auto& delayLine : shiftPreDelays)
        delayLine.reset();

    for (auto& shifter : microShifters)
        shifter.reset();

    for (auto& shifter : spectralShifters)
        shifter.reset();

    for (auto& humanizer : humanizers)
        humanizer.reset();

    for (size_t voice = 0; voice < static_cast<size_t> (numVoices); ++voice)
        phases[voice] = voiceConfigs[voice].startPhase;

    // A reset is a clean start, so there is nothing to fade away from.
    activeMode = targetMode;
    fadeState = FadeState::idle;
    fadeGain = 1.0f;
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

void Doubler::setHumanizeProportion (float newHumanize01)
{
    lastHumanize01 = newHumanize01;
    humanizeSmoothed.setTargetValue (newHumanize01);
}

void Doubler::setMode (Mode newMode)
{
    if (newMode == targetMode)
        return;

    targetMode = newMode;

    if (! prepared)
    {
        // Configured before the first prepare(): nothing is running yet, so
        // there is no discontinuity to mask.
        activeMode = newMode;
        fadeState = FadeState::idle;
        fadeGain = 1.0f;
        return;
    }

    fadeState = FadeState::fadingOut;
}

void Doubler::setFormantPreserveEnabled (bool shouldPreserve)
{
    if (shouldPreserve == formantPreserve)
        return;

    formantPreserve = shouldPreserve;

    for (auto& shifter : spectralShifters)
        shifter.setFormantPreserveEnabled (formantPreserve);
}

int Doubler::getLatencySamples() const noexcept
{
    return targetMode == Mode::shift ? spectralShifters[0].getLatencySamples() : 0;
}

int Doubler::getMaximumLatencySamples() const noexcept
{
    return spectralShifters[0].getLatencySamples();
}

void Doubler::resetActiveModeState()
{
    // Only the newly selected engine needs clearing; the others keep running
    // silently so a switch back is equally seamless.
    switch (activeMode)
    {
        case Mode::classic:
            for (auto& delayLine : delayLines)
                delayLine.reset();
            break;

        case Mode::micro:
            for (auto& shifter : microShifters)
                shifter.reset();
            break;

        case Mode::shift:
            for (auto& shifter : spectralShifters)
                shifter.reset();

            for (auto& delayLine : shiftPreDelays)
                delayLine.reset();
            break;
    }
}

void Doubler::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Defensive: a host sending a block larger than prepare() was told about
    // would otherwise overrun the scratch buffers.
    const auto samples = juce::jmin (static_cast<int> (numSamples), static_cast<int> (monoScratch.size()));

    if (samples <= 0)
        return;

    const auto amount01 = juce::jlimit (0.0f, 1.0f, amountSmoothed.skip (samples));
    const auto detuneCents = juce::jlimit (0.0f, maxDetuneCents, detuneSmoothed.skip (samples));
    const auto width01 = juce::jlimit (0.0f, 1.0f, widthSmoothed.skip (samples));
    const auto humanize01 = juce::jlimit (0.0f, 1.0f, humanizeSmoothed.skip (samples));

    const bool bypassed = amount01 <= 0.0f;

    // The mode switch itself happens on a slice boundary, once the fade-out
    // has reached silence: resetting an STFT engine is not something to do
    // halfway through a run of samples.
    if (fadeState == FadeState::fadingOut && fadeGain <= 0.0f)
    {
        activeMode = targetMode;
        resetActiveModeState();
        fadeState = FadeState::fadingIn;
    }

    // Peak instantaneous pitch-ratio deviation each voice's sinusoidal delay
    // modulation should produce, converted from cents. For a delay
    // modulated as delaySec(t) = depthSec * sin(2*pi*rate*t), the playback
    // rate is 1 - d(delaySec)/dt, whose peak deviation from 1 is
    // depthSec * 2*pi*rate - solving that for depthSec given a target peak
    // ratio deviation gives the depth used below. Classic mode only: Micro
    // and Shift express the same cents value as a constant offset instead.
    const auto maxPitchRatioDeviation = std::pow (2.0f, detuneCents / 1200.0f) - 1.0f;

    // Gain compensation so that decorrelated voices summed together don't
    // build up loudness as numVoices grows: at width == 0 (all voices
    // centered) and numVoices == 2 this reduces to the original v0.1
    // 0.5*(voiceA+voiceB) behaviour exactly; scaling by 2/numVoices keeps
    // the same overall level as voice count changes.
    constexpr auto voiceGainCompensation = 2.0f / static_cast<float> (numVoices);

    const auto maxDelaySamples = static_cast<float> (juce::jmax (1, delayLines[0].getMaximumDelayInSamples()));
    const auto sampleRateF = static_cast<float> (sampleRate);

    struct VoiceRuntime
    {
        float leftGain;
        float rightGain;
    };

    std::array<VoiceRuntime, numVoices> runtime {};

    auto* left = block.getChannelPointer (0);
    auto* right = numChannels > 1 ? block.getChannelPointer (1) : nullptr;

    // Pass 1: the mono sum every voice is derived from. Hoisting this out of
    // the per-sample loop is bit-identical to v0.2.0's interleaved version
    // because the doubler only ever *adds* to the buffer - it never reads a
    // sample it has already written.
    if (right != nullptr)
    {
        for (int sample = 0; sample < samples; ++sample)
            monoScratch[static_cast<size_t> (sample)] = 0.5f * (left[sample] + right[sample]);
    }
    else
    {
        for (int sample = 0; sample < samples; ++sample)
            monoScratch[static_cast<size_t> (sample)] = left[sample];
    }

    // Pass 2: per-voice rendering into scratch.
    for (size_t voice = 0; voice < static_cast<size_t> (numVoices); ++voice)
    {
        const auto& config = voiceConfigs[voice];
        auto& rt = runtime[voice];
        auto& humanizer = humanizers[voice];
        auto* voiceOut = voiceScratch[voice].data();

        // The humaniser advances in every mode and at every depth, so its
        // phase relationship to the audio never depends on how long Humanize
        // happened to have been at zero.
        humanizer.advance (samples);

        const auto timingOffsetMs = humanizer.getTimingOffsetMs (humanize01);
        const auto levelGain = humanizer.getLevelGain (humanize01);

        const auto depthSec = maxPitchRatioDeviation / (juce::MathConstants<float>::twoPi * config.lfoRateHz);
        const auto depthSamples = depthSec * sampleRateF;

        // Folding the humanise offset into the base delay *before* the
        // modulation term is what keeps Humanize == 0 bit-identical: `x +
        // 0.0f` is exactly `x`, whereas adding a zero term after the
        // modulation would reassociate the sum.
        const auto baseDelaySamples = (config.baseDelayMs + timingOffsetMs) * 0.001f * sampleRateF;
        const auto phaseIncrement = juce::MathConstants<double>::twoPi * static_cast<double> (config.lfoRateHz) / sampleRate;

        const auto pan = config.panSpread * width01;
        rt.leftGain = 0.5f * (1.0f - pan);
        rt.rightGain = 0.5f * (1.0f + pan);

        switch (activeMode)
        {
            case Mode::classic:
            {
                auto& delayLine = delayLines[voice];

                for (int sample = 0; sample < samples; ++sample)
                {
                    const auto delaySamples = juce::jlimit (
                        minDelaySamples,
                        maxDelaySamples,
                        baseDelaySamples + depthSamples * static_cast<float> (std::sin (phases[voice])));

                    delayLine.pushSample (0, monoScratch[static_cast<size_t> (sample)]);
                    voiceOut[sample] = delayLine.popSample (0, delaySamples) * levelGain;

                    phases[voice] += phaseIncrement;
                    if (phases[voice] >= juce::MathConstants<double>::twoPi)
                        phases[voice] -= juce::MathConstants<double>::twoPi;
                }

                break;
            }

            case Mode::micro:
            {
                auto& shifter = microShifters[voice];

                shifter.setDetuneCents (detuneCents * voiceDetuneScalers[voice]
                                        + humanizer.getPitchOffsetCents (humanize01));
                shifter.setBaseDelayOffsetMs (timingOffsetMs);

                for (int sample = 0; sample < samples; ++sample)
                    voiceOut[sample] = shifter.processSample (monoScratch[static_cast<size_t> (sample)]) * levelGain;

                // Classic's LFO keeps running underneath so a switch back to
                // it does not restart from a stale phase.
                phases[voice] += phaseIncrement * samples;
                while (phases[voice] >= juce::MathConstants<double>::twoPi)
                    phases[voice] -= juce::MathConstants<double>::twoPi;

                break;
            }

            case Mode::shift:
            {
                auto& shifter = spectralShifters[voice];
                auto& preDelay = shiftPreDelays[voice];

                shifter.setDetuneCents (detuneCents * voiceDetuneScalers[voice]
                                        + humanizer.getPitchOffsetCents (humanize01));

                shifter.process (monoScratch.data(), voiceOut, samples);

                const auto preDelaySamples = juce::jlimit (minDelaySamples, maxDelaySamples, baseDelaySamples);

                for (int sample = 0; sample < samples; ++sample)
                {
                    preDelay.pushSample (0, voiceOut[sample]);
                    voiceOut[sample] = preDelay.popSample (0, preDelaySamples) * levelGain;
                }

                phases[voice] += phaseIncrement * samples;
                while (phases[voice] >= juce::MathConstants<double>::twoPi)
                    phases[voice] -= juce::MathConstants<double>::twoPi;

                break;
            }
        }
    }

    if (bypassed)
    {
        // Every engine's state has advanced above; the buffer stays exactly
        // as it was found. The mode fade still has to run down, otherwise
        // re-enabling Double after a mode change would resume mid-fade.
        for (int sample = 0; sample < samples; ++sample)
        {
            if (fadeState == FadeState::fadingOut)
                fadeGain = juce::jmax (0.0f, fadeGain - fadeIncrement);
            else if (fadeState == FadeState::fadingIn)
                fadeGain = juce::jmin (1.0f, fadeGain + fadeIncrement);
        }

        if (fadeState == FadeState::fadingIn && fadeGain >= 1.0f)
            fadeState = FadeState::idle;

        return;
    }

    // Pass 3: pan, sum and add. The accumulation order across voices is
    // unchanged from v0.2.0, which matters: floating-point addition is not
    // associative, and Classic mode has to stay bit-identical.
    const auto fadeActive = fadeState != FadeState::idle;

    for (int sample = 0; sample < samples; ++sample)
    {
        float leftSum = 0.0f;
        float rightSum = 0.0f;
        float monoSum = 0.0f;

        for (size_t voice = 0; voice < static_cast<size_t> (numVoices); ++voice)
        {
            const auto voiceOutput = voiceScratch[voice][static_cast<size_t> (sample)];
            const auto& rt = runtime[voice];

            leftSum += voiceOutput * rt.leftGain;
            rightSum += voiceOutput * rt.rightGain;
            monoSum += voiceOutput;
        }

        if (fadeActive)
        {
            if (fadeState == FadeState::fadingOut)
                fadeGain = juce::jmax (0.0f, fadeGain - fadeIncrement);
            else
                fadeGain = juce::jmin (1.0f, fadeGain + fadeIncrement);

            leftSum *= fadeGain;
            rightSum *= fadeGain;
            monoSum *= fadeGain;
        }

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

    if (fadeState == FadeState::fadingIn && fadeGain >= 1.0f)
        fadeState = FadeState::idle;
}
