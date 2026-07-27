#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <cstdint>

// Per-voice "humanisation": three independent, very slow random walks that
// drift a doubled voice's timing, pitch and level, so a stack of voices comes
// apart the way real singers do instead of tracking clock-locked LFOs (SOTA
// DSP brief ss3.4).
//
// Each walk is uniform white noise from a seeded xorshift64 generator through
// a one-pole low-pass at 0.5 Hz, evaluated at a fixed *control* rate rather
// than per audio sample. The control rate is fixed at one update per
// controlIntervalSamples samples and driven by an internal counter, so the
// drift is independent of the host's block size and of how the engine happens
// to slice a block - two renders of the same material through the same
// prepare()/reset() sequence are bit-identical regardless of buffer size,
// which is what the determinism test in tests/DetuneTests.cpp pins.
//
// Seeds are compile-time constants derived from the voice index, so nothing
// here consults a clock or a random device: "random" means decorrelated, not
// unrepeatable.
//
// At depth == 0 every output is *exactly* 0.0f (not merely small), which is
// what keeps Classic mode bit-identical to v0.2.0 - see Doubler::process().
class VoiceHumanizer
{
public:
    // One control update per 32 samples, matching SeraphEngine's sub-block
    // slice length (brief ss3.7) - at 48 kHz that is a 1.5 kHz control rate,
    // three decades above the 0.5 Hz walk bandwidth.
    static constexpr int controlIntervalSamples = 32;

    void prepare (double newSampleRate, int voiceIndex) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        seedBase = 0x9E3779B97F4A7C15ull * static_cast<std::uint64_t> (voiceIndex + 1) + 0xD1B54A32D192ED03ull;

        const auto controlRate = sampleRate / static_cast<double> (controlIntervalSamples);
        smoothingCoefficient = static_cast<float> (
            1.0 - std::exp (-juce::MathConstants<double>::twoPi * walkFrequencyHz / controlRate));

        // A one-pole low-pass of white noise has variance
        // sigma_in^2 * a / (2 - a); with `a` this small (~2e-3 at 48 kHz) the
        // raw output would swing by well under a percent of full depth, so
        // the walk would be inaudible. Scaling by the inverse of that factor
        // restores unit variance, and targetStandardDeviation then places the
        // clamp at ~2.8 sigma so it engages rarely rather than squaring off
        // the drift. The brief specifies the filter but not this
        // normalisation; without it the feature would not move.
        const auto whiteStandardDeviation = 1.0f / std::sqrt (3.0f); // uniform on [-1, 1]
        normalisationGain = targetStandardDeviation / whiteStandardDeviation
                            * std::sqrt ((2.0f - smoothingCoefficient) / smoothingCoefficient);

        reset();
    }

    void reset() noexcept
    {
        for (int walk = 0; walk < numWalks; ++walk)
        {
            // Distinct, non-zero seed per walk per voice.
            states[walk] = seedBase ^ (0xA24BAED4963EE407ull * static_cast<std::uint64_t> (walk + 1));
            if (states[walk] == 0)
                states[walk] = 0x853C49E6748FEA9Bull;

            filtered[walk] = 0.0f;
        }

        sampleCounter = 0;
    }

    // Advances the walks by `numSamples` of audio time. Call once per slice
    // before reading the offsets below; the internal counter makes the result
    // independent of how the caller chunks its blocks.
    void advance (int numSamples) noexcept
    {
        sampleCounter += numSamples;

        while (sampleCounter >= controlIntervalSamples)
        {
            sampleCounter -= controlIntervalSamples;

            for (int walk = 0; walk < numWalks; ++walk)
            {
                const auto white = nextWhite (states[static_cast<size_t> (walk)]);
                filtered[static_cast<size_t> (walk)] += smoothingCoefficient
                                                        * (white - filtered[static_cast<size_t> (walk)]);
            }
        }
    }

    // Timing drift in milliseconds, +/- maxTimingMs at depth 1.
    float getTimingOffsetMs (float depth01) const noexcept
    {
        return depth01 <= 0.0f ? 0.0f : depth01 * maxTimingMs * walkValue (0);
    }

    // Pitch drift in cents, +/- maxPitchCents at depth 1.
    float getPitchOffsetCents (float depth01) const noexcept
    {
        return depth01 <= 0.0f ? 0.0f : depth01 * maxPitchCents * walkValue (1);
    }

    // Level drift as a linear gain; exactly 1.0f at depth 0.
    float getLevelGain (float depth01) const noexcept
    {
        if (depth01 <= 0.0f)
            return 1.0f;

        return juce::Decibels::decibelsToGain (depth01 * maxLevelDb * walkValue (2));
    }

private:
    static constexpr int numWalks = 3;              // timing, pitch, level
    static constexpr double walkFrequencyHz = 0.5;  // brief ss3.4
    static constexpr float maxTimingMs = 10.0f;
    static constexpr float maxPitchCents = 3.0f;
    static constexpr float maxLevelDb = 1.5f;
    static constexpr float targetStandardDeviation = 0.35f;

    float walkValue (int walk) const noexcept
    {
        return juce::jlimit (-1.0f, 1.0f, filtered[static_cast<size_t> (walk)] * normalisationGain);
    }

    // xorshift64*, mapped to a uniform value on [-1, 1]. Deliberately not
    // std::mt19937/std::uniform_real_distribution: those give no portability
    // guarantee on the exact bit sequence, and the determinism test needs one.
    static float nextWhite (std::uint64_t& state) noexcept
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        const auto scrambled = state * 0x2545F4914F6CDD1Dull;

        // Top 24 bits -> [0, 1), then centred. 24 bits is exactly float's
        // mantissa width, so no rounding is introduced by the conversion.
        const auto unitInterval = static_cast<float> (scrambled >> 40) * (1.0f / 16777216.0f);
        return unitInterval * 2.0f - 1.0f;
    }

    double sampleRate = 44100.0;
    std::uint64_t seedBase = 0;
    float smoothingCoefficient = 0.0f;
    float normalisationGain = 0.0f;

    std::array<std::uint64_t, numWalks> states {};
    std::array<float, numWalks> filtered {};
    int sampleCounter = 0;
};
