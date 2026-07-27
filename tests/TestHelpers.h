#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <vector>

// Small shared helpers used across the Tests target.
namespace TestHelpers
{
    // Fills every channel of the buffer with a sine wave of the given
    // frequency. `startSampleIndex` offsets the phase calculation, so
    // calling this for consecutive blocks with startSampleIndex incremented
    // by each block's length produces a phase-continuous sine across block
    // boundaries. Defaults to 0.
    inline void fillWithSine (juce::AudioBuffer<float>& buffer,
                              double sampleRate,
                              double frequencyHz,
                              float amplitude = 0.5f,
                              juce::int64 startSampleIndex = 0)
    {
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto phase = juce::MathConstants<double>::twoPi * frequencyHz
                                    * static_cast<double> (startSampleIndex + sample) / sampleRate;
                data[sample] = amplitude * static_cast<float> (std::sin (phase));
            }
        }
    }

    // Root-mean-square level across all channels/samples in the buffer.
    inline double rms (const juce::AudioBuffer<float>& buffer)
    {
        double sumOfSquares = 0.0;
        juce::int64 numValues = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = static_cast<double> (data[sample]);
                sumOfSquares += value * value;
                ++numValues;
            }
        }

        return numValues > 0 ? std::sqrt (sumOfSquares / static_cast<double> (numValues)) : 0.0;
    }

    // Largest absolute sample value across all channels/samples.
    inline float peakAbsolute (const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                peak = std::max (peak, std::abs (data[sample]));
        }

        return peak;
    }

    // Returns true if every sample in the buffer is finite (no NaN/Inf).
    inline bool allSamplesFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (! std::isfinite (data[sample]))
                    return false;
        }

        return true;
    }

    //==========================================================================
    // Spectral helpers (v0.3.0). The doubler's pitch/formant/artifact claims
    // are all frequency-domain, so they need a shared, honest measurement
    // path rather than one hand-rolled FFT per test file.

    // Hann-windowed magnitude spectrum of the first (1 << fftOrder) samples,
    // returned as the non-negative half. Zero-padded if fewer samples are
    // supplied.
    inline std::vector<float> magnitudeSpectrum (const float* data, int numSamples, int fftOrder)
    {
        const auto fftSize = 1 << fftOrder;
        std::vector<float> scratch (static_cast<size_t> (fftSize) * 2, 0.0f);

        const auto count = std::min (numSamples, fftSize);

        for (int sample = 0; sample < count; ++sample)
        {
            // Hann rather than rectangular: these measurements care about
            // sidebands 40-60 dB down, which a rectangular window's -13 dB
            // leakage would bury.
            const auto window = count > 1
                                  ? 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * sample / (count - 1))
                                  : 1.0;
            scratch[static_cast<size_t> (sample)] = data[sample] * static_cast<float> (window);
        }

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (scratch.data());

        return std::vector<float> (scratch.begin(), scratch.begin() + fftSize / 2);
    }

    // Sub-bin refinement of a spectral peak by fitting a parabola through the
    // peak bin and its neighbours in the *log* magnitude domain, which is
    // where a windowed sinusoid's main lobe is closest to quadratic. Returns
    // a fractional bin index.
    inline double parabolicPeakBin (const std::vector<float>& magnitudes, int peakBin)
    {
        if (peakBin <= 0 || peakBin + 1 >= static_cast<int> (magnitudes.size()))
            return peakBin;

        auto logMagnitude = [&magnitudes] (int bin)
        {
            return std::log (std::max (1.0e-20f, magnitudes[static_cast<size_t> (bin)]));
        };

        const auto left = logMagnitude (peakBin - 1);
        const auto right = logMagnitude (peakBin + 1);
        const auto centre = logMagnitude (peakBin);

        const auto denominator = left - 2.0 * centre + right;
        const auto offset = denominator != 0.0 ? 0.5 * (left - right) / denominator : 0.0;

        return peakBin + juce::jlimit (-0.5, 0.5, offset);
    }

    // Frequency of the strongest spectral component, in Hz, refined to
    // sub-bin accuracy. `searchLowHz`/`searchHighHz` bound the search so a
    // DC or out-of-band component cannot win.
    inline double dominantFrequencyHz (const float* data,
                                       int numSamples,
                                       double sampleRate,
                                       int fftOrder,
                                       double searchLowHz = 20.0,
                                       double searchHighHz = 20000.0)
    {
        const auto magnitudes = magnitudeSpectrum (data, numSamples, fftOrder);
        const auto binWidth = sampleRate / static_cast<double> (1 << fftOrder);

        const auto firstBin = std::max (1, static_cast<int> (std::floor (searchLowHz / binWidth)));
        const auto lastBin = std::min (static_cast<int> (magnitudes.size()) - 2,
                                       static_cast<int> (std::ceil (searchHighHz / binWidth)));

        auto peakBin = firstBin;

        for (int bin = firstBin; bin <= lastBin; ++bin)
            if (magnitudes[static_cast<size_t> (bin)] > magnitudes[static_cast<size_t> (peakBin)])
                peakBin = bin;

        return parabolicPeakBin (magnitudes, peakBin) * binWidth;
    }

    // Difference between two frequencies in cents.
    inline double centsBetween (double frequencyHz, double referenceHz)
    {
        if (frequencyHz <= 0.0 || referenceHz <= 0.0)
            return 0.0;

        return 1200.0 * std::log2 (frequencyHz / referenceHz);
    }

    // Spectral envelope: the magnitude spectrum smoothed with a moving
    // average wide enough to bridge the harmonic spacing of a voiced sound,
    // so the peaks that remain are formants rather than individual partials.
    //
    // The averaging happens in the POWER domain and only the result is
    // converted to dB. Averaging log magnitudes instead would be dominated by
    // the deep nulls between harmonics - and those nulls move when the
    // harmonic spacing changes, which is exactly what a pitch shift does, so
    // a dB-domain average produces an "envelope" that tracks the pitch rather
    // than the formants and cannot tell the two apart.
    inline std::vector<float> spectralEnvelopeDb (const std::vector<float>& magnitudes,
                                                  double binWidth,
                                                  double smoothingBandwidthHz)
    {
        const auto radius = std::max (1, static_cast<int> (std::lround (smoothingBandwidthHz / binWidth * 0.5)));

        // Running sum of power, so the cost does not grow with the smoothing
        // width (these spectra are 32k bins and the tests sweep several).
        std::vector<double> cumulativePower (magnitudes.size() + 1, 0.0);

        for (size_t bin = 0; bin < magnitudes.size(); ++bin)
        {
            const auto magnitude = static_cast<double> (magnitudes[bin]);
            cumulativePower[bin + 1] = cumulativePower[bin] + magnitude * magnitude;
        }

        std::vector<float> smoothed (magnitudes.size(), 0.0f);

        for (int bin = 0; bin < static_cast<int> (magnitudes.size()); ++bin)
        {
            const auto first = std::max (0, bin - radius);
            const auto last = std::min (static_cast<int> (magnitudes.size()) - 1, bin + radius);

            const auto power = (cumulativePower[static_cast<size_t> (last) + 1]
                                - cumulativePower[static_cast<size_t> (first)])
                               / (last - first + 1);

            smoothed[static_cast<size_t> (bin)] = static_cast<float> (10.0 * std::log10 (std::max (1.0e-24, power)));
        }

        return smoothed;
    }

    // Spectral envelope by peak tracking: the maximum magnitude within
    // +/- `windowHz`/2 of each bin, in dB.
    //
    // For a voiced sound the harmonic peaks lie ON the envelope and the
    // valleys between them do not, so tracing the local maximum recovers the
    // envelope with far less bias than any kind of averaging - and, unlike an
    // average, it barely moves when the harmonic spacing changes. That
    // property is the whole point here: a pitch shift changes the spacing, so
    // an estimator that reacts to spacing cannot distinguish "the formants
    // moved" from "the pitch moved". The window only needs to be at least one
    // harmonic spacing wide.
    inline std::vector<float> spectralPeakEnvelopeDb (const std::vector<float>& magnitudes,
                                                      double binWidth,
                                                      double windowHz)
    {
        const auto radius = std::max (1, static_cast<int> (std::lround (windowHz / binWidth * 0.5)));

        std::vector<float> envelope (magnitudes.size(), 0.0f);

        for (int bin = 0; bin < static_cast<int> (magnitudes.size()); ++bin)
        {
            const auto first = std::max (0, bin - radius);
            const auto last = std::min (static_cast<int> (magnitudes.size()) - 1, bin + radius);

            auto peak = 0.0f;

            for (int index = first; index <= last; ++index)
                peak = std::max (peak, magnitudes[static_cast<size_t> (index)]);

            envelope[static_cast<size_t> (bin)] = 20.0f * std::log10 (std::max (1.0e-12f, peak));
        }

        return envelope;
    }

    // Frequency of the highest point of a spectral envelope inside a search
    // band, in Hz. Used to locate formants.
    //
    // A peak-tracked envelope is flat for roughly the width of its tracking
    // window on either side of the partial that produced it, so taking the
    // first bin that attains the maximum would bias the answer low by half a
    // window. The midpoint of the plateau - every bin within `plateauDb` of
    // the maximum - is unbiased.
    inline double envelopePeakHz (const std::vector<float>& envelopeDb,
                                  double binWidth,
                                  double searchLowHz,
                                  double searchHighHz,
                                  double plateauDb = 0.25)
    {
        const auto firstBin = std::max (1, static_cast<int> (std::floor (searchLowHz / binWidth)));
        const auto lastBin = std::min (static_cast<int> (envelopeDb.size()) - 2,
                                       static_cast<int> (std::ceil (searchHighHz / binWidth)));

        if (firstBin >= lastBin)
            return 0.0;

        auto peakBin = firstBin;

        for (int bin = firstBin; bin <= lastBin; ++bin)
            if (envelopeDb[static_cast<size_t> (bin)] > envelopeDb[static_cast<size_t> (peakBin)])
                peakBin = bin;

        const auto threshold = envelopeDb[static_cast<size_t> (peakBin)] - static_cast<float> (plateauDb);

        auto plateauFirst = peakBin;
        while (plateauFirst > firstBin && envelopeDb[static_cast<size_t> (plateauFirst - 1)] >= threshold)
            --plateauFirst;

        auto plateauLast = peakBin;
        while (plateauLast < lastBin && envelopeDb[static_cast<size_t> (plateauLast + 1)] >= threshold)
            ++plateauLast;

        return 0.5 * (plateauFirst + plateauLast) * binWidth;
    }

    // Deterministic pseudo-random source for test stimuli. Deliberately not
    // juce::Random or <random>: several tests assert bit-identical renders,
    // which needs an exactly reproducible sequence.
    struct DeterministicNoise
    {
        explicit DeterministicNoise (std::uint32_t seed = 0x12345678u) : state (seed) {}

        float next() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<float> (static_cast<double> (state) / 4294967296.0 * 2.0 - 1.0);
        }

        // One-pole-cascade approximation of pink noise (Voss-McCartney style
        // coefficients), roughly -3 dB/octave across the audio band.
        float nextPink() noexcept
        {
            const auto white = next();
            pink[0] = 0.99765f * pink[0] + white * 0.0990460f;
            pink[1] = 0.96300f * pink[1] + white * 0.2965164f;
            pink[2] = 0.57000f * pink[2] + white * 1.0526913f;
            return (pink[0] + pink[1] + pink[2] + white * 0.1848f) * 0.12f;
        }

        std::uint32_t state;
        float pink[3] { 0.0f, 0.0f, 0.0f };
    };
}
