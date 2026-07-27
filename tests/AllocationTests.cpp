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
    setParam (processor, ParamIDs::deEssWidth, 75.0f);
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

//==============================================================================
// v0.3.0 real-time safety (SOTA DSP brief ss6.16).
//
// The Shift mode is the reason this section exists: it wraps a third-party
// STFT engine, and whether that engine allocates inside its own process() was
// an open risk in the brief - explicitly a merge gate, with "ship Micro only
// and defer Shift" declared up front as the fallback if it turned out dirty.
// It is not: the wrapper pre-warms the engine in prepare(), and the guard
// below covers steady-state processing, a live mode switch, and parameter
// movement.

TEST_CASE ("Shift-mode processing allocates no memory, including across a mode switch",
           "[dsp][rt-safety][alloc][shift]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 70.0f);
    setParam (processor, ParamIDs::deEssKnee, 6.0f);
    setParam (processor, ParamIDs::deEssLookahead, 2.0f);
    setParam (processor, ParamIDs::deEssLink, 1.0f);
    setParam (processor, ParamIDs::comp, 60.0f);
    setParam (processor, ParamIDs::compLink, 1.0f);
    setParam (processor, ParamIDs::air, 6.0f);
    setParam (processor, ParamIDs::doubleAmount, 80.0f);
    setParam (processor, ParamIDs::doubleDetune, 20.0f);
    setParam (processor, ParamIDs::doubleHumanize, 40.0f);

    auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
    REQUIRE (mode != nullptr);

    auto setMode = [&mode] (int index)
    { mode->setValueNotifyingHost (mode->convertTo0to1 (static_cast<float> (index))); };

    setMode (2); // Shift

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    // Warm-up outside the guard: prepareToPlay() and the first blocks after a
    // parameter change are allowed to allocate.
    for (int warmup = 0; warmup < 16; ++warmup)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f, static_cast<juce::int64> (warmup) * 512);
        processor.processBlock (buffer, midi);
    }

    TestAlloc::AllocationGuard guard;

    for (int block = 0; block < 64; ++block)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f, static_cast<juce::int64> (block) * 512);

        // Move an automatable parameter every block, and switch modes twice
        // mid-run so the engine reset that a switch triggers is inside the
        // guard too.
        setParam (processor, ParamIDs::doubleDetune, 5.0f + static_cast<float> (block % 40));

        if (block == 20)
            setMode (1); // -> Micro

        if (block == 40)
            setMode (2); // -> Shift

        processor.processBlock (buffer, midi);
    }

    CHECK (guard.count() == 0);
}

TEST_CASE ("Micro-mode processing allocates no memory", "[dsp][rt-safety][alloc][micro]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::doubleAmount, 90.0f);
    setParam (processor, ParamIDs::doubleDetune, 30.0f);
    setParam (processor, ParamIDs::doubleHumanize, 70.0f);

    auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
    REQUIRE (mode != nullptr);
    mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int warmup = 0; warmup < 8; ++warmup)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 440.0, 0.5f, static_cast<juce::int64> (warmup) * 512);
        processor.processBlock (buffer, midi);
    }

    TestAlloc::AllocationGuard guard;

    for (int block = 0; block < 32; ++block)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 440.0, 0.5f, static_cast<juce::int64> (block) * 512);
        processor.processBlock (buffer, midi);
    }

    CHECK (guard.count() == 0);
}

TEST_CASE ("De-esser lookahead changes allocate no memory", "[dsp][rt-safety][alloc][deesser][lookahead]")
{
    // The lookahead length is a read offset into a ring buffer sized once in
    // prepare(), precisely so that changing it cannot reallocate.
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 80.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int warmup = 0; warmup < 8; ++warmup)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 7000.0, 0.5f, static_cast<juce::int64> (warmup) * 512);
        processor.processBlock (buffer, midi);
    }

    TestAlloc::AllocationGuard guard;

    for (int block = 0; block < 32; ++block)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 7000.0, 0.5f, static_cast<juce::int64> (block) * 512);
        setParam (processor, ParamIDs::deEssLookahead, (block % 2 == 0) ? 0.0f : 2.0f);
        processor.processBlock (buffer, midi);
    }

    CHECK (guard.count() == 0);
}

TEST_CASE ("Shift mode processes a second of stereo audio well inside its CPU budget",
           "[dsp][rt-safety][cpu][shift]")
{
    // Brief ss3.2 budgets four Shift voices at no more than 8% of one
    // Apple-Silicon core, and ss6.16 asks for one second of 48 kHz stereo in
    // under 100 ms wall clock in a Release build.
    //
    // A Debug build is several times slower than Release, and CI machines are
    // shared, so the assertion below is deliberately loose in Debug and only
    // tightens to the brief's figure when optimisations are on. The measured
    // time is always reported, so a regression is visible in the log even
    // where it is not fatal.
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 70.0f);
    setParam (processor, ParamIDs::deEssLookahead, 2.0f);
    setParam (processor, ParamIDs::comp, 60.0f);
    setParam (processor, ParamIDs::doubleAmount, 80.0f);
    setParam (processor, ParamIDs::doubleDetune, 20.0f);
    setParam (processor, ParamIDs::doubleHumanize, 40.0f);

    auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
    REQUIRE (mode != nullptr);
    mode->setValueNotifyingHost (mode->convertTo0to1 (2.0f));

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int warmup = 0; warmup < 16; ++warmup)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 440.0, 0.5f, static_cast<juce::int64> (warmup) * 512);
        processor.processBlock (buffer, midi);
    }

    constexpr int blocksPerSecond = 48000 / 512;

    const auto startTicks = juce::Time::getHighResolutionTicks();

    for (int block = 0; block < blocksPerSecond; ++block)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 440.0, 0.5f, static_cast<juce::int64> (block) * 512);
        processor.processBlock (buffer, midi);
    }

    const auto elapsedMs = 1000.0 * juce::Time::highResolutionTicksToSeconds (
                               juce::Time::getHighResolutionTicks() - startTicks);

    INFO ("one second of 48 kHz stereo Shift-mode audio took " << elapsedMs << " ms ("
          << (elapsedMs / 10.0) << "% of real time)");

   #if defined(NDEBUG)
    CHECK (elapsedMs < 100.0);
   #else
    // Debug: no optimisation, plus JUCE's own assertions in every inner loop.
    CHECK (elapsedMs < 1000.0);
   #endif
}
