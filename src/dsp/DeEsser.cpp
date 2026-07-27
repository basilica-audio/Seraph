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
    numPreparedChannels = static_cast<int> (numChannels);

    detectorFilters.clear();
    detectorFilters.resize (numChannels); // Filter<float> is move-only (owns a HeapBlock), so resize() rather than assign()
    envelopeState.assign (numChannels, 0.0f);

    // Ring buffers big enough for the maximum lookahead at this sample rate,
    // plus one guard sample so a read at the maximum offset never collides
    // with the write position.
    const auto maxLookaheadSamples = static_cast<int> (std::ceil (maxLookaheadMs * 0.001f * sampleRate));
    historyLength = juce::jmax (2, maxLookaheadSamples + 2);
    inputHistory.assign (numChannels * static_cast<size_t> (historyLength), 0.0f);
    bandHistory.assign (numChannels * static_cast<size_t> (historyLength), 0.0f);

    gainMinima.clear();
    gainMinima.resize (numChannels);
    for (auto& minimum : gainMinima)
        minimum.prepare (maxLookaheadSamples + 1);

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
    kneeSmoothed.reset (sampleRate, smoothingTimeSeconds);
    kneeSmoothed.setCurrentAndTargetValue (lastKneeWidthDb);

    lookaheadFadeIncrement = static_cast<float> (1.0 / juce::jmax (1.0, lookaheadFadeSeconds * sampleRate));

    setLookaheadMs (lastLookaheadMs);
    activeLookaheadSamples = targetLookaheadSamples;
    previousLookaheadSamples = targetLookaheadSamples;
    lookaheadFadeGain = 1.0f;

    for (auto& minimum : gainMinima)
        minimum.setWindowLength (activeLookaheadSamples + 1);

    reset();
}

void DeEsser::reset()
{
    for (auto& filter : detectorFilters)
        filter.reset();

    std::fill (envelopeState.begin(), envelopeState.end(), 0.0f);
    std::fill (inputHistory.begin(), inputHistory.end(), 0.0f);
    std::fill (bandHistory.begin(), bandHistory.end(), 0.0f);

    for (auto& minimum : gainMinima)
        minimum.reset();

    historyWriteIndex = 0;
    activeLookaheadSamples = targetLookaheadSamples;
    previousLookaheadSamples = targetLookaheadSamples;
    lookaheadFadeGain = 1.0f;
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

void DeEsser::setKneeWidthDb (float newKneeWidthDb)
{
    lastKneeWidthDb = newKneeWidthDb;
    kneeSmoothed.setTargetValue (newKneeWidthDb);
}

void DeEsser::setLookaheadMs (float newLookaheadMs)
{
    lastLookaheadMs = juce::jlimit (0.0f, maxLookaheadMs, newLookaheadMs);

    const auto maximumSamples = juce::jmax (0, historyLength - 2);
    targetLookaheadSamples = juce::jlimit (
        0, maximumSamples, static_cast<int> (std::lround (lastLookaheadMs * 0.001f * sampleRate)));
}

float DeEsser::computeReductionDb (float overshootDb, float maxReductionDb) const noexcept
{
    // Hard knee: the v0.2.0 expression, evaluated verbatim so DeEssKnee == 0
    // is bit-identical rather than merely equivalent.
    const auto kneeWidthDb = juce::jlimit (0.0f, 12.0f, kneeSmoothed.getCurrentValue());

    if (kneeWidthDb <= 0.0f)
        return juce::jlimit (0.0f, maxReductionDb, juce::jmax (0.0f, overshootDb));

    const auto halfKnee = kneeWidthDb * 0.5f;

    if (overshootDb < -halfKnee)
        return 0.0f;

    if (overshootDb <= halfKnee)
    {
        // Quadratic interpolation across the knee: value and first
        // derivative both match the hard-knee asymptotes at each end (0 and
        // slope 0 at -W/2; W/2 and slope 1 at +W/2).
        const auto aboveKneeStart = overshootDb + halfKnee;
        return juce::jmin (maxReductionDb, aboveKneeStart * aboveKneeStart / (2.0f * kneeWidthDb));
    }

    return juce::jmin (maxReductionDb, overshootDb);
}

void DeEsser::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0 || historyLength <= 0)
        return;

    const auto channels = juce::jmin (static_cast<int> (numChannels), numPreparedChannels);

    if (channels <= 0)
        return;

    const auto frequencyHz = clampBelowNyquist (frequencySmoothed.skip (static_cast<int> (numSamples)), sampleRate);
    const auto amount01 = juce::jlimit (0.0f, 1.0f, amountSmoothed.skip (static_cast<int> (numSamples)));
    const auto width01 = juce::jlimit (0.0f, 1.0f, widthSmoothed.skip (static_cast<int> (numSamples)));
    kneeSmoothed.skip (static_cast<int> (numSamples));
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

    // A lookahead change re-points the read head; crossfade from the old
    // position to the new one rather than jumping.
    if (targetLookaheadSamples != activeLookaheadSamples)
    {
        previousLookaheadSamples = activeLookaheadSamples;
        activeLookaheadSamples = targetLookaheadSamples;
        lookaheadFadeGain = 0.0f;

        for (auto& minimum : gainMinima)
            minimum.setWindowLength (activeLookaheadSamples + 1);
    }

    const bool lookaheadEngaged = activeLookaheadSamples > 0 || previousLookaheadSamples > 0;
    const bool fadingLookahead = lookaheadFadeGain < 1.0f;

    float lastGainReductionDb = 0.0f;

    // Sample-outer / channel-inner (v0.2.0 was the other way round): the
    // linked detector needs every channel's envelope for the *same* sample
    // before it can pick a shared gain. Per-channel state is independent, so
    // the reordering is bit-identical when the link is off.
    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        float linkedEnvelope = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = block.getChannelPointer (static_cast<size_t> (channel));
            auto& envelope = envelopeState[static_cast<size_t> (channel)];
            auto& filter = detectorFilters[static_cast<size_t> (channel)];

            const auto inputSample = data[sample];
            const auto bandpassed = filter.processSample (inputSample);

            const auto rectified = bandpassed * bandpassed;
            const auto coeff = rectified > envelope ? attackCoeff : releaseCoeff;
            envelope = static_cast<float> (coeff * envelope + (1.0 - coeff) * rectified);

            linkedEnvelope = juce::jmax (linkedEnvelope, envelope);

            // The detector above runs on the *undelayed* band; the history
            // below is what the output path reads, delayed.
            const auto historyOffset = static_cast<size_t> (channel) * static_cast<size_t> (historyLength);
            inputHistory[historyOffset + static_cast<size_t> (historyWriteIndex)] = inputSample;
            bandHistory[historyOffset + static_cast<size_t> (historyWriteIndex)] = bandpassed;
        }

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = block.getChannelPointer (static_cast<size_t> (channel));
            const auto historyOffset = static_cast<size_t> (channel) * static_cast<size_t> (historyLength);

            auto readHistory = [this, historyOffset] (const std::vector<float>& history, int delay) noexcept
            {
                auto index = historyWriteIndex - delay;
                while (index < 0)
                    index += historyLength;
                return history[historyOffset + static_cast<size_t> (index)];
            };

            const auto delayedInput = lookaheadEngaged ? readHistory (inputHistory, activeLookaheadSamples)
                                                       : data[sample];
            const auto delayedBand = lookaheadEngaged ? readHistory (bandHistory, activeLookaheadSamples)
                                                      : bandHistory[historyOffset + static_cast<size_t> (historyWriteIndex)];

            // Listen mode replaces the output with the detected band
            // regardless of the current DeEss amount/bypass state, so
            // DeEssFreq can be tuned by ear before dialling in reduction.
            // It reads the delayed band so it stays aligned with everything
            // downstream of the reported latency.
            if (listenEnabled)
            {
                data[sample] = delayedBand;
                continue;
            }

            if (bypassed)
            {
                // Bit-exact bypass. With lookahead engaged it is a bit-exact
                // *delayed* bypass, so the reported latency stays truthful.
                if (lookaheadEngaged)
                    data[sample] = delayedInput;

                continue;
            }

            const auto detectorEnvelope = linkEnabled ? linkedEnvelope : envelopeState[static_cast<size_t> (channel)];
            const auto envelopeDb = juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (detectorEnvelope, 1.0e-12f)), -120.0f);
            const auto overshootDb = envelopeDb - thresholdDb;
            const auto reductionDb = computeReductionDb (overshootDb, maxReductionDb);
            const auto instantaneousGain = juce::Decibels::decibelsToGain (-reductionDb);

            // Sliding minimum over the lookahead window: the gain reaches
            // its target before the delayed ess arrives instead of chasing
            // it. At zero lookahead the window is one sample and this
            // returns its argument unchanged.
            const auto gainFactor = gainMinima[static_cast<size_t> (channel)].process (instantaneousGain);

            auto output = delayedInput + delayedBand * (gainFactor - 1.0f);

            if (fadingLookahead)
            {
                // Both read positions come from the same ring buffer, so the
                // crossfade costs one extra pair of reads while it lasts.
                const auto oldInput = readHistory (inputHistory, previousLookaheadSamples);
                const auto oldBand = readHistory (bandHistory, previousLookaheadSamples);
                const auto oldOutput = oldInput + oldBand * (gainFactor - 1.0f);
                output = oldOutput * (1.0f - lookaheadFadeGain) + output * lookaheadFadeGain;
            }

            data[sample] = output;
            lastGainReductionDb = reductionDb;
        }

        if (fadingLookahead)
            lookaheadFadeGain = juce::jmin (1.0f, lookaheadFadeGain + lookaheadFadeIncrement);

        historyWriteIndex = historyWriteIndex + 1 < historyLength ? historyWriteIndex + 1 : 0;
    }

    if (lookaheadFadeGain >= 1.0f)
        previousLookaheadSamples = activeLookaheadSamples;

    if (! bypassed)
        currentGainReductionDb = lastGainReductionDb;
    else
        currentGainReductionDb = 0.0f;
}
