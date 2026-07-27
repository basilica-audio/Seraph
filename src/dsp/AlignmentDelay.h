#pragma once

#include <juce_dsp/juce_dsp.h>

#include <vector>

// A whole-sample delay whose length can change while audio is running,
// without a click and without reallocating.
//
// Used by the doubler to keep its main path aligned with the Shift mode's
// STFT voices. The STFT engine delays what passes through it; the signal the
// voices are added *onto* does not go through it, so without this the plugin
// would emit a dry component ahead of its own reported latency and every
// host's delay compensation would pull the whole track early.
//
// The delay is an offset into a fixed ring buffer sized for the worst case in
// prepare(), so changing it neither allocates nor drops samples. Because both
// the old and the new read positions are available from that same buffer, a
// change is served by crossfading between them over `fadeSeconds` instead of
// jumping - which is what makes a mid-session mode switch click-free.
//
// At a delay of zero, with no crossfade in flight, process() writes history
// and leaves the audio untouched. That is a deliberate short-circuit rather
// than a delay of zero samples: the default configuration must stay bit-exact.
class AlignmentDelay
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec, int maximumDelaySamples, double fadeSeconds)
    {
        numChannels = static_cast<int> (juce::jmax (juce::uint32 (1), spec.numChannels));
        capacity = juce::jmax (2, maximumDelaySamples + 2);
        history.assign (static_cast<size_t> (numChannels) * static_cast<size_t> (capacity), 0.0f);

        const auto sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        fadeIncrement = static_cast<float> (1.0 / juce::jmax (1.0, fadeSeconds * sampleRate));

        setDelaySamples (targetDelay);
        reset();
    }

    void reset() noexcept
    {
        std::fill (history.begin(), history.end(), 0.0f);
        writeIndex = 0;
        activeDelay = targetDelay;
        previousDelay = targetDelay;
        fadeGain = 1.0f;
    }

    void setDelaySamples (int newDelaySamples) noexcept
    {
        targetDelay = juce::jlimit (0, juce::jmax (0, capacity - 2), newDelaySamples);
    }

    int getDelaySamples() const noexcept { return targetDelay; }

    // Delays `block` in place. No allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        const auto numSamples = static_cast<int> (block.getNumSamples());

        if (numSamples == 0 || capacity <= 0)
            return;

        if (targetDelay != activeDelay)
        {
            previousDelay = activeDelay;
            activeDelay = targetDelay;
            fadeGain = 0.0f;
        }

        const auto channels = juce::jmin (static_cast<int> (block.getNumChannels()), numChannels);
        const bool fading = fadeGain < 1.0f;

        if (activeDelay == 0 && ! fading)
        {
            // Bit-exact short-circuit, but the history still has to advance:
            // a later switch to a non-zero delay must find real audio behind
            // it rather than a buffer of silence.
            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < channels; ++channel)
                    history[static_cast<size_t> (channel) * static_cast<size_t> (capacity)
                            + static_cast<size_t> (writeIndex)] = block.getChannelPointer (static_cast<size_t> (channel))[sample];

                writeIndex = writeIndex + 1 < capacity ? writeIndex + 1 : 0;
            }

            return;
        }

        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                auto* data = block.getChannelPointer (static_cast<size_t> (channel));
                const auto offset = static_cast<size_t> (channel) * static_cast<size_t> (capacity);

                history[offset + static_cast<size_t> (writeIndex)] = data[sample];

                auto read = [this, offset] (int delay) noexcept
                {
                    auto index = writeIndex - delay;
                    while (index < 0)
                        index += capacity;
                    return history[offset + static_cast<size_t> (index)];
                };

                const auto current = read (activeDelay);

                data[sample] = fading ? read (previousDelay) * (1.0f - fadeGain) + current * fadeGain
                                      : current;
            }

            if (fading)
                fadeGain = juce::jmin (1.0f, fadeGain + fadeIncrement);

            writeIndex = writeIndex + 1 < capacity ? writeIndex + 1 : 0;
        }

        if (fadeGain >= 1.0f)
            previousDelay = activeDelay;
    }

private:
    std::vector<float> history;
    int capacity = 0;
    int numChannels = 1;
    int writeIndex = 0;

    int targetDelay = 0;
    int activeDelay = 0;
    int previousDelay = 0;
    float fadeGain = 1.0f;
    float fadeIncrement = 1.0f;
};
