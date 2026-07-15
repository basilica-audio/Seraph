#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>

// Real-time-safe biquad coefficient updates for juce::dsp::IIR::Filter.
//
// juce::dsp::IIR::Coefficients<float>::makeHighShelf/makeBandPass/... (the
// usual way to build filter coefficients) heap-allocate a brand new
// Coefficients object on every call - fine in prepare(), not fine on the
// audio thread, where SeraphEngine's Air high-shelf and DeEsser's bandpass
// detector both used to recompute their coefficients on every process()
// block (basilica-audio/Seraph issues #12, #13).
//
// juce::dsp::IIR::ArrayCoefficients<float>::makeXxx returns the same
// coefficients as a std::array (stack storage, zero allocation). This
// header writes that array's values directly into an *already-allocated*
// Coefficients<float> object's raw coefficient storage (normalising by a0
// exactly the way Coefficients' own constructor/assignImpl does), so
// repeated calls during process() never touch the heap. Same pattern as
// sibling plugin lancet's src/dsp/RealtimeCoefficients.h (whose own comment
// notes it mirrors twist-your-guts's).
//
// JUCE 8.0.14: juce_dsp/processors/juce_IIRFilter_Impl.h
// (Coefficients::assignImpl shows the {b0,b1,b2,a1,a2} normalised-by-a0
// storage layout this mirrors) and juce_IIRFilter.h (getRawCoefficients()).
namespace srph
{
    // Writes a normalised 2nd-order {b0,b1,b2,a1,a2} set (5 raw
    // coefficients) computed from a raw {b0,b1,b2,a0,a1,a2} array (as
    // returned by juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf/
    // makeBandPass/...) into `target`, which must already hold a 2nd-order
    // filter's coefficient storage (i.e. have been constructed via the
    // 6-argument Coefficients constructor, or a prior makeXxx() call, at
    // least once - typically during prepare()) so its backing Array's
    // capacity is already sufficient and this never reallocates.
    inline void applyBiquadCoefficients (juce::dsp::IIR::Coefficients<float>& target,
                                          const std::array<float, 6>& raw) noexcept
    {
        jassert (target.getFilterOrder() == 2);

        auto* dest = target.getRawCoefficients();
        const auto a0 = raw[3];

        dest[0] = raw[0] / a0; // b0
        dest[1] = raw[1] / a0; // b1
        dest[2] = raw[2] / a0; // b2
        dest[3] = raw[4] / a0; // a1
        dest[4] = raw[5] / a0; // a2
    }
}
