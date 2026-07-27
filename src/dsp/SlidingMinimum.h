#pragma once

#include <juce_core/juce_core.h>

#include <vector>

// Minimum of the most recent `window` values, in amortised O(1) per sample
// (the monotonic-wedge / Lemire algorithm). Used by the de-esser's lookahead
// path to make the gain start descending before the sibilance it belongs to
// reaches the output (see DeEsser.h).
//
// The wedge holds only those past values that could still become the minimum:
// a new value evicts every value at least as large behind it, because none of
// those can be the minimum of any window that also contains the new value.
// The wedge is therefore non-decreasing front to back, and its front is the
// answer.
//
// Storage is a ring buffer sized once in prepare(), so process() never
// allocates. At window == 1 the wedge is emptied and refilled on every call
// and process() returns its argument unchanged - which is what lets a zero
// lookahead setting stay bit-identical to the v0.2.0 gain path.
class SlidingMinimum
{
public:
    // Allocates for the largest window that will ever be requested. Never
    // call from the audio thread.
    void prepare (int maximumWindowLength)
    {
        capacity = juce::jmax (2, maximumWindowLength + 2);
        entries.assign (static_cast<size_t> (capacity), Entry {});
        setWindowLength (window);
    }

    // Changes the window length and clears the wedge. Allocation-free.
    void setWindowLength (int newWindowLength) noexcept
    {
        window = juce::jlimit (1, juce::jmax (1, capacity - 2), newWindowLength);
        reset();
    }

    int getWindowLength() const noexcept { return window; }

    void reset() noexcept
    {
        head = 0;
        tail = 0;
        counter = 0;
    }

    // Pushes `value` and returns the minimum over the trailing `window`
    // values (including this one).
    float process (float value) noexcept
    {
        if (capacity <= 0)
            return value;

        // Evict every tail entry that this value makes irrelevant.
        while (head != tail)
        {
            const auto previous = tail == 0 ? capacity - 1 : tail - 1;

            if (entries[static_cast<size_t> (previous)].value < value)
                break;

            tail = previous;
        }

        entries[static_cast<size_t> (tail)] = Entry { value, counter };
        tail = tail + 1 < capacity ? tail + 1 : 0;

        // Drop the front once it has fallen out of the window.
        while (head != tail && entries[static_cast<size_t> (head)].index <= counter - window)
            head = head + 1 < capacity ? head + 1 : 0;

        ++counter;

        return entries[static_cast<size_t> (head)].value;
    }

private:
    struct Entry
    {
        float value = 0.0f;
        juce::int64 index = 0;
    };

    std::vector<Entry> entries;
    int capacity = 0;
    int head = 0;
    int tail = 0;
    int window = 1;
    juce::int64 counter = 0;
};
