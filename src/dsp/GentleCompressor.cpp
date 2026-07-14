#include "GentleCompressor.h"

void GentleCompressor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    envelopeState.assign (static_cast<size_t> (spec.numChannels), 0.0f);

    amountSmoothed.reset (sampleRate, smoothingTimeSeconds);
    amountSmoothed.setCurrentAndTargetValue (lastAmount01);

    reset();
}

void GentleCompressor::reset()
{
    std::fill (envelopeState.begin(), envelopeState.end(), 0.0f);
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
    const auto releaseCoeff = std::exp (-1.0 / (releaseTimeSeconds * sampleRate));

    float lastGainReductionDb = 0.0f;

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto* data = block.getChannelPointer (channel);
        auto& envelope = envelopeState[channel];

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const auto inputSample = data[sample];

            const auto rectified = inputSample * inputSample;
            const auto coeff = rectified > envelope ? attackCoeff : releaseCoeff;
            envelope = static_cast<float> (coeff * envelope + (1.0 - coeff) * rectified);

            if (bypassed)
                continue;

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
