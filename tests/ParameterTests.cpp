#include "PluginProcessor.h"
#include "params/ParameterIds.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
    // Convenience wrapper: fetches a parameter by ID and requires it to
    // exist before returning, so every SECTION below fails loudly (not with
    // a null-deref) if an ID typo ever creeps in.
    juce::RangedAudioParameter* requireParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        auto* param = apvts.getParameter (id);
        REQUIRE (param != nullptr);
        return param;
    }

    // Checks that a float parameter's underlying NormalisableRange covers
    // [expectedMin, expectedMax], independent of any skew/log mapping.
    void checkFloatRange (juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& id,
                           float expectedMin,
                           float expectedMax)
    {
        auto* param = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id));
        REQUIRE (param != nullptr);

        const auto range = param->getNormalisableRange().getRange();
        CHECK (range.getStart() == Catch::Approx (expectedMin));
        CHECK (range.getEnd() == Catch::Approx (expectedMax));
    }

    // Checks a float parameter's default value in real (non-normalised)
    // units, going through convertTo0to1 so log-skewed ranges are handled
    // the same way as linear ones.
    void checkFloatDefault (juce::AudioProcessorValueTreeState& apvts,
                             const juce::String& id,
                             float expectedDefault)
    {
        auto* param = requireParam (apvts, id);
        CHECK (param->getDefaultValue() == Catch::Approx (param->convertTo0to1 (expectedDefault)).margin (1e-4));
    }
}

TEST_CASE ("Processor instantiates with the expected parameters", "[processor][parameters]")
{
    SeraphAudioProcessor processor;
    auto& apvts = processor.apvts;

    SECTION ("plugin name")
    {
        CHECK (processor.getName() == juce::String ("Seraph"));
    }

    SECTION ("all documented parameter IDs resolve")
    {
        static constexpr const char* allIds[] = {
            ParamIDs::deEss,
            ParamIDs::deEssFreq,
            ParamIDs::deEssWidth,
            ParamIDs::deEssListen,
            ParamIDs::air,
            ParamIDs::comp,
            ParamIDs::doubleAmount,
            ParamIDs::doubleDetune,
            ParamIDs::doubleWidth,
            ParamIDs::mix,
            ParamIDs::output,
        };

        for (const auto* id : allIds)
            CHECK (apvts.getParameter (id) != nullptr);
    }

    SECTION ("total parameter count matches the v0.2.0 layout")
    {
        // v0.1.0 had 10; v0.2.0 adds DeEssWidth (docs/design-brief.md ss2.1).
        CHECK (apvts.processor.getParameters().size() == 11);
    }

    SECTION ("DeEssListen: sibilance-listen toggle defaults off")
    {
        auto* param = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::deEssListen));
        REQUIRE (param != nullptr);
        CHECK (param->get() == false);
    }

    SECTION ("Comp: gentle compressor amount defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::comp, 0.0f);
        checkFloatRange (apvts, ParamIDs::comp, 0.0f, 100.0f);
    }

    SECTION ("DeEss: sibilance reduction amount defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::deEss, 30.0f);
        checkFloatRange (apvts, ParamIDs::deEss, 0.0f, 100.0f);
    }

    SECTION ("DeEssFreq: sibilance band center defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::deEssFreq, 7000.0f);
        checkFloatRange (apvts, ParamIDs::deEssFreq, 3000.0f, 12000.0f);
    }

    SECTION ("DeEssWidth: sibilance detection bandwidth defaults and range (new in v0.2.0)")
    {
        checkFloatDefault (apvts, ParamIDs::deEssWidth, 40.0f);
        checkFloatRange (apvts, ParamIDs::deEssWidth, 0.0f, 100.0f);
    }

    SECTION ("Air: high-shelf gain defaults and range (v0.2.0: narrowed to -6/+9 dB, default +2 dB)")
    {
        checkFloatDefault (apvts, ParamIDs::air, 2.0f);
        checkFloatRange (apvts, ParamIDs::air, -6.0f, 9.0f);
    }

    SECTION ("Double: doubler send amount defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::doubleAmount, 25.0f);
        checkFloatRange (apvts, ParamIDs::doubleAmount, 0.0f, 100.0f);
    }

    SECTION ("DoubleDetune: modulated-delay detune depth defaults and range (v0.2.0: default 15 -> 10 cents)")
    {
        checkFloatDefault (apvts, ParamIDs::doubleDetune, 10.0f);
        checkFloatRange (apvts, ParamIDs::doubleDetune, 0.0f, 50.0f);
    }

    SECTION ("DoubleDetune: power-law taper gives the tight-double register more knob travel (v0.2.0)")
    {
        // cents = 50 * p^2.2 (docs/design-brief.md ss2.4): at the knob's
        // midpoint (p == 0.5), the resulting cents value must sit below the
        // range's own midpoint (25 cents) - confirms the reshaped taper
        // actually favours the low-cents "tight double" register, not just
        // documented as doing so.
        auto* param = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (ParamIDs::doubleDetune));
        REQUIRE (param != nullptr);

        const auto centsAtMidpoint = param->convertFrom0to1 (0.5f);
        CHECK (centsAtMidpoint < 25.0f);
        CHECK (centsAtMidpoint > 0.0f);
    }

    SECTION ("DoubleWidth: doubler pan spread defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::doubleWidth, 100.0f);
        checkFloatRange (apvts, ParamIDs::doubleWidth, 0.0f, 100.0f);
    }

    SECTION ("Mix: dry/wet defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::mix, 100.0f);
        checkFloatRange (apvts, ParamIDs::mix, 0.0f, 100.0f);
    }

    SECTION ("Output: output trim defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::output, 0.0f);
        checkFloatRange (apvts, ParamIDs::output, -24.0f, 24.0f);
    }
}
