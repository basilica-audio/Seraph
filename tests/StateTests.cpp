#include "PluginProcessor.h"
#include "params/ParameterIds.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("State round-trip preserves non-default values of every parameter", "[state]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto* deEssParam = processor.apvts.getParameter (ParamIDs::deEss);
    auto* deEssFreqParam = processor.apvts.getParameter (ParamIDs::deEssFreq);
    auto* airParam = processor.apvts.getParameter (ParamIDs::air);
    auto* doubleParam = processor.apvts.getParameter (ParamIDs::doubleAmount);
    auto* doubleDetuneParam = processor.apvts.getParameter (ParamIDs::doubleDetune);
    auto* doubleWidthParam = processor.apvts.getParameter (ParamIDs::doubleWidth);
    auto* mixParam = processor.apvts.getParameter (ParamIDs::mix);
    auto* outputParam = processor.apvts.getParameter (ParamIDs::output);

    REQUIRE (deEssParam != nullptr);
    REQUIRE (deEssFreqParam != nullptr);
    REQUIRE (airParam != nullptr);
    REQUIRE (doubleParam != nullptr);
    REQUIRE (doubleDetuneParam != nullptr);
    REQUIRE (doubleWidthParam != nullptr);
    REQUIRE (mixParam != nullptr);
    REQUIRE (outputParam != nullptr);

    deEssParam->setValueNotifyingHost (deEssParam->convertTo0to1 (72.0f));
    deEssFreqParam->setValueNotifyingHost (deEssFreqParam->convertTo0to1 (8200.0f));
    airParam->setValueNotifyingHost (airParam->convertTo0to1 (-4.5f));
    doubleParam->setValueNotifyingHost (doubleParam->convertTo0to1 (55.0f));
    doubleDetuneParam->setValueNotifyingHost (doubleDetuneParam->convertTo0to1 (33.0f));
    doubleWidthParam->setValueNotifyingHost (doubleWidthParam->convertTo0to1 (60.0f));
    mixParam->setValueNotifyingHost (mixParam->convertTo0to1 (42.0f));
    outputParam->setValueNotifyingHost (outputParam->convertTo0to1 (6.5f));

    const auto savedDeEss = deEssParam->getValue();
    const auto savedDeEssFreq = deEssFreqParam->getValue();
    const auto savedAir = airParam->getValue();
    const auto savedDouble = doubleParam->getValue();
    const auto savedDoubleDetune = doubleDetuneParam->getValue();
    const auto savedDoubleWidth = doubleWidthParam->getValue();
    const auto savedMix = mixParam->getValue();
    const auto savedOutput = outputParam->getValue();

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // Reset every parameter back to its default before restoring, so the
    // round-trip assertion below can't pass by accident.
    deEssParam->setValueNotifyingHost (deEssParam->getDefaultValue());
    deEssFreqParam->setValueNotifyingHost (deEssFreqParam->getDefaultValue());
    airParam->setValueNotifyingHost (airParam->getDefaultValue());
    doubleParam->setValueNotifyingHost (doubleParam->getDefaultValue());
    doubleDetuneParam->setValueNotifyingHost (doubleDetuneParam->getDefaultValue());
    doubleWidthParam->setValueNotifyingHost (doubleWidthParam->getDefaultValue());
    mixParam->setValueNotifyingHost (mixParam->getDefaultValue());
    outputParam->setValueNotifyingHost (outputParam->getDefaultValue());

    REQUIRE (deEssParam->getValue() != Catch::Approx (savedDeEss));
    REQUIRE (deEssFreqParam->getValue() != Catch::Approx (savedDeEssFreq));
    REQUIRE (airParam->getValue() != Catch::Approx (savedAir));
    REQUIRE (doubleParam->getValue() != Catch::Approx (savedDouble));
    REQUIRE (doubleDetuneParam->getValue() != Catch::Approx (savedDoubleDetune));
    REQUIRE (doubleWidthParam->getValue() != Catch::Approx (savedDoubleWidth));
    REQUIRE (mixParam->getValue() != Catch::Approx (savedMix));
    REQUIRE (outputParam->getValue() != Catch::Approx (savedOutput));

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (deEssParam->getValue() == Catch::Approx (savedDeEss).margin (1e-6));
    CHECK (deEssFreqParam->getValue() == Catch::Approx (savedDeEssFreq).margin (1e-6));
    CHECK (airParam->getValue() == Catch::Approx (savedAir).margin (1e-6));
    CHECK (doubleParam->getValue() == Catch::Approx (savedDouble).margin (1e-6));
    CHECK (doubleDetuneParam->getValue() == Catch::Approx (savedDoubleDetune).margin (1e-6));
    CHECK (doubleWidthParam->getValue() == Catch::Approx (savedDoubleWidth).margin (1e-6));
    CHECK (mixParam->getValue() == Catch::Approx (savedMix).margin (1e-6));
    CHECK (outputParam->getValue() == Catch::Approx (savedOutput).margin (1e-6));
}
