#include "SeraphEngine.h"

SeraphEngine::SeraphEngine() = default;

void SeraphEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    deEsser.prepare (spec);

    airShelf.prepare (spec);
    *airShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, airFrequencyHz, airShelfQ, juce::Decibels::decibelsToGain (lastAirDb));

    doubler.prepare (spec);

    outputGain.setRampDurationSeconds (smoothingTimeSeconds);
    outputGain.prepare (spec);

    dryBuffer.setSize (static_cast<int> (spec.numChannels), static_cast<int> (spec.maximumBlockSize), false, false, true);

    airDbSmoothed.reset (sampleRate, smoothingTimeSeconds);
    airDbSmoothed.setCurrentAndTargetValue (lastAirDb);
    mixSmoothed.reset (sampleRate, smoothingTimeSeconds);
    mixSmoothed.setCurrentAndTargetValue (lastMixProportion);

    reset();
}

void SeraphEngine::reset()
{
    deEsser.reset();
    airShelf.reset();
    doubler.reset();
    outputGain.reset();
}

void SeraphEngine::setDeEssAmountProportion (float newAmount01)
{
    deEsser.setAmountProportion (newAmount01);
}

void SeraphEngine::setDeEssFrequencyHz (float newFrequencyHz)
{
    deEsser.setFrequencyHz (newFrequencyHz);
}

void SeraphEngine::setAirDb (float newAirDb)
{
    lastAirDb = newAirDb;
    airDbSmoothed.setTargetValue (newAirDb);
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
    const auto wetMix = juce::jlimit (0.0f, 1.0f, mixSmoothed.skip (static_cast<int> (numSamples)));

    *airShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, airFrequencyHz, airShelfQ, juce::Decibels::decibelsToGain (airDb));

    // Capture the true dry signal before any processing touches `block`, for
    // the final Mix crossfade below. Bounded by dryBuffer's prepare()-time
    // capacity - see the class comment on oversized blocks.
    const auto dryChannels = static_cast<size_t> (juce::jmin (static_cast<int> (numChannels), dryBuffer.getNumChannels()));
    const auto drySamples = juce::jmin (static_cast<int> (numSamples), dryBuffer.getNumSamples());

    for (size_t channel = 0; channel < dryChannels; ++channel)
        dryBuffer.copyFrom (static_cast<int> (channel), 0, block.getChannelPointer (channel), drySamples);

    deEsser.process (block);

    juce::dsp::ProcessContextReplacing<float> context (block);
    airShelf.process (context);

    doubler.process (block);

    outputGain.process (context);

    // Final dry/wet crossfade. At wetMix == 1 (Mix default, 100%) this is a
    // no-op multiply-by-1/add-0 pass; at wetMix == 0 the output is the exact
    // dry capture above.
    for (size_t channel = 0; channel < dryChannels; ++channel)
    {
        auto* wetData = block.getChannelPointer (channel);
        const auto* dryData = dryBuffer.getReadPointer (static_cast<int> (channel));

        for (int sample = 0; sample < drySamples; ++sample)
            wetData[sample] = dryData[sample] * (1.0f - wetMix) + wetData[sample] * wetMix;
    }
}
