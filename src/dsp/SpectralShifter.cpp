#include "SpectralShifter.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <cmath>

// Deliberately NOT built with -ffast-math anywhere in this project: Apple
// Clang 16.0.0 is known to miscompile this engine under it. Nothing here
// enables it; keep it that way.

struct SpectralShifter::Engine
{
    // A fixed seed rather than the default std::random_device constructor:
    // the engine's phase randomisation would otherwise differ between two
    // runs of the same render, and the determinism guarantee this plugin
    // makes for the doubler covers the Shift mode too.
    Engine() : stretch (0x5E6A9Cl) {}

    signalsmith::stretch::SignalsmithStretch<float> stretch;
};

SpectralShifter::SpectralShifter()
    : engine (std::make_unique<Engine>())
{
}

SpectralShifter::~SpectralShifter() = default;

void SpectralShifter::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maximumBlock = juce::jmax (1, maximumBlockSize);

    // Seconds x sample rate, never a fixed bin count: a 96 kHz session keeps
    // the same physical window length (brief ss3.2).
    const auto requestedWindow = juce::jmax (16, static_cast<int> (std::lround (windowSeconds * sampleRate)));
    const auto requestedInterval = juce::jmax (4, static_cast<int> (std::lround (intervalSeconds * sampleRate)));

    engine->stretch.configure (1, requestedWindow, requestedInterval);

    windowSamples = engine->stretch.blockSamples();
    latencySamples = engine->stretch.inputLatency() + engine->stretch.outputLatency();

    setDetuneCents (detuneCents);
    setFormantPreserveEnabled (formantPreserve);

    // Pre-warm: push one maximum-size silent block through so any internal
    // buffer that is sized lazily on first use grows here, on the message
    // thread, rather than inside processBlock().
    scratch.assign (static_cast<size_t> (maximumBlock) * 2, 0.0f);

    process (scratch.data(), scratch.data() + maximumBlock, maximumBlock);

    reset();
}

void SpectralShifter::reset()
{
    engine->stretch.reset();
}

void SpectralShifter::setDetuneCents (float newDetuneCents)
{
    detuneCents = newDetuneCents;

    // Tonality limit as a multiple of the sample rate: above ~8 kHz the
    // engine maps partials more conservatively, which keeps sibilance from
    // being smeared into a chorus of shifted noise.
    const auto tonalityLimit = static_cast<float> (8000.0 / sampleRate);
    engine->stretch.setTransposeSemitones (detuneCents / 100.0f, tonalityLimit);
}

void SpectralShifter::setFormantPreserveEnabled (bool shouldPreserve)
{
    formantPreserve = shouldPreserve;

    if (formantPreserve)
    {
        // Hold the envelope where it is (0 semitones of formant shift) while
        // compensating for the pitch change, with the fundamental auto-
        // detected rather than assumed.
        engine->stretch.setFormantBase (0.0f);
        engine->stretch.setFormantSemitones (0.0f, true);
    }
    else
    {
        // No compensation: the envelope rides along with the partials.
        engine->stretch.setFormantSemitones (0.0f, false);
    }
}

void SpectralShifter::process (const float* input, float* output, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    // Sanitise before the engine sees anything.
    //
    // A phase vocoder carries per-bin phase accumulators from frame to frame,
    // so a single NaN or infinity from a misbehaving upstream plugin poisons
    // the spectral state permanently: the engine cannot recover, and neither
    // does its own reset() - measured, not assumed (tests/RobustnessTests.cpp
    // pins the resulting guarantee). Every other stage in Seraph recovers
    // when the host calls reset(), so this one must not be the exception.
    //
    // Substituting silence for a non-finite sample is the standard boundary
    // defence: the alternative is emitting NaN for the rest of the session.
    // The branch costs nothing measurable next to an FFT.
    const auto usable = juce::jmin (numSamples, maximumBlock);

    if (usable <= 0 || static_cast<int> (scratch.size()) < usable)
        return;

    auto* sanitised = scratch.data();

    for (int sample = 0; sample < usable; ++sample)
    {
        const auto value = input[sample];
        sanitised[sample] = std::isfinite (value) ? value : 0.0f;
    }

    input = sanitised;
    numSamples = usable;

    // The engine's process() is templated over anything indexable as
    // `channels[c][sample]`; these two adaptors avoid needing a
    // heap-allocated pointer array per call.
    struct MonoInput
    {
        const float* data;
        const float* operator[] (int) const noexcept { return data; }
    };

    struct MonoOutput
    {
        float* data;
        float* operator[] (int) const noexcept { return data; }
    };

    engine->stretch.process (MonoInput { input }, numSamples, MonoOutput { output }, numSamples);
}
