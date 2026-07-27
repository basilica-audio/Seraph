#include "PluginProcessor.h"
#include "dsp/SeraphEngine.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <vector>

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 8192;

    juce::dsp::ProcessSpec makeTestSpec (int numChannels)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = testSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (testBlockSize);
        spec.numChannels = static_cast<juce::uint32> (numChannels);
        return spec;
    }
}

TEST_CASE ("Engine null test: neutral settings null against the input", "[dsp][engine][null]")
{
    SeraphEngine engine;

    engine.setDeEssAmountProportion (0.0f);
    engine.setDeEssFrequencyHz (7000.0f);
    engine.setDeEssWidthProportion (0.4f);
    engine.setDeEssListenEnabled (false);
    engine.setAirDb (0.0f);
    engine.setCompAmountProportion (0.0f);
    engine.setDoubleAmountProportion (0.0f);
    engine.setDoubleDetuneCents (10.0f);
    engine.setDoubleWidthProportion (1.0f);
    engine.setMixProportion (1.0f); // fully wet - the "wet" chain itself must equal the input here
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    REQUIRE (engine.getLatencySamples() == 0);

    juce::AudioBuffer<float> reference (2, testBlockSize);
    TestHelpers::fillWithSine (reference, testSampleRate, 1000.0, 0.5f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);

    juce::dsp::AudioBlock<float> block (processed);
    engine.process (block);

    // < -90 dBFS residual, in linear amplitude.
    constexpr float tolerance = 3.1623e-5f; // 10^(-90/20)

    for (int channel = 0; channel < reference.getNumChannels(); ++channel)
    {
        const auto* refData = reference.getReadPointer (channel);
        const auto* outData = processed.getReadPointer (channel);

        float maxResidual = 0.0f;

        for (int i = 0; i < testBlockSize; ++i)
            maxResidual = std::max (maxResidual, std::abs (outData[i] - refData[i]));

        CHECK (maxResidual < tolerance);
    }
}

TEST_CASE ("De-esser reduces energy of a sibilant-like input", "[dsp][engine][deesser]")
{
    constexpr float sibilantFrequencyHz = 7000.0f;

    juce::AudioBuffer<float> sibilantInput (2, testBlockSize);
    TestHelpers::fillWithSine (sibilantInput, testSampleRate, sibilantFrequencyHz, 0.5f);

    double rmsWithoutDeEss = 0.0;
    double rmsWithDeEss = 0.0;

    for (const bool deEssEnabled : { false, true })
    {
        SeraphEngine engine;
        engine.setDeEssAmountProportion (deEssEnabled ? 1.0f : 0.0f);
        engine.setDeEssFrequencyHz (sibilantFrequencyHz);
        engine.setAirDb (0.0f);
        engine.setDoubleAmountProportion (0.0f);
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        const auto spec = makeTestSpec (2);
        engine.prepare (spec);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (sibilantInput);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        REQUIRE (TestHelpers::allSamplesFinite (processed));

        (deEssEnabled ? rmsWithDeEss : rmsWithoutDeEss) = TestHelpers::rms (processed);
    }

    CHECK (rmsWithDeEss < rmsWithoutDeEss * 0.7);
}

TEST_CASE ("Air high-shelf boosts high-frequency content", "[dsp][engine][air]")
{
    constexpr float highFrequencyHz = 13000.0f; // within the ~10-16 kHz Air register

    juce::AudioBuffer<float> highFreqInput (2, testBlockSize);
    TestHelpers::fillWithSine (highFreqInput, testSampleRate, highFrequencyHz, 0.1f);

    double rmsNeutral = 0.0;
    double rmsBoosted = 0.0;

    for (const bool boosted : { false, true })
    {
        SeraphEngine engine;
        engine.setDeEssAmountProportion (0.0f);
        engine.setAirDb (boosted ? 9.0f : 0.0f); // v0.2.0: Air's new max is +9 dB (was +12)
        engine.setDoubleAmountProportion (0.0f);
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        const auto spec = makeTestSpec (2);
        engine.prepare (spec);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (highFreqInput);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        REQUIRE (TestHelpers::allSamplesFinite (processed));

        (boosted ? rmsBoosted : rmsNeutral) = TestHelpers::rms (processed);
    }

    // v0.2.0's wider, gentler shelf (Q ~0.5 vs the old Butterworth ~0.707)
    // spreads the same dB setting's gain across a wider transition band, so
    // this uses a looser (but still clearly discriminating) margin than the
    // old fixed-Q shelf did.
    CHECK (rmsBoosted > rmsNeutral * 1.2);
}

TEST_CASE ("GentleCompressor reduces RMS level of a loud sustained signal", "[dsp][engine][compressor]")
{
    // A steady loud sine, well above the compressor's threshold even at a
    // modest amount - measured over the whole (170 ms @ 48 kHz) block, which
    // is long enough for the 15 ms attack to settle and for reduction to
    // dominate the aggregate RMS, unlike a raw peak comparison which would
    // be dominated by the very first (pre-attack-settled) cycle.
    juce::AudioBuffer<float> loudInput (2, testBlockSize);
    TestHelpers::fillWithSine (loudInput, testSampleRate, 500.0, 0.95f);

    double rmsUncompressed = 0.0;
    double rmsCompressed = 0.0;

    for (const bool compEnabled : { false, true })
    {
        SeraphEngine engine;
        engine.setDeEssAmountProportion (0.0f);
        engine.setAirDb (0.0f);
        engine.setCompAmountProportion (compEnabled ? 1.0f : 0.0f);
        engine.setDoubleAmountProportion (0.0f);
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        const auto spec = makeTestSpec (2);
        engine.prepare (spec);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (loudInput);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        REQUIRE (TestHelpers::allSamplesFinite (processed));

        (compEnabled ? rmsCompressed : rmsUncompressed) = TestHelpers::rms (processed);
    }

    CHECK (rmsCompressed < rmsUncompressed * 0.9);
}

TEST_CASE ("GentleCompressor at 0% amount is a bit-exact bypass", "[dsp][engine][compressor][null]")
{
    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.0f);
    engine.setAirDb (0.0f);
    engine.setCompAmountProportion (0.0f);
    engine.setDoubleAmountProportion (0.0f);
    engine.setMixProportion (1.0f);
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> reference (2, testBlockSize);
    TestHelpers::fillWithSine (reference, testSampleRate, 500.0, 0.9f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);

    juce::dsp::AudioBlock<float> block (processed);
    engine.process (block);

    constexpr float tolerance = 3.1623e-5f; // 10^(-90/20)

    for (int channel = 0; channel < reference.getNumChannels(); ++channel)
    {
        const auto* refData = reference.getReadPointer (channel);
        const auto* outData = processed.getReadPointer (channel);

        float maxResidual = 0.0f;

        for (int i = 0; i < testBlockSize; ++i)
            maxResidual = std::max (maxResidual, std::abs (outData[i] - refData[i]));

        CHECK (maxResidual < tolerance);
    }
}

TEST_CASE ("De-Ess Listen mode replaces output with the detected sibilance band", "[dsp][engine][deesser][listen]")
{
    constexpr float sibilantFrequencyHz = 7000.0f;

    juce::AudioBuffer<float> sibilantInput (2, testBlockSize);
    TestHelpers::fillWithSine (sibilantInput, testSampleRate, sibilantFrequencyHz, 0.5f);

    SeraphEngine engineNormal;
    engineNormal.setDeEssAmountProportion (0.0f);
    engineNormal.setDeEssListenEnabled (false);
    engineNormal.setDoubleAmountProportion (0.0f);
    engineNormal.setMixProportion (1.0f);

    SeraphEngine engineListen;
    engineListen.setDeEssAmountProportion (0.0f);
    engineListen.setDeEssListenEnabled (true);
    engineListen.setDoubleAmountProportion (0.0f);
    engineListen.setMixProportion (1.0f);

    const auto spec = makeTestSpec (2);
    engineNormal.prepare (spec);
    engineListen.prepare (spec);

    juce::AudioBuffer<float> normalOutput;
    normalOutput.makeCopyOf (sibilantInput);
    juce::dsp::AudioBlock<float> normalBlock (normalOutput);
    engineNormal.process (normalBlock);

    juce::AudioBuffer<float> listenOutput;
    listenOutput.makeCopyOf (sibilantInput);
    juce::dsp::AudioBlock<float> listenBlock (listenOutput);
    engineListen.process (listenBlock);

    REQUIRE (TestHelpers::allSamplesFinite (listenOutput));

    // DeEss amount is 0% (no reduction) on both engines, so "normal" mode is
    // a bit-exact bypass and equals the dry input; listen mode must still
    // differ audibly from the dry input, since it routes the detected band
    // to the output regardless of the reduction amount.
    bool differsFromDry = false;

    for (int channel = 0; channel < listenOutput.getNumChannels() && ! differsFromDry; ++channel)
    {
        const auto* dry = sibilantInput.getReadPointer (channel);
        const auto* listen = listenOutput.getReadPointer (channel);

        for (int i = 0; i < testBlockSize; ++i)
        {
            if (std::abs (dry[i] - listen[i]) > 1.0e-4f)
            {
                differsFromDry = true;
                break;
            }
        }
    }

    CHECK (differsFromDry);
}

TEST_CASE ("Doubler's four voices audibly change the signal and widen the stereo image at full width", "[dsp][engine][doubler]")
{
    juce::AudioBuffer<float> source (2, testBlockSize);
    TestHelpers::fillWithSine (source, testSampleRate, 300.0, 0.4f);

    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.0f);
    engine.setAirDb (0.0f);
    engine.setDoubleAmountProportion (1.0f);
    engine.setDoubleDetuneCents (15.0f);
    engine.setDoubleWidthProportion (1.0f);
    engine.setMixProportion (1.0f);
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer;
    buffer.makeCopyOf (source);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    REQUIRE (TestHelpers::allSamplesFinite (buffer));

    // Four voices summed on top of the source (some of them phase-shifted
    // by their base delay relative to a 300 Hz source) should measurably
    // change the aggregate level in at least one direction - i.e. the
    // doubler must not be a silent no-op at Double == 100%. Interference
    // between the delayed voices and the source can just as validly reduce
    // RMS as increase it depending on their relative phase at this
    // frequency, so this only checks that the level actually moved.
    const auto rmsSource = TestHelpers::rms (source);
    const auto rmsDoubled = TestHelpers::rms (buffer);
    CHECK (std::abs (rmsDoubled - rmsSource) > rmsSource * 0.05);

    // Per-voice pan spreads the four voices unevenly across L/R at full
    // width, so the left and right channels should no longer be identical
    // (unlike the mono-sum source, which was identical in both channels).
    bool channelsDiffer = false;

    for (int i = 0; i < testBlockSize && ! channelsDiffer; ++i)
        if (std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)) > 1.0e-4f)
            channelsDiffer = true;

    CHECK (channelsDiffer);
}

TEST_CASE ("Engine reset() clears filter/delay-line state without crashing", "[dsp][engine]")
{
    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.8f);
    engine.setDoubleAmountProportion (0.8f);
    engine.setMixProportion (1.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer (2, testBlockSize);
    TestHelpers::fillWithSine (buffer, testSampleRate, 1000.0, 0.9f);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    CHECK_NOTHROW (engine.reset());
    CHECK (TestHelpers::allSamplesFinite (buffer));

    TestHelpers::fillWithSine (buffer, testSampleRate, 1000.0, 0.9f);
    CHECK_NOTHROW (engine.process (block));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}

//==============================================================================
// v0.3.0 compatibility and smoothing (SOTA DSP brief ss6.4 / ss6.14).
namespace
{
    // A deterministic, spectrally rich stimulus: pink noise plus a tone, the
    // two channels decorrelated so stereo-dependent code paths are exercised.
    void fillCompatibilityStimulus (juce::AudioBuffer<float>& buffer,
                                    double sampleRate,
                                    juce::int64 startSample,
                                    TestHelpers::DeterministicNoise& noise)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto pink = noise.nextPink();
            const auto phase = juce::MathConstants<double>::twoPi * 440.0
                                * static_cast<double> (startSample + sample) / sampleRate;
            const auto value = pink + 0.25f * static_cast<float> (std::sin (phase));

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, sample, channel == 0 ? value : value * 0.8f);
        }
    }

    // Renders `seconds` of that stimulus through a processor and returns the
    // interleaved output.
    std::vector<float> renderCompatibility (SeraphAudioProcessor& processor,
                                            double sampleRate,
                                            int blockSize,
                                            double seconds)
    {
        processor.prepareToPlay (sampleRate, blockSize);

        const auto totalSamples = static_cast<int> (sampleRate * seconds);

        TestHelpers::DeterministicNoise noise;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        std::vector<float> output;
        output.reserve (static_cast<size_t> (totalSamples) * 2);

        for (int position = 0; position + blockSize <= totalSamples; position += blockSize)
        {
            fillCompatibilityStimulus (buffer, sampleRate, position, noise);
            processor.processBlock (buffer, midi);

            for (int sample = 0; sample < blockSize; ++sample)
            {
                output.push_back (buffer.getSample (0, sample));
                output.push_back (buffer.getSample (1, sample));
            }
        }

        return output;
    }

    // A v0.2.0-shaped state: the eleven parameters that existed then, at
    // their v0.2.0 defaults, with no stateVersion attribute and none of the
    // v0.3.0 IDs. This is what an upgraded session actually looks like.
    juce::MemoryBlock makeLegacyDefaultState()
    {
        juce::ValueTree state ("PARAMETERS");

        auto addParam = [&state] (const char* id, float value)
        {
            juce::ValueTree param ("PARAM");
            param.setProperty ("id", id, nullptr);
            param.setProperty ("value", value, nullptr);
            state.appendChild (param, nullptr);
        };

        addParam (ParamIDs::deEss, 30.0f);
        addParam (ParamIDs::deEssFreq, 7000.0f);
        addParam (ParamIDs::deEssWidth, 40.0f);
        addParam (ParamIDs::deEssListen, 0.0f);
        addParam (ParamIDs::air, 2.0f);
        addParam (ParamIDs::comp, 0.0f);
        addParam (ParamIDs::doubleAmount, 25.0f);
        addParam (ParamIDs::doubleDetune, 10.0f);
        addParam (ParamIDs::doubleWidth, 100.0f);
        addParam (ParamIDs::mix, 100.0f);
        addParam (ParamIDs::output, 0.0f);

        const std::unique_ptr<juce::XmlElement> xml (state.createXml());
        juce::MemoryBlock binary;
        juce::AudioProcessor::copyXmlToBinary (*xml, binary);
        return binary;
    }
}

// ss6.4. This is the load-bearing compatibility guarantee of the whole
// release: v0.3.0 rewrote every stage's inner loop (sub-block smoothing) and
// added eight parameters, and a session saved by v0.2.0 still has to render
// the same samples. Run at three sample rates and two block sizes, because
// the sub-block refactor is exactly the kind of change that is bit-identical
// at one block size and not at another.
TEST_CASE ("A v0.2.0 session renders bit-identically after upgrading", "[dsp][engine][null][compatibility]")
{
    const auto legacyState = makeLegacyDefaultState();

    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        for (const auto blockSize : { 64, 1024 })
        {
            SeraphAudioProcessor fresh;
            const auto freshOutput = renderCompatibility (fresh, sampleRate, blockSize, 1.0);

            SeraphAudioProcessor migrated;
            migrated.setStateInformation (legacyState.getData(), static_cast<int> (legacyState.getSize()));
            const auto migratedOutput = renderCompatibility (migrated, sampleRate, blockSize, 1.0);

            INFO ("sample rate " << sampleRate << ", block size " << blockSize);
            REQUIRE (freshOutput.size() == migratedOutput.size());
            REQUIRE (freshOutput.size() > 0);
            CHECK (std::memcmp (freshOutput.data(), migratedOutput.data(),
                                freshOutput.size() * sizeof (float)) == 0);
        }
    }
}

TEST_CASE ("Explicitly setting every v0.3.0 parameter to its default changes nothing", "[dsp][engine][null][compatibility]")
{
    // Guards against a default that is neutral in the layout but not in the
    // engine - a setter that, say, resets state when called.
    SeraphAudioProcessor untouched;
    const auto reference = renderCompatibility (untouched, 48000.0, 256, 0.5);

    SeraphAudioProcessor explicitDefaults;

    for (const char* id : { ParamIDs::doubleMode, ParamIDs::doubleHumanize, ParamIDs::doubleFormant,
                            ParamIDs::deEssLink, ParamIDs::deEssKnee, ParamIDs::deEssLookahead,
                            ParamIDs::airFreq, ParamIDs::compLink })
    {
        auto* param = explicitDefaults.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->getDefaultValue());
    }

    const auto rendered = renderCompatibility (explicitDefaults, 48000.0, 256, 0.5);

    REQUIRE (reference.size() == rendered.size());
    CHECK (std::memcmp (reference.data(), rendered.data(), reference.size() * sizeof (float)) == 0);
}

// ss6.14. Before v0.3.0 every smoothed parameter advanced once per host
// block, so a 4096-sample block moved each one in a single step - a
// block-sized staircase that is audible as zipper on fast automation. The
// engine now slices at SeraphEngine::parameterSliceSamples.
TEST_CASE ("Smoothed parameters advance in sub-block slices, not once per block", "[dsp][engine][smoothing]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 4096;

    // The Mix trajectory is recovered exactly rather than estimated: render
    // the same input at a fixed Mix of 0 and of 1, then for a third render
    // with Mix moving, solve out[n] = dry[n]*(1-m[n]) + wet[n]*m[n] for m[n].
    auto render = [&] (float startMix, float endMix)
    {
        SeraphEngine engine;
        engine.setDeEssAmountProportion (0.0f);
        engine.setCompAmountProportion (0.0f);
        engine.setDoubleAmountProportion (0.0f);
        engine.setAirDb (9.0f); // makes wet differ strongly from dry up top
        engine.setOutputDb (0.0f);
        engine.setMixProportion (startMix);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = blockSize;
        spec.numChannels = 1;
        engine.prepare (spec);

        juce::AudioBuffer<float> buffer (1, blockSize);

        // One settled block first, then the block under test.
        TestHelpers::fillWithSine (buffer, sampleRate, 15000.0, 0.5f);
        juce::dsp::AudioBlock<float> warmup (buffer);
        engine.process (warmup);

        engine.setMixProportion (endMix);

        TestHelpers::fillWithSine (buffer, sampleRate, 15000.0, 0.5f, blockSize);
        juce::dsp::AudioBlock<float> block (buffer);
        engine.process (block);

        std::vector<float> output (static_cast<size_t> (blockSize));

        for (int sample = 0; sample < blockSize; ++sample)
            output[static_cast<size_t> (sample)] = buffer.getSample (0, sample);

        return output;
    };

    const auto dry = render (0.0f, 0.0f);
    const auto wet = render (1.0f, 1.0f);
    const auto ramped = render (0.0f, 1.0f);

    // Recover the trajectory only where dry and wet are far enough apart for
    // the division to be well conditioned.
    std::vector<std::pair<int, double>> trajectory;

    for (int sample = 0; sample < blockSize; ++sample)
    {
        const auto difference = static_cast<double> (wet[static_cast<size_t> (sample)])
                                - static_cast<double> (dry[static_cast<size_t> (sample)]);

        if (std::abs (difference) < 0.05)
            continue;

        const auto mix = (static_cast<double> (ramped[static_cast<size_t> (sample)])
                          - static_cast<double> (dry[static_cast<size_t> (sample)])) / difference;

        trajectory.emplace_back (sample, mix);
    }

    REQUIRE (trajectory.size() > 100);

    SECTION ("the trajectory moves within the block instead of jumping once")
    {
        const auto first = trajectory.front().second;
        const auto last = trajectory.back().second;

        INFO ("mix went from " << first << " to " << last << " across the block");
        CHECK (first < 0.2);
        CHECK (last > 0.8);
    }

    SECTION ("no value is held for longer than one slice")
    {
        // The whole point: with once-per-block smoothing the recovered
        // trajectory would be a single constant across all 4096 samples.
        //
        // Only the moving part of the ramp is measured. A 50 ms ramp finishes
        // 2400 samples into a 4096-sample block, and the flat tail afterwards
        // is the parameter having arrived, not the smoothing standing still.
        auto longestRun = 0;
        auto currentRun = 1;

        for (size_t index = 1; index < trajectory.size(); ++index)
        {
            if (trajectory[index].second > 0.98)
                break;

            const auto sameValue = std::abs (trajectory[index].second - trajectory[index - 1].second) < 1.0e-4;
            const auto sampleGap = trajectory[index].first - trajectory[index - 1].first;

            currentRun = sameValue ? currentRun + sampleGap : 1;
            longestRun = std::max (longestRun, currentRun);
        }

        INFO ("longest run of a constant Mix value: " << longestRun << " samples (slice length "
                                                      << SeraphEngine::parameterSliceSamples << ")");
        CHECK (longestRun <= SeraphEngine::parameterSliceSamples + 1);
    }

    SECTION ("and each step is consistent with the documented 50 ms ramp")
    {
        // 32 samples of a 50 ms linear ramp at 48 kHz is 1/75 of the range.
        const auto expectedStep = static_cast<double> (SeraphEngine::parameterSliceSamples) / (0.05 * sampleRate);

        auto largestStep = 0.0;

        for (size_t index = 1; index < trajectory.size(); ++index)
            largestStep = std::max (largestStep, std::abs (trajectory[index].second - trajectory[index - 1].second));

        INFO ("largest step " << largestStep << ", expected at most " << expectedStep);
        CHECK (largestStep <= expectedStep * 1.2);
    }
}

TEST_CASE ("Automating Detune in Micro mode stays continuous", "[dsp][engine][smoothing][micro]")
{
    // The Micro engine recomputes its sweep geometry whenever the detune
    // changes, so a fast automation move is the case most likely to produce a
    // discontinuity. Asserted as a bound on the sample-to-sample slew of the
    // output relative to the input's own.
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int numBlocks = 200;

    Doubler doubler;
    doubler.setMode (Doubler::Mode::micro);
    doubler.setAmountProportion (1.0f);
    doubler.setWidthProportion (1.0f);
    doubler.setDetuneCents (0.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = blockSize;
    spec.numChannels = 2;
    doubler.prepare (spec);

    juce::AudioBuffer<float> buffer (2, blockSize);

    auto largestSlew = 0.0f;
    auto previousSample = 0.0f;

    for (int block = 0; block < numBlocks; ++block)
    {
        // Sweep the detune across its whole range and back, fast.
        const auto position = static_cast<float> (block) / numBlocks;
        doubler.setDetuneCents (50.0f * std::abs (1.0f - 2.0f * position));

        TestHelpers::fillWithSine (buffer, sampleRate, 500.0, 0.5f, static_cast<juce::int64> (block) * blockSize);

        juce::dsp::AudioBlock<float> audioBlock (buffer);
        doubler.process (audioBlock);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = buffer.getSample (0, sample);
            CHECK (std::isfinite (value));

            if (block > 8) // past the initial delay-line fill
                largestSlew = std::max (largestSlew, std::abs (value - previousSample));

            previousSample = value;
        }
    }

    // A 500 Hz sine at 0.5 amplitude slews at most 2*pi*500/48000*0.5 = 0.033
    // per sample; the doubler adds four such voices. A click from a
    // discontinuous delay change would be an order of magnitude beyond this.
    INFO ("largest sample-to-sample slew " << largestSlew);
    CHECK (largestSlew < 0.2f);
}
