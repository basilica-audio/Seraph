#include "ParameterLayout.h"
#include "ParameterIds.h"

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
        // Air: fixed-frequency high-shelf gain, -12 to +12 dB, default +3 dB.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::air, 1 },
            "Air",
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
            3.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // Double: doubler send amount, 0-100%, default 25%.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::doubleAmount, 1 },
            "Double",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            25.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // DoubleDetune: modulated-delay detune depth, 0-50 cents, default 15
        // cents - a subtle wobble, not a discrete pitch shift.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::doubleDetune, 1 },
            "Double Detune",
            juce::NormalisableRange<float> (0.0f, 50.0f, 0.01f),
            15.0f,
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

        return layout;
    }
}
