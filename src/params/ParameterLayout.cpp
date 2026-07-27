#include "ParameterLayout.h"
#include "ParameterIds.h"

#include <cmath>

namespace
{
    // True logarithmic (base-10) mapping for frequency parameters, so slider/
    // knob travel spends equal space per octave rather than per Hz. Uses
    // juce::mapToLog10/mapFromLog10 rather than NormalisableRange's built-in
    // power-law skew, which only approximates a log curve.
    juce::NormalisableRange<float> makeLogFrequencyRange (float minHz, float maxHz)
    {
        return juce::NormalisableRange<float> (
            minHz,
            maxHz,
            [] (float rangeStart, float rangeEnd, float normalised)
            { return juce::mapToLog10 (normalised, rangeStart, rangeEnd); },
            [] (float rangeStart, float rangeEnd, float value)
            { return juce::mapFromLog10 (value, rangeStart, rangeEnd); });
    }

    // Power-law ("reshaped log-ish") taper for DoubleDetune
    // (docs/design-brief.md ss2.4): normalised knob position p in [0,1] maps
    // to `value = minVal + (maxVal - minVal) * p^exponent`, giving the
    // reference-validated low-cents "tight double" register more knob travel
    // than the upper "loose chorus" register, without changing the
    // parameter's stored real-unit (cents) value - see ParameterIds.h's
    // deEssWidth comment and docs/design-brief.md ss6 for why this is not a
    // breaking change to saved state (JUCE 8.0.14's
    // AudioProcessorValueTreeState persists each parameter's denormalised/
    // real value, not its normalised knob position - verified against
    // juce_AudioProcessorValueTreeState.cpp's ParameterAdapter::flushToTree).
    juce::NormalisableRange<float> makePowerTaperRange (float minVal, float maxVal, float exponent)
    {
        return juce::NormalisableRange<float> (
            minVal,
            maxVal,
            [exponent] (float rangeStart, float rangeEnd, float normalised)
            { return rangeStart + (rangeEnd - rangeStart) * std::pow (normalised, exponent); },
            [exponent] (float rangeStart, float rangeEnd, float value)
            {
                const auto span = rangeEnd - rangeStart;
                const auto proportion = span > 0.0f ? juce::jlimit (0.0f, 1.0f, (value - rangeStart) / span) : 0.0f;
                return std::pow (proportion, 1.0f / exponent);
            });
    }
}

namespace srph
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        //======================================================================
        // DeEss: sibilance gain-reduction amount, 0-100%, default 30%.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::deEss, 1 },
            "De-Ess",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            30.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // DeEssFreq: sibilance detection/reduction band center, 3-12 kHz,
        // default 7 kHz (within the ~5-9 kHz sibilance register).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::deEssFreq, 1 },
            "De-Ess Freq",
            makeLogFrequencyRange (3000.0f, 12000.0f),
            7000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        //======================================================================
        // DeEssListen: sibilance-listen ("solo") mode, off by default. A
        // bit-exact no-op on the rest of the chain when off.
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::deEssListen, 1 },
            "De-Ess Listen",
            false));

        //======================================================================
        // DeEssWidth (new in v0.2.0): sibilance detection bandwidth, 0-100%,
        // default 40% - maps to detector Q 3.0 (narrow, 0%) -> 0.7 (wide,
        // 100%) in DeEsser. See ParameterIds.h and docs/design-brief.md ss2.1.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::deEssWidth, 1 },
            "De-Ess Width",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            40.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // Air: fixed-frequency high-shelf gain. v0.2.0: range narrowed and
        // re-centered from -12/+12 dB to -6/+9 dB (default +2 dB), matching
        // the reference class's effective ~5-6 dB max audible lift more
        // closely than the old ±12 dB range - see docs/design-brief.md ss2.2.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::air, 1 },
            "Air",
            juce::NormalisableRange<float> (-6.0f, 9.0f, 0.01f),
            2.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // Comp: gentle broadband downward-compressor amount, 0-100%, default
        // 0% (bit-exact bypass) - an optional glue stage, not applied by
        // default the way DeEss/Air are.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::comp, 1 },
            "Comp",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // Double: doubler send amount, 0-100%, default 25%.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::doubleAmount, 1 },
            "Double",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            25.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // DoubleDetune: modulated-delay detune depth, 0-50 cents, default 10
        // cents (v0.2.0: was 15) - a subtle wobble, not a discrete pitch
        // shift. v0.2.0 also reshapes the knob taper to a power curve
        // (cents = 50 * p^2.2) so the reference-validated 4-20 cent "tight
        // double" register gets more knob travel than the 20-50 cent "loose
        // chorus" register; the range and the parameter's stored real-unit
        // (cents) value are both unchanged - see docs/design-brief.md ss2.4/ss6.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::doubleDetune, 1 },
            "Double Detune",
            makePowerTaperRange (0.0f, 50.0f, 2.2f),
            10.0f,
            juce::AudioParameterFloatAttributes().withLabel ("cents")));

        //======================================================================
        // DoubleWidth: stereo pan spread of the two doubled voices, 0-100%,
        // default 100% (classic hard-panned doubler).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::doubleWidth, 1 },
            "Double Width",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // Mix: dry/wet. Default 100% (fully wet) - a vocal channel-strip
        // processor is normally run fully in the signal path, not blended.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::mix, 1 },
            "Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // Output: output trim, applied after the doubler and before Mix.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::output, 1 },
            "Output",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // v0.3.0 additions (SOTA DSP brief ss4). Deliberately appended after
        // the frozen v0.1/v0.2 block rather than interleaved in signal-flow
        // order: APVTS persists by ID, but hosts that address parameters by
        // *index* (some AU/VST3 automation lanes do) would otherwise see every
        // existing parameter shift, silently re-pointing saved automation.
        //
        // Every default below reproduces v0.2.0 behaviour exactly.

        //======================================================================
        // DoubleMode: Classic (v0.2.0's modulated-delay chorus) / Micro
        // (dual-head constant-ratio micropitch) / Shift (STFT pitch shift).
        // Not automatable: Shift reports host latency, and a latency change
        // mid-automation is exactly the thing hosts cope with worst.
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::doubleMode, 1 },
            "Double Mode",
            juce::StringArray { "Classic", "Micro", "Shift" },
            0,
            juce::AudioParameterChoiceAttributes().withAutomatable (false)));

        //======================================================================
        // DoubleHumanize: per-voice random-walk drift depth, 0-100%, default
        // 0% (all three offsets exactly zero -> Classic stays bit-identical).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::doubleHumanize, 1 },
            "Humanize",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // DoubleFormant: formant preservation in Shift mode, default on.
        // Inert in Classic/Micro, so "on" is still neutral for old sessions.
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::doubleFormant, 1 },
            "Formant Preserve",
            true));

        //======================================================================
        // DeEssLink: stereo-linked sibilance detection, default off.
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::deEssLink, 1 },
            "De-Ess Link",
            false));

        //======================================================================
        // DeEssKnee: soft-knee width in dB, 0-12, default 0 (hard knee).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::deEssKnee, 1 },
            "De-Ess Knee",
            juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // DeEssLookahead: 0-2 ms, default 0. Not automatable - it changes
        // reported host latency (see DoubleMode above).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::deEssLookahead, 1 },
            "De-Ess Lookahead",
            juce::NormalisableRange<float> (0.0f, 2.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms").withAutomatable (false)));

        //======================================================================
        // AirFreq: high-shelf corner, 10/12/15 kHz, default 12 kHz - the
        // fixed constant v0.1/v0.2 always used, so index 1 is neutral.
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::airFreq, 1 },
            "Air Freq",
            juce::StringArray { "10 kHz", "12 kHz", "15 kHz" },
            1));

        //======================================================================
        // CompLink: stereo-linked compressor detection, default off.
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::compLink, 1 },
            "Comp Link",
            false));

        return layout;
    }
}
