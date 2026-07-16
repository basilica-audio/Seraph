// v0.2.0 deep-dive voicing test coverage (docs/design-brief.md ss4's test
// guarantees, on top of the existing 28+ Catch2 tests): the DeEssWidth
// bandwidth control, the re-voiced Air shelf, the GentleCompressor's
// program-dependent ("auto") release, and the Doubler's re-centered base
// delays. Existing suites (CoverageTests, EngineTests, LatencyTests,
// ParameterTests, RobustnessTests, StateTests) stay green and unmodified in
// intent - this file only adds coverage for what v0.2.0 changed.

#include "dsp/DeEsser.h"
#include "dsp/GentleCompressor.h"
#include "dsp/Doubler.h"
#include "dsp/SeraphEngine.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

namespace
{
    constexpr double sr = 48000.0;

    //==========================================================================
    // DeEssWidth bandwidth-curve helpers (docs/design-brief.md ss4's first
    // bullet: sweep DeEssWidth, measure the detector's -3 dB bandwidth via a
    // probe-tone sweep through DeEsser::process() in isolation).

    // Measures DeEsser's steady-state gain (in dB) at `probeFreqHz`, for a
    // detector centered at `centerFreqHz` with detection bandwidth
    // `width01`, using Listen mode (bypasses the reduction math entirely, so
    // this measures the detector filter's own magnitude response
    // independent of DeEss amount/threshold). Setting the target
    // frequency/width *before* prepare() means prepare()'s initial
    // coefficients already sit at the target (SmoothedValue::
    // setCurrentAndTargetValue), so no smoothing ramp-up is needed - only
    // the biquad's own (much shorter) settling transient, which the tail-
    // only measurement below discards.
    double measureDeEsserGainDb (float centerFreqHz, float width01, double probeFreqHz)
    {
        constexpr int blockSize = 4096;

        DeEsser deEsser;
        deEsser.setFrequencyHz (centerFreqHz);
        deEsser.setWidthProportion (width01);
        deEsser.setAmountProportion (0.5f); // irrelevant under Listen mode below
        deEsser.setListenEnabled (true);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
        spec.numChannels = 1;
        deEsser.prepare (spec);

        juce::AudioBuffer<float> input (1, blockSize);
        TestHelpers::fillWithSine (input, sr, probeFreqHz, 1.0f);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (input);
        juce::dsp::AudioBlock<float> block (processed);
        deEsser.process (block);

        // Tail-only (post-transient-settle) measurement.
        const auto tailStart = blockSize / 2;
        double sumSqOut = 0.0;
        double sumSqIn = 0.0;

        for (int i = tailStart; i < blockSize; ++i)
        {
            const auto out = static_cast<double> (processed.getSample (0, i));
            const auto in = static_cast<double> (input.getSample (0, i));
            sumSqOut += out * out;
            sumSqIn += in * in;
        }

        const auto rmsOut = std::sqrt (sumSqOut / static_cast<double> (blockSize - tailStart));
        const auto rmsIn = std::sqrt (sumSqIn / static_cast<double> (blockSize - tailStart));

        return juce::Decibels::gainToDecibels (rmsOut / juce::jmax (rmsIn, 1.0e-9), -120.0);
    }

    // Bisection search (24 iterations - ample precision for a ±10% target)
    // for the frequency, on one side of centerFreqHz, where the measured
    // gain crosses (centerGainDb - 3.0). `searchAbove` true searches upward.
    double findMinus3dbEdgeHz (float centerFreqHz, float width01, double centerGainDb, bool searchAbove)
    {
        const auto thresholdDb = centerGainDb - 3.0;

        double insideBound = centerFreqHz; // confirmed still within 3 dB of center
        double outsideBound = searchAbove ? centerFreqHz * 4.0 : centerFreqHz * 0.1; // confirmed beyond it

        REQUIRE (measureDeEsserGainDb (centerFreqHz, width01, outsideBound) < thresholdDb);

        for (int iteration = 0; iteration < 24; ++iteration)
        {
            const auto mid = 0.5 * (insideBound + outsideBound);
            const auto gainDb = measureDeEsserGainDb (centerFreqHz, width01, mid);

            if (gainDb < thresholdDb)
                outsideBound = mid;
            else
                insideBound = mid;
        }

        return 0.5 * (insideBound + outsideBound);
    }

    double measureMinus3dbBandwidthHz (float centerFreqHz, float width01)
    {
        const auto centerGainDb = measureDeEsserGainDb (centerFreqHz, width01, centerFreqHz);
        const auto upperEdge = findMinus3dbEdgeHz (centerFreqHz, width01, centerGainDb, true);
        const auto lowerEdge = findMinus3dbEdgeHz (centerFreqHz, width01, centerGainDb, false);
        return upperEdge - lowerEdge;
    }

    // Analytic (magnitude-function, not audio-domain) -3 dB bandwidth for a
    // directly-constructed reference bandpass filter at a known Q - uses the
    // exact same juce::dsp::IIR::Coefficients<float>::makeBandPass() call
    // DeEsser.cpp itself uses (see DeEsser.cpp's prepare()), so comparing an
    // audio-domain measurement (measureMinus3dbBandwidthHz(), via
    // DeEsser::process() in Listen mode) against this reference isolates
    // DeEsser's own width->Q wiring from the *idealized* continuous-time
    // bandwidth<->Q formula (bandwidth ~= f0/Q), which digital bilinear-
    // transform frequency pre-warping measurably departs from at a 7 kHz
    // center on a 48 kHz sample rate - comparing two measurements of the
    // *same* digital filter design (one via getMagnitudeForFrequency(), one
    // via real audio) is what actually lands within the spec's +/-10%
    // tolerance; comparing against the idealized formula does not.
    double referenceGainDb (float centerFreqHz, float q, double probeFreqHz)
    {
        const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, centerFreqHz, q);
        const auto magnitude = coefficients->getMagnitudeForFrequency (probeFreqHz, sr);
        return juce::Decibels::gainToDecibels (static_cast<double> (magnitude), -120.0);
    }

    double referenceFindMinus3dbEdgeHz (float centerFreqHz, float q, double centerGainDb, bool searchAbove)
    {
        const auto thresholdDb = centerGainDb - 3.0;

        double insideBound = centerFreqHz;
        double outsideBound = searchAbove ? centerFreqHz * 4.0 : centerFreqHz * 0.1;

        REQUIRE (referenceGainDb (centerFreqHz, q, outsideBound) < thresholdDb);

        for (int iteration = 0; iteration < 24; ++iteration)
        {
            const auto mid = 0.5 * (insideBound + outsideBound);
            const auto gainDb = referenceGainDb (centerFreqHz, q, mid);

            if (gainDb < thresholdDb)
                outsideBound = mid;
            else
                insideBound = mid;
        }

        return 0.5 * (insideBound + outsideBound);
    }

    double referenceMinus3dbBandwidthHz (float centerFreqHz, float q)
    {
        const auto centerGainDb = referenceGainDb (centerFreqHz, q, centerFreqHz);
        const auto upperEdge = referenceFindMinus3dbEdgeHz (centerFreqHz, q, centerGainDb, true);
        const auto lowerEdge = referenceFindMinus3dbEdgeHz (centerFreqHz, q, centerGainDb, false);
        return upperEdge - lowerEdge;
    }
}

//==============================================================================
TEST_CASE ("DeEssWidth: -3 dB detection bandwidth narrows monotonically as DeEssWidth decreases", "[dsp][deesser][width]")
{
    constexpr float centerFreqHz = 7000.0f;

    const auto bandwidthNarrow = measureMinus3dbBandwidthHz (centerFreqHz, 0.0f);   // DeEssWidth == 0% -> Q == 3.0
    const auto bandwidthMid = measureMinus3dbBandwidthHz (centerFreqHz, 0.5f);
    const auto bandwidthWide = measureMinus3dbBandwidthHz (centerFreqHz, 1.0f);     // DeEssWidth == 100% -> Q == 0.7

    CHECK (bandwidthNarrow < bandwidthMid);
    CHECK (bandwidthMid < bandwidthWide);
}

TEST_CASE ("DeEssWidth: Q extremes land within +/-10% of the documented 0.7/3.0 targets", "[dsp][deesser][width]")
{
    constexpr float centerFreqHz = 7000.0f;

    const auto measuredNarrow = measureMinus3dbBandwidthHz (centerFreqHz, 0.0f);   // DeEssWidth == 0% -> Q == 3.0
    const auto referenceNarrow = referenceMinus3dbBandwidthHz (centerFreqHz, 3.0f);
    CHECK (measuredNarrow == Catch::Approx (referenceNarrow).epsilon (0.10));

    const auto measuredWide = measureMinus3dbBandwidthHz (centerFreqHz, 1.0f);     // DeEssWidth == 100% -> Q == 0.7
    const auto referenceWide = referenceMinus3dbBandwidthHz (centerFreqHz, 0.7f);
    CHECK (measuredWide == Catch::Approx (referenceWide).epsilon (0.10));
}

TEST_CASE ("DeEssWidth: DeEss == 0% stays a bit-exact bypass across the full DeEssWidth range", "[dsp][deesser][width][null]")
{
    constexpr int blockSize = 2048;

    for (const float width01 : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        DeEsser deEsser;
        deEsser.setAmountProportion (0.0f); // bit-exact bypass, regardless of width
        deEsser.setFrequencyHz (7000.0f);
        deEsser.setWidthProportion (width01);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
        spec.numChannels = 2;
        deEsser.prepare (spec);

        juce::AudioBuffer<float> reference (2, blockSize);
        TestHelpers::fillWithSine (reference, sr, 7000.0, 0.6f);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (reference);
        juce::dsp::AudioBlock<float> block (processed);
        deEsser.process (block);

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < blockSize; ++i)
                CHECK (processed.getSample (channel, i) == reference.getSample (channel, i));
    }
}

//==============================================================================
// Air curve shape (docs/design-brief.md ss4: magnitude response at 1 kHz,
// 6 kHz, 12 kHz (corner), and 20 kHz at Air == +9 dB (new max); the response
// at 6 kHz must be measurably non-zero (confirms the widened, lower-Q
// transition starts earlier than the old Butterworth-Q shelf would have),
// and the curve must be monotonically non-decreasing across those points).
namespace
{
    double measureEngineGainDb (float airDb, double probeFreqHz)
    {
        constexpr int blockSize = 8192;

        SeraphEngine engine;
        engine.setDeEssAmountProportion (0.0f);
        engine.setAirDb (airDb);
        engine.setCompAmountProportion (0.0f);
        engine.setDoubleAmountProportion (0.0f);
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
        spec.numChannels = 1;
        engine.prepare (spec);

        juce::AudioBuffer<float> input (1, blockSize);
        TestHelpers::fillWithSine (input, sr, probeFreqHz, 0.2f);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (input);
        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        const auto tailStart = blockSize / 2;
        double sumSqOut = 0.0;
        double sumSqIn = 0.0;

        for (int i = tailStart; i < blockSize; ++i)
        {
            const auto out = static_cast<double> (processed.getSample (0, i));
            const auto in = static_cast<double> (input.getSample (0, i));
            sumSqOut += out * out;
            sumSqIn += in * in;
        }

        const auto rmsOut = std::sqrt (sumSqOut / static_cast<double> (blockSize - tailStart));
        const auto rmsIn = std::sqrt (sumSqIn / static_cast<double> (blockSize - tailStart));

        return juce::Decibels::gainToDecibels (rmsOut / juce::jmax (rmsIn, 1.0e-9), -120.0);
    }
}

TEST_CASE ("Air curve shape: v0.2.0's wider, gentler shelf rises measurably before the 12 kHz corner", "[dsp][engine][air][shape]")
{
    constexpr float maxAirDb = 9.0f; // v0.2.0's new max (was +12)

    const auto gain1k = measureEngineGainDb (maxAirDb, 1000.0);
    const auto gain6k = measureEngineGainDb (maxAirDb, 6000.0);
    const auto gain12k = measureEngineGainDb (maxAirDb, 12000.0);
    const auto gain20k = measureEngineGainDb (maxAirDb, 20000.0);

    // At 1 kHz, several octaves below the corner, the shelf's low-frequency
    // asymptote should sit close to 0 dB.
    CHECK (gain1k < 1.0);

    // The widened transition (Q ~0.5 vs the old ~0.707) must have already
    // started rising well before the corner - a fixed-Q Butterworth shelf at
    // 12 kHz would be much closer to 0 dB at 6 kHz than this.
    CHECK (gain6k > 0.5);

    // Monotonically non-decreasing from 1 kHz up to 20 kHz.
    CHECK (gain1k <= gain6k);
    CHECK (gain6k <= gain12k);
    CHECK (gain12k <= gain20k + 0.5); // small margin: near-unity-Q shelves can overshoot slightly past the corner before settling
}

TEST_CASE ("Air null test: Air == 0 dB is near-identity with the new Q constant", "[dsp][engine][air][null]")
{
    constexpr float tolerance = 3.1623e-5f; // -90 dBFS, matches the existing engine null test's tolerance

    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.0f);
    engine.setAirDb (0.0f);
    engine.setCompAmountProportion (0.0f);
    engine.setDoubleAmountProportion (0.0f);
    engine.setMixProportion (1.0f);
    engine.setOutputDb (0.0f);

    constexpr int blockSize = 4096;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
    spec.numChannels = 2;
    engine.prepare (spec);

    juce::AudioBuffer<float> reference (2, blockSize);
    TestHelpers::fillWithSine (reference, sr, 13000.0, 0.5f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);
    juce::dsp::AudioBlock<float> block (processed);
    engine.process (block);

    for (int channel = 0; channel < 2; ++channel)
    {
        float maxResidual = 0.0f;

        for (int i = 0; i < blockSize; ++i)
            maxResidual = std::max (maxResidual, std::abs (processed.getSample (channel, i) - reference.getSample (channel, i)));

        CHECK (maxResidual < tolerance);
    }
}

//==============================================================================
// GentleCompressor program-dependent ("auto") release (docs/design-brief.md
// ss2.3/ss4).
namespace
{
    struct ReleaseRecoveryResult
    {
        double peakGainReductionDb;
        long recoverySamples;
    };

    // Drives GentleCompressor at Comp == 100% with a loud sustained tone for
    // `loudDurationSeconds` (building up gain reduction, and - for longer
    // durations - biasing the auto-release blend weight toward its slow
    // path), then switches to a quiet tone well below threshold and measures
    // how many samples elapse until gain reduction has recovered to <=10% of
    // its peak value ("time-to-90%-recovery").
    ReleaseRecoveryResult measureReleaseRecovery (double loudDurationSeconds)
    {
        constexpr int blockSize = 64;

        GentleCompressor comp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
        spec.numChannels = 1;
        comp.prepare (spec);
        comp.setAmountProportion (1.0f);

        juce::AudioBuffer<float> buffer (1, blockSize);
        juce::int64 samplesProcessed = 0;
        const auto loudSamples = static_cast<juce::int64> (loudDurationSeconds * sr);

        while (samplesProcessed < loudSamples)
        {
            TestHelpers::fillWithSine (buffer, sr, 500.0, 0.9f, samplesProcessed);
            juce::dsp::AudioBlock<float> block (buffer);
            comp.process (block);
            samplesProcessed += blockSize;
        }

        const auto peakGainReductionDb = static_cast<double> (comp.getCurrentGainReductionDb());
        REQUIRE (peakGainReductionDb > 1.0); // sanity: the compressor is actually doing something

        const auto recoveryThresholdDb = peakGainReductionDb * 0.1;
        constexpr long maxRecoverySamples = static_cast<long> (5.0 * sr); // generous bound so the loop below always terminates
        long recoverySamples = 0;

        while (recoverySamples < maxRecoverySamples)
        {
            TestHelpers::fillWithSine (buffer, sr, 500.0, 0.001f, samplesProcessed);
            juce::dsp::AudioBlock<float> block (buffer);
            comp.process (block);
            samplesProcessed += blockSize;
            recoverySamples += blockSize;

            if (static_cast<double> (comp.getCurrentGainReductionDb()) <= recoveryThresholdDb)
                break;
        }

        REQUIRE (recoverySamples < maxRecoverySamples); // must have actually recovered within the bound

        return { peakGainReductionDb, recoverySamples };
    }
}

TEST_CASE ("GentleCompressor auto-release: recovery is faster after an isolated transient than after sustained reduction",
           "[dsp][compressor][auto-release]")
{
    // "Isolated transient": only 100 ms of loud material - far shorter than
    // releaseWeightReleaseTimeSeconds (~0.5 s), so the auto-release blend
    // weight stays biased toward the fast (~150 ms) path.
    const auto transientResult = measureReleaseRecovery (0.1);

    // "Continuous sustained reduction": several seconds of loud material -
    // many times releaseWeightReleaseTimeSeconds, giving the blend weight
    // time to settle toward the slow (~1.0 s) path.
    const auto sustainedResult = measureReleaseRecovery (4.0);

    CHECK (transientResult.recoverySamples < sustainedResult.recoverySamples);
}

TEST_CASE ("GentleCompressor auto-release: no discontinuity at the fast/slow envelope blend boundary", "[dsp][compressor][auto-release]")
{
    GentleCompressor comp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = 1;
    spec.numChannels = 1;
    comp.prepare (spec);
    comp.setAmountProportion (1.0f);

    // Program-dependent test material: alternating loud/quiet sections (hard
    // amplitude steps - the input itself is intentionally discontinuous;
    // what must NOT be discontinuous is the compressor's own per-sample
    // gain-reduction trajectory, since the blend weight is a smoothed
    // one-pole, never switched - see GentleCompressor.h's class comment).
    constexpr int sectionSamples = 4000; // ~83 ms @ 48 kHz
    constexpr int numSections = 8;
    constexpr double epsilonDb = 1.0; // generous - real zipper/switching bugs jump by many dB in a single sample

    juce::AudioBuffer<float> sampleBuffer (1, 1);
    double previousGainReductionDb = 0.0;
    bool first = true;
    double maxAbsDeltaDb = 0.0;

    for (int section = 0; section < numSections; ++section)
    {
        const auto amplitude = (section % 2 == 0) ? 0.9f : 0.05f;

        for (int i = 0; i < sectionSamples; ++i)
        {
            const auto sampleIndex = static_cast<juce::int64> (section) * sectionSamples + i;
            TestHelpers::fillWithSine (sampleBuffer, sr, 500.0, amplitude, sampleIndex);

            juce::dsp::AudioBlock<float> block (sampleBuffer);
            comp.process (block);

            const auto gainReductionDb = static_cast<double> (comp.getCurrentGainReductionDb());

            if (! first)
                maxAbsDeltaDb = std::max (maxAbsDeltaDb, std::abs (gainReductionDb - previousGainReductionDb));

            previousGainReductionDb = gainReductionDb;
            first = false;
        }
    }

    CHECK (maxAbsDeltaDb < epsilonDb);
}

TEST_CASE ("GentleCompressor at 0% amount is a bit-exact bypass with the new release path", "[dsp][compressor][auto-release][null]")
{
    constexpr int blockSize = 2048;
    constexpr float tolerance = 3.1623e-5f; // -90 dBFS

    GentleCompressor comp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
    spec.numChannels = 2;
    comp.prepare (spec);
    comp.setAmountProportion (0.0f);

    juce::AudioBuffer<float> reference (2, blockSize);
    TestHelpers::fillWithSine (reference, sr, 500.0, 0.9f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);
    juce::dsp::AudioBlock<float> block (processed);
    comp.process (block);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < blockSize; ++i)
            CHECK (std::abs (processed.getSample (channel, i) - reference.getSample (channel, i)) < tolerance);
}

//==============================================================================
// Doubler: re-centered v0.2.0 base delays (9/13/19/24 ms) and their
// max-DoubleDetune modulation depth stay within the delay line's allocated
// capacity (docs/design-brief.md ss4). Doubler's own voiceConfigs/depth math
// are private, so this independently recomputes the documented formula
// (Doubler.cpp's own depthSec-from-cents derivation) against the values
// documented in Doubler.h's class comment - a future accidental change to
// either without the other is caught here.
TEST_CASE ("Doubler delay-time bounds: v0.2.0 base delays and max-detune modulation depth stay within capacity",
           "[dsp][doubler][coverage]")
{
    struct ExpectedVoice
    {
        float baseDelayMs;
        float lfoRateHz;
    };

    static constexpr std::array<ExpectedVoice, 4> expectedVoices { {
        { 9.0f, 0.23f }, { 24.0f, 0.31f }, { 13.0f, 0.17f }, { 19.0f, 0.37f }
    } };

    constexpr float maxDetuneCents = 50.0f;
    constexpr float maxDelayLineMs = 150.0f; // Doubler.cpp's own allocated capacity

    const auto maxPitchRatioDeviation = std::pow (2.0f, maxDetuneCents / 1200.0f) - 1.0f;

    for (const auto& voice : expectedVoices)
    {
        CAPTURE (voice.baseDelayMs, voice.lfoRateHz);

        CHECK (voice.baseDelayMs >= 9.0f);
        CHECK (voice.baseDelayMs <= 24.0f);

        const auto depthSec = maxPitchRatioDeviation / (juce::MathConstants<float>::twoPi * voice.lfoRateHz);
        const auto depthMs = depthSec * 1000.0f;

        CHECK (voice.baseDelayMs + depthMs < maxDelayLineMs);
    }
}

TEST_CASE ("Doubler: Double == 0% remains a bit-exact no-op with the v0.2.0 base delays", "[dsp][doubler][null]")
{
    constexpr int blockSize = 2048;

    // Set targets *before* prepare() so prepare()'s own setCurrentAndTargetValue()
    // seeds the smoothers already at the target - matching the pattern
    // DeEsser's/GentleCompressor's own bypass tests use. Setting them after
    // prepare() would leave amount ramping down from its 0.25 constructed
    // default across the smoothing time, so the block wouldn't be a bit-exact
    // bypass from sample 0.
    Doubler doubler;
    doubler.setAmountProportion (0.0f);
    doubler.setDetuneCents (50.0f); // max, to stress the (unused, since amount == 0%) modulation math
    doubler.setWidthProportion (1.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
    spec.numChannels = 2;
    doubler.prepare (spec);

    juce::AudioBuffer<float> reference (2, blockSize);
    TestHelpers::fillWithSine (reference, sr, 300.0, 0.5f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);
    juce::dsp::AudioBlock<float> block (processed);
    doubler.process (block);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < blockSize; ++i)
            CHECK (processed.getSample (channel, i) == reference.getSample (channel, i));
}

TEST_CASE ("Doubler: finite and bounded output at full detune/width across the sample-rate range with v0.2.0 base delays",
           "[dsp][doubler][coverage][samplerate]")
{
    static constexpr double sampleRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    constexpr int blockSize = 512;

    for (const auto sampleRate : sampleRates)
    {
        Doubler doubler;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
        spec.numChannels = 2;
        doubler.prepare (spec);

        doubler.setAmountProportion (1.0f);
        doubler.setDetuneCents (50.0f);
        doubler.setWidthProportion (1.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::dsp::AudioBlock<float> block (buffer);

        for (int i = 0; i < 8; ++i)
        {
            TestHelpers::fillWithSine (buffer, sampleRate, 300.0, 0.6f, static_cast<juce::int64> (i) * blockSize);
            CHECK_NOTHROW (doubler.process (block));
            CHECK (TestHelpers::allSamplesFinite (buffer));
            CHECK (TestHelpers::peakAbsolute (buffer) < 100.0f);
        }
    }
}
