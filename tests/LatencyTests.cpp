#include "PluginProcessor.h"
#include "dsp/SeraphEngine.h"

#include <catch2/catch_test_macros.hpp>

// Seraph reports 0 latency: unlike an oversampled processor, nothing in the
// chain (de-esser, Air shelf, doubler) delays the signal in a way that needs
// host-side plugin delay compensation - the doubler's short delay lines are
// the effect itself. See docs/architecture.md.

TEST_CASE ("getLatencySamples() is 0 before and after prepareToPlay", "[latency]")
{
    SeraphAudioProcessor processor;
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (48000.0, 512);
    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("SeraphEngine::getLatencySamples() is always 0", "[latency]")
{
    SeraphEngine engine;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 96000.0;
    spec.maximumBlockSize = 256;
    spec.numChannels = 2;
    engine.prepare (spec);

    CHECK (engine.getLatencySamples() == 0);
}

TEST_CASE ("Latency stays 0 regardless of parameter settings", "[latency]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    auto setMax = [&] (const char* id)
    {
        auto* param = processor.apvts.getParameter (id);
        param->setValueNotifyingHost (1.0f);
    };

    setMax ("deEss");
    setMax ("air");
    setMax ("double");
    setMax ("doubleDetune");
    setMax ("doubleWidth");

    CHECK (processor.getLatencySamples() == 0);
}
