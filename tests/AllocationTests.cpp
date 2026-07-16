#include "AllocationGuard.h"
#include "PluginProcessor.h"
#include "dsp/DeEsser.h"
#include "dsp/SeraphEngine.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

// Permanent audio-thread allocation regression guard (basilica-audio/Seraph
// issue #14): none of pluginval (--strictness-level 10), auval (-strict), or
// the other 28 Catch2 tests do allocation-instrumented profiling, so a
// process()-time heap allocation - such as the ones fixed in issues #12
// (SeraphEngine's Air high-shelf coefficient recompute) and #13 (DeEsser's
// bandpass detector coefficient recompute) - passes CI clean. This test
// exercises the full plugin with every stage actively engaged and fails if
// processBlock() ever touches the heap again, for these two stages or any
// future one.
namespace
{
    void setParam (SeraphAudioProcessor& processor, const char* id, float realValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }
}

TEST_CASE ("SeraphAudioProcessor::processBlock allocates no memory with every stage active", "[dsp][rt-safety][alloc]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // Every stage engaged simultaneously, so a regression in any one of
    // them (not just Air/DeEsser) shows up here.
    setParam (processor, ParamIDs::deEss, 70.0f);
    setParam (processor, ParamIDs::deEssFreq, 7500.0f);
    setParam (processor, ParamIDs::comp, 60.0f);
    setParam (processor, ParamIDs::air, 6.0f);
    setParam (processor, ParamIDs::doubleAmount, 80.0f);
    setParam (processor, ParamIDs::doubleDetune, 20.0f);
    setParam (processor, ParamIDs::doubleWidth, 100.0f);
    setParam (processor, ParamIDs::output, 3.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    // Allocation during prepareToPlay()/parameter smoothing settle is
    // expected and allowed - only the steady-state per-block behaviour
    // below is guarded.
    for (int warmup = 0; warmup < 4; ++warmup)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f, static_cast<juce::int64> (warmup) * 512);
        processor.processBlock (buffer, midi);
    }

    TestAlloc::AllocationGuard guard;

    for (int block = 0; block < 32; ++block)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f, static_cast<juce::int64> (block) * 512);
        processor.processBlock (buffer, midi);
    }

    CHECK (guard.count() == 0);
}

TEST_CASE ("DeEsser::process allocates no memory across repeated blocks", "[dsp][deesser][rt-safety][alloc]")
{
    // Isolated from SeraphEngine/PluginProcessor so this attributes any
    // regression specifically to DeEsser's detector coefficient recompute
    // (basilica-audio/Seraph issue #13), independent of any other stage.
    DeEsser deEsser;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    deEsser.prepare (spec);

    deEsser.setAmountProportion (0.7f);
    deEsser.setFrequencyHz (7500.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    TestHelpers::fillWithSine (buffer, 48000.0, 7500.0, 0.5f);

    juce::dsp::AudioBlock<float> block (buffer);

    // Warm-up block outside the guard, as above.
    deEsser.process (block);

    TestAlloc::AllocationGuard guard;

    for (int i = 0; i < 32; ++i)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 7500.0, 0.5f, static_cast<juce::int64> (i) * 512);
        deEsser.process (block);
    }

    CHECK (guard.count() == 0);
}

TEST_CASE ("SeraphEngine::process allocates no memory with Air active", "[dsp][engine][air][rt-safety][alloc]")
{
    // DeEss/Comp/Double left at zero so any regression here attributes
    // specifically to the Air high-shelf coefficient recompute
    // (basilica-audio/Seraph issue #12), not another stage.
    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.0f);
    engine.setCompAmountProportion (0.0f);
    engine.setDoubleAmountProportion (0.0f);
    engine.setAirDb (6.0f);
    engine.setMixProportion (1.0f);
    engine.setOutputDb (0.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer (2, 512);
    TestHelpers::fillWithSine (buffer, 48000.0, 13000.0, 0.3f);

    juce::dsp::AudioBlock<float> block (buffer);

    // Warm-up block outside the guard, as above.
    engine.process (block);

    TestAlloc::AllocationGuard guard;

    for (int i = 0; i < 32; ++i)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 13000.0, 0.3f, static_cast<juce::int64> (i) * 512);
        engine.process (block);
    }

    CHECK (guard.count() == 0);
}
