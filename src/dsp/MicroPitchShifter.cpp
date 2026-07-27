#include "MicroPitchShifter.h"

namespace
{
    // Below this the ratio is treated as exactly 1: the sweep is frozen and
    // the voice degenerates to a static delay (see MicroPitchShifter.h). The
    // bound is far below the 0.5-cent accuracy the shifter is measured to
    // (1e-7 in ratio is about 1.7e-4 cents), so it can never swallow a detune
    // a listener or a test could resolve.
    constexpr float ratioDeadZone = 1.0e-7f;
}

void MicroPitchShifter::prepare (double newSampleRate, float newBaseDelayMs)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    baseDelayMs = newBaseDelayMs;

    bufferSize = static_cast<int> (std::ceil (sampleRate * maxDelayLineMs / 1000.0)) + 8;
    buffer.assign (static_cast<size_t> (bufferSize), 0.0f);

    sweepEngagement.reset (sampleRate, smoothingTimeSeconds);
    sweepEngagement.setCurrentAndTargetValue (std::abs (ratio - 1.0f) > ratioDeadZone ? 1.0f : 0.0f);

    reset();
}

void MicroPitchShifter::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
    sweepPhase = 0.0;
    sweepEngagement.setCurrentAndTargetValue (std::abs (ratio - 1.0f) > ratioDeadZone ? 1.0f : 0.0f);
}

void MicroPitchShifter::setDetuneCents (float newDetuneCents) noexcept
{
    if (newDetuneCents == detuneCents)
        return;

    detuneCents = newDetuneCents;
    ratio = std::pow (2.0f, detuneCents / 1200.0f);

    const auto ratioOffset = std::abs (ratio - 1.0f);

    if (ratioOffset > ratioDeadZone)
    {
        // f_x = |1 - r| / D, the rate at which each head has to be reset.
        sweepRateHz = ratioOffset / (sweepRangeMs * 0.001f);
        phaseIncrement = static_cast<double> (sweepRateHz) / sampleRate;
        sweepEngagement.setTargetValue (1.0f);
    }
    else
    {
        sweepRateHz = 0.0f;
        phaseIncrement = 0.0;
        sweepEngagement.setTargetValue (0.0f);
    }
}

void MicroPitchShifter::setBaseDelayOffsetMs (float newOffsetMs) noexcept
{
    baseDelayOffsetMs = newOffsetMs;
}

float MicroPitchShifter::getSweepRateHz() const noexcept
{
    return sweepRateHz;
}

float MicroPitchShifter::readCubic (float delaySamples) const noexcept
{
    const auto maxDelay = static_cast<float> (bufferSize - 4);
    const auto clampedDelay = juce::jlimit (minReadDelaySamples, maxDelay, delaySamples);

    const auto integerDelay = static_cast<int> (clampedDelay);
    const auto fraction = clampedDelay - static_cast<float> (integerDelay);

    // Four taps in increasing-delay (decreasing-time) order. `fraction`
    // moves from the tap at `integerDelay` toward the tap at
    // `integerDelay + 1`, i.e. backwards in time, so the Catmull-Rom
    // parameter is `fraction` with p1/p2 being those two taps.
    auto tapAt = [this] (int delay) noexcept
    {
        auto index = writeIndex - delay;
        while (index < 0)
            index += bufferSize;
        return buffer[static_cast<size_t> (index)];
    };

    const auto p0 = tapAt (integerDelay - 1);
    const auto p1 = tapAt (integerDelay);
    const auto p2 = tapAt (integerDelay + 1);
    const auto p3 = tapAt (integerDelay + 2);

    const auto t = fraction;
    const auto t2 = t * t;
    const auto t3 = t2 * t;

    return 0.5f * ((2.0f * p1)
                   + (-p0 + p2) * t
                   + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                   + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float MicroPitchShifter::processSample (float input) noexcept
{
    if (bufferSize <= 0)
        return 0.0f;

    // Advance the write head first so a delay of `d` samples reads the sample
    // written `d` writes ago.
    writeIndex = writeIndex + 1 < bufferSize ? writeIndex + 1 : 0;
    buffer[static_cast<size_t> (writeIndex)] = input;

    const auto sampleRateF = static_cast<float> (sampleRate);
    const auto floorMs = juce::jmax (0.0f, baseDelayMs + baseDelayOffsetMs);
    const auto floorSamples = floorMs * 0.001f * sampleRateF;
    const auto sweepSamples = sweepRangeMs * 0.001f * sampleRateF;

    const auto engagement = sweepEngagement.getNextValue();

    // The static reference read: a plain delay at the sweep floor. This is
    // the whole output at a settled zero detune, and the analytic reference
    // the Micro null test compares against.
    const auto staticOutput = readCubic (floorSamples);

    if (engagement <= 0.0f)
        return staticOutput;

    // Head A leads head B by half a sweep; the gains below make whichever
    // head is at its wrap point silent at that instant.
    const auto phaseA = sweepPhase + 0.5 >= 1.0 ? sweepPhase - 0.5 : sweepPhase + 0.5;
    const auto phaseB = sweepPhase;

    // r > 1 needs a shrinking delay (base + D -> base); r < 1 a growing one.
    const auto shrinking = ratio > 1.0f;
    const auto positionA = shrinking ? 1.0 - phaseA : phaseA;
    const auto positionB = shrinking ? 1.0 - phaseB : phaseB;

    const auto delayA = floorSamples + sweepSamples * static_cast<float> (positionA);
    const auto delayB = floorSamples + sweepSamples * static_cast<float> (positionB);

    const auto angle = juce::MathConstants<float>::pi * static_cast<float> (sweepPhase);
    const auto cosine = std::cos (angle);
    const auto sine = std::sin (angle);
    const auto gainA = cosine * cosine;
    const auto gainB = sine * sine;

    const auto sweptOutput = gainA * readCubic (delayA) + gainB * readCubic (delayB);

    sweepPhase += phaseIncrement;
    if (sweepPhase >= 1.0)
        sweepPhase -= 1.0;

    if (engagement >= 1.0f)
        return sweptOutput;

    return staticOutput * (1.0f - engagement) + sweptOutput * engagement;
}
