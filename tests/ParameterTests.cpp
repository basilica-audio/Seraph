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
            ParamIDs::air,
            ParamIDs::doubleAmount,
            ParamIDs::doubleDetune,
            ParamIDs::doubleWidth,
            ParamIDs::mix,
            ParamIDs::output,
        };

        for (const auto* id : allIds)
            CHECK (apvts.getParameter (id) != nullptr);
    }

    SECTION ("total parameter count matches the v0.1 layout")
    {
        CHECK (apvts.processor.getParameters().size() == 8);
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

    SECTION ("Air: high-shelf gain defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::air, 3.0f);
        checkFloatRange (apvts, ParamIDs::air, -12.0f, 12.0f);
    }

    SECTION ("Double: doubler send amount defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::doubleAmount, 25.0f);
        checkFloatRange (apvts, ParamIDs::doubleAmount, 0.0f, 100.0f);
    }

    SECTION ("DoubleDetune: modulated-delay detune depth defaults and range")
    {
        checkFloatDefault (apvts, ParamIDs::doubleDetune, 15.0f);
        checkFloatRange (apvts, ParamIDs::doubleDetune, 0.0f, 50.0f);
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
