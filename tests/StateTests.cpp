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
    auto* deEssListenParam = processor.apvts.getParameter (ParamIDs::deEssListen);
    auto* airParam = processor.apvts.getParameter (ParamIDs::air);
    auto* compParam = processor.apvts.getParameter (ParamIDs::comp);
    auto* doubleParam = processor.apvts.getParameter (ParamIDs::doubleAmount);
    auto* doubleDetuneParam = processor.apvts.getParameter (ParamIDs::doubleDetune);
    auto* doubleWidthParam = processor.apvts.getParameter (ParamIDs::doubleWidth);
    auto* mixParam = processor.apvts.getParameter (ParamIDs::mix);
    auto* outputParam = processor.apvts.getParameter (ParamIDs::output);

    REQUIRE (deEssParam != nullptr);
    REQUIRE (deEssFreqParam != nullptr);
    REQUIRE (deEssListenParam != nullptr);
    REQUIRE (airParam != nullptr);
    REQUIRE (compParam != nullptr);
    REQUIRE (doubleParam != nullptr);
    REQUIRE (doubleDetuneParam != nullptr);
    REQUIRE (doubleWidthParam != nullptr);
    REQUIRE (mixParam != nullptr);
    REQUIRE (outputParam != nullptr);

    deEssParam->setValueNotifyingHost (deEssParam->convertTo0to1 (72.0f));
    deEssFreqParam->setValueNotifyingHost (deEssFreqParam->convertTo0to1 (8200.0f));
    deEssListenParam->setValueNotifyingHost (1.0f); // bool params: 1.0 == true
    airParam->setValueNotifyingHost (airParam->convertTo0to1 (-4.5f));
    compParam->setValueNotifyingHost (compParam->convertTo0to1 (65.0f));
    doubleParam->setValueNotifyingHost (doubleParam->convertTo0to1 (55.0f));
    doubleDetuneParam->setValueNotifyingHost (doubleDetuneParam->convertTo0to1 (33.0f));
    doubleWidthParam->setValueNotifyingHost (doubleWidthParam->convertTo0to1 (60.0f));
    mixParam->setValueNotifyingHost (mixParam->convertTo0to1 (42.0f));
    outputParam->setValueNotifyingHost (outputParam->convertTo0to1 (6.5f));

    const auto savedDeEss = deEssParam->getValue();
    const auto savedDeEssFreq = deEssFreqParam->getValue();
    const auto savedDeEssListen = deEssListenParam->getValue();
    const auto savedAir = airParam->getValue();
    const auto savedComp = compParam->getValue();
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
    deEssListenParam->setValueNotifyingHost (deEssListenParam->getDefaultValue());
    airParam->setValueNotifyingHost (airParam->getDefaultValue());
    compParam->setValueNotifyingHost (compParam->getDefaultValue());
    doubleParam->setValueNotifyingHost (doubleParam->getDefaultValue());
    doubleDetuneParam->setValueNotifyingHost (doubleDetuneParam->getDefaultValue());
    doubleWidthParam->setValueNotifyingHost (doubleWidthParam->getDefaultValue());
    mixParam->setValueNotifyingHost (mixParam->getDefaultValue());
    outputParam->setValueNotifyingHost (outputParam->getDefaultValue());

    REQUIRE (deEssParam->getValue() != Catch::Approx (savedDeEss));
    REQUIRE (deEssFreqParam->getValue() != Catch::Approx (savedDeEssFreq));
    REQUIRE (deEssListenParam->getValue() != Catch::Approx (savedDeEssListen));
    REQUIRE (airParam->getValue() != Catch::Approx (savedAir));
    REQUIRE (compParam->getValue() != Catch::Approx (savedComp));
    REQUIRE (doubleParam->getValue() != Catch::Approx (savedDouble));
    REQUIRE (doubleDetuneParam->getValue() != Catch::Approx (savedDoubleDetune));
    REQUIRE (doubleWidthParam->getValue() != Catch::Approx (savedDoubleWidth));
    REQUIRE (mixParam->getValue() != Catch::Approx (savedMix));
    REQUIRE (outputParam->getValue() != Catch::Approx (savedOutput));

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (deEssParam->getValue() == Catch::Approx (savedDeEss).margin (1e-6));
    CHECK (deEssFreqParam->getValue() == Catch::Approx (savedDeEssFreq).margin (1e-6));
    CHECK (deEssListenParam->getValue() == Catch::Approx (savedDeEssListen).margin (1e-6));
    CHECK (airParam->getValue() == Catch::Approx (savedAir).margin (1e-6));
    CHECK (compParam->getValue() == Catch::Approx (savedComp).margin (1e-6));
    CHECK (doubleParam->getValue() == Catch::Approx (savedDouble).margin (1e-6));
    CHECK (doubleDetuneParam->getValue() == Catch::Approx (savedDoubleDetune).margin (1e-6));
    CHECK (doubleWidthParam->getValue() == Catch::Approx (savedDoubleWidth).margin (1e-6));
    CHECK (mixParam->getValue() == Catch::Approx (savedMix).margin (1e-6));
    CHECK (outputParam->getValue() == Catch::Approx (savedOutput).margin (1e-6));
}

// docs/design-brief.md ss6: tolerant import of a v0.1.0-shaped state (no
// DeEssWidth entry, which was added in v0.2.0). Loading such a state must
// not fail or reset any *other* parameter, and the missing DeEssWidth must
// land on its documented default (40%) rather than 0%/garbage - which holds
// here because a freshly constructed SeraphAudioProcessor already sits at
// each parameter's ParameterLayout default (including DeEssWidth == 40%,
// see src/params/ParameterLayout.cpp) before setStateInformation() is ever
// called, and JUCE 8.0.14's AudioProcessorValueTreeState::replaceState()
// (juce_AudioProcessorValueTreeState.cpp's updateParameterConnectionsToChildTrees())
// never touches a parameter's live value when its own "PARAM" child is
// absent from the loaded state - it only appends a fresh (valueless) child
// tree for it, leaving the parameter's current value exactly as it was.
TEST_CASE ("setStateInformation tolerantly imports a v0.1.0-shaped state missing DeEssWidth", "[state][migration]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // Hand-built to match AudioProcessorValueTreeState's own state shape
    // (root "PARAMETERS", "PARAM" children with "id"/"value" properties -
    // see JUCE 8.0.14's juce_AudioProcessorValueTreeState.h's
    // valueType/idPropertyID/valuePropertyID), but omitting deEssWidth
    // entirely - the v0.1.0 parameter set this simulates never had it.
    juce::ValueTree oldState ("PARAMETERS");

    auto addParam = [&oldState] (const char* id, float value)
    {
        juce::ValueTree param ("PARAM");
        param.setProperty ("id", id, nullptr);
        param.setProperty ("value", value, nullptr);
        oldState.appendChild (param, nullptr);
    };

    addParam (ParamIDs::deEss, 72.0f);
    addParam (ParamIDs::deEssFreq, 8200.0f);
    addParam (ParamIDs::deEssListen, 1.0f);
    addParam (ParamIDs::air, -4.5f); // inside both the old (-12/+12) and new (-6/+9) ranges
    addParam (ParamIDs::comp, 65.0f);
    addParam (ParamIDs::doubleAmount, 55.0f);
    addParam (ParamIDs::doubleDetune, 33.0f);
    addParam (ParamIDs::doubleWidth, 60.0f);
    addParam (ParamIDs::mix, 42.0f);
    addParam (ParamIDs::output, 6.5f);
    // ParamIDs::deEssWidth deliberately absent - this is the whole point.

    const std::unique_ptr<juce::XmlElement> xml (oldState.createXml());
    juce::MemoryBlock oldStateBinary;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldStateBinary);

    processor.setStateInformation (oldStateBinary.getData(), static_cast<int> (oldStateBinary.getSize()));

    auto* deEssWidthParam = processor.apvts.getParameter (ParamIDs::deEssWidth);
    REQUIRE (deEssWidthParam != nullptr);
    CHECK (deEssWidthParam->getValue() == Catch::Approx (deEssWidthParam->convertTo0to1 (40.0f)).margin (1e-4));

    auto checkParam = [&processor] (const char* id, float expected)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        CHECK (param->convertFrom0to1 (param->getValue()) == Catch::Approx (expected).margin (1e-3));
    };

    checkParam (ParamIDs::deEss, 72.0f);
    checkParam (ParamIDs::deEssFreq, 8200.0f);
    checkParam (ParamIDs::deEssListen, 1.0f);
    checkParam (ParamIDs::air, -4.5f);
    checkParam (ParamIDs::comp, 65.0f);
    checkParam (ParamIDs::doubleAmount, 55.0f);
    checkParam (ParamIDs::doubleDetune, 33.0f);
    checkParam (ParamIDs::doubleWidth, 60.0f);
    checkParam (ParamIDs::mix, 42.0f);
    checkParam (ParamIDs::output, 6.5f);
}

//==============================================================================
// v0.3.0 state schema (SOTA DSP brief ss4, test plan ss6.15).

TEST_CASE ("getStateInformation stamps schema version 2 on the APVTS root", "[state][schema]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    const std::unique_ptr<juce::XmlElement> xml (
        juce::AudioProcessor::getXmlFromBinary (savedState.getData(), static_cast<int> (savedState.getSize())));
    REQUIRE (xml != nullptr);

    const auto state = juce::ValueTree::fromXml (*xml);
    CHECK (state.hasProperty (SeraphAudioProcessor::stateVersionProperty));
    CHECK (SeraphAudioProcessor::readStateSchemaVersion (state) == SeraphAudioProcessor::stateSchemaVersion);
    CHECK (SeraphAudioProcessor::stateSchemaVersion == 2);
}

TEST_CASE ("A state tree without the version attribute reads as schema version 1", "[state][schema][migration]")
{
    // This is exactly the shape every v0.1.x/v0.2.0 session carries.
    const juce::ValueTree legacyState ("PARAMETERS");
    CHECK (SeraphAudioProcessor::readStateSchemaVersion (legacyState) == 1);
}

TEST_CASE ("State round-trip is exact for all 19 parameters", "[state][schema]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto& parameters = processor.apvts.processor.getParameters();
    REQUIRE (parameters.size() == 19);

    // Drive every parameter to a non-default position.
    //
    // Discrete parameters (the bools and the two choices) need a value that
    // is already snapped to a legal step: APVTS persists each parameter's
    // *denormalised* value, and NormalisableRange::convertFrom0to1() snaps to
    // the interval on the way out, so an arbitrary normalised position like
    // 0.37 would come back as 0.0 for a bool and make this test assert
    // quantisation noise instead of round-trip fidelity. Continuous
    // parameters get an arbitrary off-default position.
    std::vector<float> expected;
    expected.reserve (static_cast<size_t> (parameters.size()));

    for (int index = 0; index < parameters.size(); ++index)
    {
        auto* parameter = parameters[index];
        const auto steps = parameter->getNumSteps();
        const bool isDiscrete = steps > 1 && steps <= 64;

        // For discrete parameters pick whichever end of the range is not the
        // default, so the restore below still has something to prove.
        const auto target = isDiscrete
                              ? (std::abs (parameter->getDefaultValue() - 1.0f) > 1.0e-6f ? 1.0f : 0.0f)
                              : ((index % 2 == 0) ? 0.37f : 0.63f);

        parameter->setValueNotifyingHost (target);
        expected.push_back (parameter->getValue());

        INFO ("parameter index " << index);
        CHECK (parameter->getValue() != Catch::Approx (parameter->getDefaultValue()).margin (1e-6));
    }

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);

    for (auto* parameter : parameters)
        parameter->setValueNotifyingHost (parameter->getDefaultValue());

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    for (int index = 0; index < parameters.size(); ++index)
    {
        INFO ("parameter index " << index << " ("
                                 << dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[index])->paramID << ")");
        // 1e-5 rather than exact equality: the log-frequency and power-taper
        // ranges make a round-trip through float storage in real units, so a
        // last-bit difference in the normalised position is expected there.
        CHECK (parameters[index]->getValue() == Catch::Approx (expected[static_cast<size_t> (index)]).margin (1e-5));
    }
}

// Brief ss4/ss6.15: a v0.2.0-shaped state (eleven parameters, no stateVersion
// attribute, none of the eight v0.3.0 IDs) must import with all eleven carried
// values exact and all eight new parameters sitting on their neutral defaults
// - that is what makes an upgraded session render bit-identical (the render
// side of the same contract is pinned in tests/EngineTests.cpp).
TEST_CASE ("A v0.2.0-shaped state migrates to 8 neutral defaults and 11 exact values", "[state][schema][migration]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::ValueTree oldState ("PARAMETERS");

    auto addParam = [&oldState] (const char* id, float value)
    {
        juce::ValueTree param ("PARAM");
        param.setProperty ("id", id, nullptr);
        param.setProperty ("value", value, nullptr);
        oldState.appendChild (param, nullptr);
    };

    addParam (ParamIDs::deEss, 61.0f);
    addParam (ParamIDs::deEssFreq, 6400.0f);
    addParam (ParamIDs::deEssWidth, 55.0f);
    addParam (ParamIDs::deEssListen, 0.0f);
    addParam (ParamIDs::air, 4.25f);
    addParam (ParamIDs::comp, 45.0f);
    addParam (ParamIDs::doubleAmount, 70.0f);
    addParam (ParamIDs::doubleDetune, 22.0f);
    addParam (ParamIDs::doubleWidth, 85.0f);
    addParam (ParamIDs::mix, 90.0f);
    addParam (ParamIDs::output, -1.5f);
    // No stateVersion attribute and none of the v0.3.0 IDs - the point.

    CHECK (SeraphAudioProcessor::readStateSchemaVersion (oldState) == 1);

    const std::unique_ptr<juce::XmlElement> xml (oldState.createXml());
    juce::MemoryBlock oldStateBinary;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldStateBinary);

    // Move the new parameters off their defaults first, so "lands on the
    // default" below cannot pass just because nothing ever touched them.
    auto* humanize = processor.apvts.getParameter (ParamIDs::doubleHumanize);
    auto* knee = processor.apvts.getParameter (ParamIDs::deEssKnee);
    REQUIRE (humanize != nullptr);
    REQUIRE (knee != nullptr);
    humanize->setValueNotifyingHost (0.8f);
    knee->setValueNotifyingHost (0.8f);

    processor.setStateInformation (oldStateBinary.getData(), static_cast<int> (oldStateBinary.getSize()));

    auto realValue = [&processor] (const char* id)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        return param->convertFrom0to1 (param->getValue());
    };

    // The eleven carried parameters: exact.
    CHECK (realValue (ParamIDs::deEss) == Catch::Approx (61.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::deEssFreq) == Catch::Approx (6400.0f).margin (1e-1));
    CHECK (realValue (ParamIDs::deEssWidth) == Catch::Approx (55.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::deEssListen) == Catch::Approx (0.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::air) == Catch::Approx (4.25f).margin (1e-3));
    CHECK (realValue (ParamIDs::comp) == Catch::Approx (45.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::doubleAmount) == Catch::Approx (70.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::doubleDetune) == Catch::Approx (22.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::doubleWidth) == Catch::Approx (85.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::mix) == Catch::Approx (90.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::output) == Catch::Approx (-1.5f).margin (1e-3));

    // The eight new parameters: neutral defaults, i.e. v0.2.0 behaviour.
    CHECK (realValue (ParamIDs::doubleMode) == Catch::Approx (0.0f).margin (1e-3));      // Classic
    CHECK (realValue (ParamIDs::doubleHumanize) == Catch::Approx (0.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::doubleFormant) == Catch::Approx (1.0f).margin (1e-3));   // inert outside Shift
    CHECK (realValue (ParamIDs::deEssLink) == Catch::Approx (0.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::deEssKnee) == Catch::Approx (0.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::deEssLookahead) == Catch::Approx (0.0f).margin (1e-3));
    CHECK (realValue (ParamIDs::airFreq) == Catch::Approx (1.0f).margin (1e-3));         // 12 kHz
    CHECK (realValue (ParamIDs::compLink) == Catch::Approx (0.0f).margin (1e-3));
}
