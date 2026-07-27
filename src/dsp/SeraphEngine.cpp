#include "SeraphEngine.h"
#include "RealtimeCoefficients.h"

SeraphEngine::SeraphEngine() = default;

float SeraphEngine::getAirDesignFrequencyHz() const noexcept
{
    const auto choice = airFrequencyChoicesHz[static_cast<size_t> (juce::jlimit (0, 2, airFrequencyChoice))];
    return juce::jmin (choice, static_cast<float> (sampleRate) * 0.45f);
}

void SeraphEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    deEsser.prepare (spec);

    airShelf.prepare (spec);
    *airShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, getAirDesignFrequencyHz(), airShelfQ, juce::Decibels::decibelsToGain (lastAirDb));

    compressor.prepare (spec);

    doubler.prepare (spec);

    outputGain.setRampDurationSeconds (smoothingTimeSeconds);
    outputGain.prepare (spec);

    dryBuffer.setSize (static_cast<int> (spec.numChannels), static_cast<int> (spec.maximumBlockSize), false, false, true);

    // Worst case across every mode and lookahead setting, so switching modes
    // on the audio thread never needs a longer line than was allocated.
    const auto worstCaseLatency = doubler.getMaximumLatencySamples()
                                  + static_cast<int> (std::ceil (DeEsser::maxLookaheadMs * 0.001f * sampleRate));

    dryCompensationDelay.prepare (spec);
    dryCompensationDelay.setMaximumDelayInSamples (worstCaseLatency + 8);

    airDbSmoothed.reset (sampleRate, smoothingTimeSeconds);
    airDbSmoothed.setCurrentAndTargetValue (lastAirDb);
    mixSmoothed.reset (sampleRate, smoothingTimeSeconds);
    mixSmoothed.setCurrentAndTargetValue (lastMixProportion);

    appliedLatencySamples = getLatencySamples();
    dryCompensationDelay.setDelay (static_cast<float> (appliedLatencySamples));

    reset();
}

void SeraphEngine::reset()
{
    deEsser.reset();
    airShelf.reset();
    compressor.reset();
    doubler.reset();
    outputGain.reset();
    dryCompensationDelay.reset();
}

int SeraphEngine::getLatencySamples() const noexcept
{
    return deEsser.getLatencySamples() + doubler.getLatencySamples();
}

void SeraphEngine::setDeEssAmountProportion (float newAmount01)
{
    deEsser.setAmountProportion (newAmount01);
}

void SeraphEngine::setDeEssFrequencyHz (float newFrequencyHz)
{
    deEsser.setFrequencyHz (newFrequencyHz);
}

void SeraphEngine::setDeEssWidthProportion (float newWidth01)
{
    deEsser.setWidthProportion (newWidth01);
}

void SeraphEngine::setDeEssListenEnabled (bool shouldListen)
{
    deEsser.setListenEnabled (shouldListen);
}

void SeraphEngine::setDeEssLinkEnabled (bool shouldLink)
{
    deEsser.setLinkEnabled (shouldLink);
}

void SeraphEngine::setDeEssKneeDb (float newKneeDb)
{
    deEsser.setKneeWidthDb (newKneeDb);
}

void SeraphEngine::setDeEssLookaheadMs (float newLookaheadMs)
{
    deEsser.setLookaheadMs (newLookaheadMs);
}

void SeraphEngine::setAirDb (float newAirDb)
{
    lastAirDb = newAirDb;
    airDbSmoothed.setTargetValue (newAirDb);
}

void SeraphEngine::setAirFrequencyChoice (int choiceIndex)
{
    airFrequencyChoice = juce::jlimit (0, 2, choiceIndex);
}

void SeraphEngine::setCompAmountProportion (float newAmount01)
{
    compressor.setAmountProportion (newAmount01);
}

void SeraphEngine::setCompLinkEnabled (bool shouldLink)
{
    compressor.setLinkEnabled (shouldLink);
}

void SeraphEngine::setDoubleAmountProportion (float newAmount01)
{
    doubler.setAmountProportion (newAmount01);
}

void SeraphEngine::setDoubleDetuneCents (float newDetuneCents)
{
    doubler.setDetuneCents (newDetuneCents);
}

void SeraphEngine::setDoubleWidthProportion (float newWidth01)
{
    doubler.setWidthProportion (newWidth01);
}

void SeraphEngine::setDoubleMode (Doubler::Mode newMode)
{
    doubler.setMode (newMode);
}

void SeraphEngine::setDoubleHumanizeProportion (float newHumanize01)
{
    doubler.setHumanizeProportion (newHumanize01);
}

void SeraphEngine::setDoubleFormantPreserveEnabled (bool shouldPreserve)
{
    doubler.setFormantPreserveEnabled (shouldPreserve);
}

void SeraphEngine::setMixProportion (float newProportion01)
{
    lastMixProportion = newProportion01;
    mixSmoothed.setTargetValue (newProportion01);
}

void SeraphEngine::setOutputDb (float newOutputDb)
{
    outputGain.setGainDecibels (newOutputDb);
}

void SeraphEngine::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0)
        return;

    const auto airDb = airDbSmoothed.skip (static_cast<int> (numSamples));

    // Real-time-safe recompute: ArrayCoefficients::makeHighShelf returns a
    // stack std::array (no allocation), written in place into the already-
    // allocated airShelf.state object below - unlike
    // Coefficients<float>::makeHighShelf (used once in prepare() above),
    // which heap-allocates a brand new Coefficients object on every call.
    // See RealtimeCoefficients.h and basilica-audio/Seraph issue #12.
    //
    // Deliberately once per block, not once per slice: see the class comment.
    const auto rawAirShelfCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
        sampleRate, getAirDesignFrequencyHz(), airShelfQ, juce::Decibels::decibelsToGain (airDb));
    srph::applyBiquadCoefficients (*airShelf.state, rawAirShelfCoefficients);

    // Capture the true dry signal before any processing touches `block`, for
    // the final Mix crossfade below. Bounded by dryBuffer's prepare()-time
    // capacity - see the class comment on oversized blocks.
    const auto dryChannels = static_cast<size_t> (juce::jmin (static_cast<int> (numChannels), dryBuffer.getNumChannels()));
    const auto drySamples = juce::jmin (static_cast<int> (numSamples), dryBuffer.getNumSamples());

    for (size_t channel = 0; channel < dryChannels; ++channel)
        dryBuffer.copyFrom (static_cast<int> (channel), 0, block.getChannelPointer (channel), drySamples);

    // Keep the dry compensation delay in step with whatever the chain now
    // reports. Both parameters that can move this are non-automatable, so
    // this changes at most once per user action, never per block.
    const auto latencySamples = getLatencySamples();

    if (latencySamples != appliedLatencySamples)
    {
        appliedLatencySamples = latencySamples;
        dryCompensationDelay.setDelay (static_cast<float> (appliedLatencySamples));
    }

    const auto compensating = appliedLatencySamples > 0;

    for (int offset = 0; offset < static_cast<int> (numSamples); offset += parameterSliceSamples)
    {
        const auto sliceLength = juce::jmin (parameterSliceSamples, static_cast<int> (numSamples) - offset);

        auto slice = block.getSubBlock (static_cast<size_t> (offset), static_cast<size_t> (sliceLength));

        deEsser.process (slice);

        juce::dsp::ProcessContextReplacing<float> context (slice);
        airShelf.process (context);

        compressor.process (slice);

        doubler.process (slice);

        outputGain.process (context);

        const auto wetMix = juce::jlimit (0.0f, 1.0f, mixSmoothed.skip (sliceLength));

        // Final dry/wet crossfade. At wetMix == 1 (Mix default, 100%) this is
        // a no-op multiply-by-1/add-0 pass; at wetMix == 0 the output is the
        // exact dry capture above, delayed by the reported latency.
        //
        // The compensation delay is pushed and popped on every sample even
        // when the chain reports no latency, so that switching into a
        // latency-reporting mode finds a warm delay line rather than a
        // buffer of silence. Its output is only *used* when there is latency
        // to compensate, which keeps the zero-latency dry path bit-exact.
        for (size_t channel = 0; channel < dryChannels; ++channel)
        {
            auto* wetData = block.getChannelPointer (channel);
            const auto* dryData = dryBuffer.getReadPointer (static_cast<int> (channel));

            const auto sliceSamples = juce::jmin (sliceLength, drySamples - offset);

            for (int sample = 0; sample < sliceSamples; ++sample)
            {
                const auto index = offset + sample;

                dryCompensationDelay.pushSample (static_cast<int> (channel), dryData[index]);
                const auto delayed = dryCompensationDelay.popSample (static_cast<int> (channel));

                const auto dry = compensating ? delayed : dryData[index];

                wetData[index] = dry * (1.0f - wetMix) + wetData[index] * wetMix;
            }
        }
    }
}
