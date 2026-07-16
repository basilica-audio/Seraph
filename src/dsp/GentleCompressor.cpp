#include "GentleCompressor.h"

void GentleCompressor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    const auto numChannels = static_cast<size_t> (spec.numChannels);
    envelopeFastState.assign (numChannels, 0.0f);
    envelopeSlowState.assign (numChannels, 0.0f);
    releaseWeightState.assign (numChannels, 0.0f);

    amountSmoothed.reset (sampleRate, smoothingTimeSeconds);
    amountSmoothed.setCurrentAndTargetValue (lastAmount01);

    reset();
}

void GentleCompressor::reset()
{
    std::fill (envelopeFastState.begin(), envelopeFastState.end(), 0.0f);
    std::fill (envelopeSlowState.begin(), envelopeSlowState.end(), 0.0f);
    std::fill (releaseWeightState.begin(), releaseWeightState.end(), 0.0f);
    currentGainReductionDb = 0.0f;
}

void GentleCompressor::setAmountProportion (float newAmount01)
{
    lastAmount01 = newAmount01;
    amountSmoothed.setTargetValue (newAmount01);
}

void GentleCompressor::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0 || numChannels == 0)
        return;

    const auto amount01 = juce::jlimit (0.0f, 1.0f, amountSmoothed.skip (static_cast<int> (numSamples)));

    // Bit-exact bypass at amount == 0: skip the whole reduction computation
    // (still advance the envelope state below so re-enabling Comp mid-stream
    // doesn't start from a discontinuous envelope), and never touch
    // `block`'s samples.
    const bool bypassed = amount01 <= 0.0f;

    const auto thresholdDb = juce::jmap (amount01, 0.0f, 1.0f, 0.0f, thresholdMinDb);
    const auto ratio = juce::jmap (amount01, 0.0f, 1.0f, 1.0f, maxRatio);
    const auto ratioFactor = 1.0f - (1.0f / ratio);

    const auto attackCoeff = std::exp (-1.0 / (attackTimeSeconds * sampleRate));
    const auto releaseFastCoeff = std::exp (-1.0 / (releaseFastTimeSeconds * sampleRate));
    const auto releaseSlowCoeff = std::exp (-1.0 / (releaseSlowTimeSeconds * sampleRate));
    const auto releaseWeightAttackCoeff = std::exp (-1.0 / (releaseWeightAttackTimeSeconds * sampleRate));
    const auto releaseWeightReleaseCoeff = std::exp (-1.0 / (releaseWeightReleaseTimeSeconds * sampleRate));

    float lastGainReductionDb = 0.0f;

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto* data = block.getChannelPointer (channel);
        auto& envelopeFast = envelopeFastState[channel];
        auto& envelopeSlow = envelopeSlowState[channel];
        auto& releaseWeight = releaseWeightState[channel];

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const auto inputSample = data[sample];

            const auto rectified = inputSample * inputSample;
            // Blended envelope from the *previous* sample drives this
            // sample's attack/release decision (matches v1's own
            // rectified-vs-envelope hysteresis, now against the blended
            // value instead of a single envelope).
            const auto blendedEnvelopePrev = envelopeFast * (1.0f - releaseWeight) + envelopeSlow * releaseWeight;
            const bool attacking = rectified > blendedEnvelopePrev;

            // Program-dependent release blend weight: snaps quickly toward
            // the fast path (0) on a fresh transient, drifts slowly toward
            // the slow path (1) while gain reduction is continuously active.
            // Always a smoothed one-pole update, never a switched value - no
            // discontinuity is introduced at the blend boundary.
            const auto releaseWeightCoeff = attacking ? releaseWeightAttackCoeff : releaseWeightReleaseCoeff;
            const auto releaseWeightTarget = attacking ? 0.0 : 1.0;
            releaseWeight = static_cast<float> (releaseWeightCoeff * releaseWeight + (1.0 - releaseWeightCoeff) * releaseWeightTarget);

            const auto fastCoeff = attacking ? attackCoeff : releaseFastCoeff;
            envelopeFast = static_cast<float> (fastCoeff * envelopeFast + (1.0 - fastCoeff) * rectified);

            const auto slowCoeff = attacking ? attackCoeff : releaseSlowCoeff;
            envelopeSlow = static_cast<float> (slowCoeff * envelopeSlow + (1.0 - slowCoeff) * rectified);

            if (bypassed)
                continue;

            const auto envelope = envelopeFast * (1.0f - releaseWeight) + envelopeSlow * releaseWeight;
            const auto envelopeDb = juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (envelope, 1.0e-12f)), -120.0f);
            const auto overshootDb = juce::jmax (0.0f, envelopeDb - thresholdDb);
            const auto reductionDb = overshootDb * ratioFactor;
            const auto gainFactor = juce::Decibels::decibelsToGain (-reductionDb);

            data[sample] = inputSample * gainFactor;
            lastGainReductionDb = reductionDb;
        }
    }

    if (! bypassed)
        currentGainReductionDb = lastGainReductionDb;
    else
        currentGainReductionDb = 0.0f;
}
