#pragma once

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

// One voice of the doubler's `Shift` mode: a mono STFT pitch shifter with
// optional formant preservation, wrapping the MIT-licensed Signalsmith
// Stretch engine (SOTA DSP brief ss3.0/ss3.2; licence notice in
// THIRD-PARTY-NOTICES.md).
//
// Why a vendored phase vocoder rather than something hand-rolled: the two
// classic in-house options both fail on this plugin's actual input. LPC
// residual shifting estimates the spectral envelope with an all-pole model,
// which is at its *worst* on high-pitched choir-register vocals - exactly
// Seraph's subject - and TD-PSOLA needs a reliable pitch mark per period.
// Both assume a monophonic source, and Seraph is routinely fed stacks and
// buses. Signalsmith Stretch is polyphonic-safe, has a maintained formant
// implementation, and is licence-compatible with this project's AGPLv3.
//
// Latency: the engine is a phase vocoder, so it *does* report host latency,
// unlike every other stage in Seraph. configure() is driven from the brief's
// binding budget - a 30 ms window at a 7.5 ms interval, giving roughly 1440
// samples at 48 kHz and staying well inside the 2208-sample ceiling. The
// engine's own presetDefault() would use a 0.12*sr window (~150 ms), which is
// explicitly out of budget for a tracking-vocal insert.
//
// Window sizes are specified in *seconds* times the sample rate, never as a
// fixed bin count, so a 96 kHz session keeps the same ~30 ms physical window
// rather than silently halving it.
//
// This wrapper owns the engine through a pimpl so that the ~40 kB Signalsmith
// header (and its FFT backend) stays out of every translation unit that only
// needs to *hold* a shifter.
class SpectralShifter
{
public:
    // Analysis window and hop, in seconds (brief ss3.0). Public so the
    // latency tests can restate the budget in the same units the engine is
    // configured with.
    static constexpr double windowSeconds = 0.03;
    static constexpr double intervalSeconds = 0.0075;

    SpectralShifter();
    ~SpectralShifter();

    // Configures the engine for the given sample rate and maximum block
    // size. Allocates; never call from the audio thread. Also pre-warms the
    // engine by pushing one silent maximum-size block through it, so any
    // lazily-sized internal buffer is grown here rather than on the first
    // real block (the allocation guard in tests/AllocationTests.cpp is a
    // merge gate for exactly that).
    void prepare (double newSampleRate, int maximumBlockSize);

    // Clears the engine's spectral state without deallocating.
    void reset();

    // Pitch shift in cents. Applied through the engine's transpose API with
    // a tonality limit of 8000/sampleRate, so partials above ~8 kHz are
    // mapped more conservatively and sibilance does not smear.
    void setDetuneCents (float newDetuneCents);

    // Formant preservation. When enabled the engine holds the spectral
    // envelope in place while the partials move (auto-detecting the
    // fundamental); when disabled the formants ride along with the pitch,
    // which within Seraph's +/- 50 cent range is a <= 2.9% shift, near the
    // 3-5% audibility threshold.
    void setFormantPreserveEnabled (bool shouldPreserve);

    // Processes `numSamples` mono samples from `input` into `output`.
    // `input` and `output` may not alias. numSamples is clamped to the
    // maximum block size passed to prepare(). No allocation occurs here.
    //
    // Non-finite input samples are replaced with silence before the engine
    // sees them - see the implementation for why that is load-bearing rather
    // than merely tidy.
    void process (const float* input, float* output, int numSamples) noexcept;

    // Reported latency in samples: the engine's analysis plus synthesis
    // latency. Constant for a given configuration and identical across
    // voices, since every voice uses the same window/interval.
    int getLatencySamples() const noexcept { return latencySamples; }

    // Analysis window length in samples, for the latency-budget tests.
    int getWindowSamples() const noexcept { return windowSamples; }

private:
    struct Engine;
    std::unique_ptr<Engine> engine;

    double sampleRate = 44100.0;
    int latencySamples = 0;
    int windowSamples = 0;
    int maximumBlock = 0;

    float detuneCents = 0.0f;
    bool formantPreserve = true;

    // Pre-allocated working buffer: holds the sanitised copy of the input
    // during process(), and doubles as the silent block prepare() uses to
    // pre-warm the engine. Sized for two maximum blocks so both halves can be
    // used at once during that pre-warm.
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralShifter)
};
