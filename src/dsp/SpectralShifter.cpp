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
    silentBlock.assign (static_cast<size_t> (maximumBlock) * 2, 0.0f);

    auto* inputData = silentBlock.data();
    auto* outputData = silentBlock.data() + maximumBlock;
    process (inputData, outputData, maximumBlock);

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
