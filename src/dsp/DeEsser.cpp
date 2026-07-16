#include "DeEsser.h"
#include "RealtimeCoefficients.h"

namespace
{
    constexpr double smoothingTimeSeconds = 0.05;

    // Keeps a requested filter frequency safely below Nyquist regardless of
    // host sample rate, so juce::dsp::IIR::Coefficients::makeBandPass never
    // receives an out-of-range value (which would produce invalid/NaN
    // coefficients).
    float clampBelowNyquist (float frequencyHz, double sampleRate) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRate) * 0.5f;
        return juce::jlimit (200.0f, nyquist * 0.9f, frequencyHz);
    }

    // DeEssWidth (0-100%) -> detector Q, linear between the two extremes
    // documented in DeEsser.h (0% -> maxDetectorQ/narrow, 100% ->
    // minDetectorQ/wide).
    float widthToQ (float width01, float minQ, float maxQ) noexcept
    {
        return juce::jmap (juce::jlimit (0.0f, 1.0f, width01), 0.0f, 1.0f, maxQ, minQ);
    }
}

void DeEsser::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    const auto numChannels = static_cast<size_t> (spec.numChannels);
    detectorFilters.clear();
    detectorFilters.resize (numChannels); // Filter<float> is move-only (owns a HeapBlock), so resize() rather than assign()
    envelopeState.assign (numChannels, 0.0f);

    detectorCoefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (
        sampleRate, clampBelowNyquist (lastFrequencyHz, sampleRate), widthToQ (lastWidth01, minDetectorQ, maxDetectorQ));

    for (auto& filter : detectorFilters)
        filter.coefficients = detectorCoefficients;

    frequencySmoothed.reset (sampleRate, smoothingTimeSeconds);
    frequencySmoothed.setCurrentAndTargetValue (lastFrequencyHz);
    amountSmoothed.reset (sampleRate, smoothingTimeSeconds);
    amountSmoothed.setCurrentAndTargetValue (lastAmount01);
    widthSmoothed.reset (sampleRate, smoothingTimeSeconds);
    widthSmoothed.setCurrentAndTargetValue (lastWidth01);

    reset();
}

void DeEsser::reset()
{
    for (auto& filter : detectorFilters)
        filter.reset();

    std::fill (envelopeState.begin(), envelopeState.end(), 0.0f);
    currentGainReductionDb = 0.0f;
}

void DeEsser::setAmountProportion (float newAmount01)
{
    lastAmount01 = newAmount01;
    amountSmoothed.setTargetValue (newAmount01);
}

void DeEsser::setFrequencyHz (float newFrequencyHz)
{
    lastFrequencyHz = newFrequencyHz;
    frequencySmoothed.setTargetValue (newFrequencyHz);
}

void DeEsser::setWidthProportion (float newWidth01)
{
    lastWidth01 = newWidth01;
    widthSmoothed.setTargetValue (newWidth01);
}

void DeEsser::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0)
        return;

    const auto frequencyHz = clampBelowNyquist (frequencySmoothed.skip (static_cast<int> (numSamples)), sampleRate);
    const auto amount01 = juce::jlimit (0.0f, 1.0f, amountSmoothed.skip (static_cast<int> (numSamples)));
    const auto width01 = juce::jlimit (0.0f, 1.0f, widthSmoothed.skip (static_cast<int> (numSamples)));
    const auto detectorQ = widthToQ (width01, minDetectorQ, maxDetectorQ);

    // Bit-exact bypass at amount == 0: skip the whole reduction computation
    // (still advance the detector filter/envelope state below so re-enabling
    // DeEss mid-stream doesn't start from a discontinuous filter state), and
    // never touch `block`'s samples.
    const bool bypassed = amount01 <= 0.0f;

    // Real-time-safe recompute: ArrayCoefficients::makeBandPass returns a
    // stack std::array (no allocation), written in place into the already-
    // allocated detectorCoefficients object below - unlike
    // Coefficients<float>::makeBandPass (used once in prepare() above),
    // which heap-allocates a brand new Coefficients object on every call.
    // See RealtimeCoefficients.h and basilica-audio/Seraph issue #13.
    const auto rawCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sampleRate, frequencyHz, detectorQ);
    srph::applyBiquadCoefficients (*detectorCoefficients, rawCoefficients);

    const auto maxReductionDb = amount01 * maxReductionRangeDb;

    // One-pole attack/release envelope coefficients, recomputed once per
    // block from the (fixed) time constants - cheap and standard practice
    // for a level detector running at block rate.
    const auto attackCoeff = std::exp (-1.0 / (attackTimeSeconds * sampleRate));
    const auto releaseCoeff = std::exp (-1.0 / (releaseTimeSeconds * sampleRate));

    float lastGainReductionDb = 0.0f;

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto* data = block.getChannelPointer (channel);
        auto& envelope = envelopeState[channel];
        auto& filter = detectorFilters[channel];

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const auto inputSample = data[sample];
            const auto bandpassed = filter.processSample (inputSample);

            const auto rectified = bandpassed * bandpassed;
            const auto coeff = rectified > envelope ? attackCoeff : releaseCoeff;
            envelope = static_cast<float> (coeff * envelope + (1.0 - coeff) * rectified);

            // Listen mode replaces the output with the raw detected band
            // regardless of the current DeEss amount/bypass state, so
            // DeEssFreq can be tuned by ear before dialling in reduction.
            if (listenEnabled)
            {
                data[sample] = bandpassed;
                continue;
            }

            if (bypassed)
                continue;

            const auto envelopeDb = juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (envelope, 1.0e-12f)), -120.0f);
            const auto overshootDb = juce::jmax (0.0f, envelopeDb - thresholdDb);
            const auto reductionDb = juce::jlimit (0.0f, maxReductionDb, overshootDb);
            const auto gainFactor = juce::Decibels::decibelsToGain (-reductionDb);

            data[sample] = inputSample + bandpassed * (gainFactor - 1.0f);
            lastGainReductionDb = reductionDb;
        }
    }

    if (! bypassed)
        currentGainReductionDb = lastGainReductionDb;
    else
        currentGainReductionDb = 0.0f;
}
