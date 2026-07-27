#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterIds.h"
#include "params/ParameterLayout.h"

#include <BinaryData.h>

namespace
{
    // The small, Seraph-specific config surface PresetManager needs (see
    // src/presets/PresetManager.h's class docs) - everything else about the
    // preset system is fully generic and portable across the suite (see
    // docs/preset-system-notes.md in sibling plugin nave, the M2 pilot).
    basilica::presets::PresetManagerConfig makePresetManagerConfig()
    {
        // JucePlugin_CFBundleIdentifier expands to a raw (unquoted) token
        // sequence, not a string literal - JUCE_STRINGIFY() is the
        // documented way to turn it into one. Always "com.yvesvogl.seraph"
        // here (BUNDLE_ID in CMakeLists.txt), matching the "plugin" field
        // baked into every presets/factory/*.json file.
        basilica::presets::PresetManagerConfig config;
        config.pluginId = JUCE_STRINGIFY (JucePlugin_CFBundleIdentifier);
        config.pluginName = JucePlugin_Name;
        config.manufacturerName = "Yves Vogl";
        config.pluginVersion = JucePlugin_VersionString;
        // userPresetsDirectoryOverrideForTests intentionally left
        // default-constructed (empty) - production instances always use the
        // real platform-standard preset location (see PresetManager.h).
        return config;
    }

    // BinaryData symbol names are derived from the presets/factory/*.json
    // file names passed to juce_add_binary_data() in CMakeLists.txt (dots
    // become underscores) - this list must stay in sync with that SOURCES
    // list. Order here only affects factory-preset iteration order before
    // getAllPresets() re-sorts alphabetically, so it isn't otherwise
    // significant.
    std::vector<basilica::presets::FactoryPresetAsset> makeFactoryPresetAssets()
    {
        return {
            { BinaryData::default_json, BinaryData::default_jsonSize },
            { BinaryData::leadCutThrough_json, BinaryData::leadCutThrough_jsonSize },
            { BinaryData::leadIntimateCloseMic_json, BinaryData::leadIntimateCloseMic_jsonSize },
            { BinaryData::choirWideSpread_json, BinaryData::choirWideSpread_jsonSize },
            { BinaryData::choirTightBlend_json, BinaryData::choirTightBlend_jsonSize },
            { BinaryData::spokenGrowledInterlude_json, BinaryData::spokenGrowledInterlude_jsonSize },
            { BinaryData::glueOnly_json, BinaryData::glueOnly_jsonSize },
            { BinaryData::deEssOnlySurgical_json, BinaryData::deEssOnlySurgical_jsonSize },
            { BinaryData::wideDoubleNoDynamics_json, BinaryData::wideDoubleNoDynamics_jsonSize },
        };
    }
}

//==============================================================================
SeraphAudioProcessor::SeraphAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      presetManager (apvts, makePresetManagerConfig(), makeFactoryPresetAssets())
{
    deEssAmount = apvts.getRawParameterValue (ParamIDs::deEss);
    deEssFreqHz = apvts.getRawParameterValue (ParamIDs::deEssFreq);
    deEssWidth = apvts.getRawParameterValue (ParamIDs::deEssWidth);
    deEssListen = apvts.getRawParameterValue (ParamIDs::deEssListen);
    airDb = apvts.getRawParameterValue (ParamIDs::air);
    compAmount = apvts.getRawParameterValue (ParamIDs::comp);
    doubleAmount = apvts.getRawParameterValue (ParamIDs::doubleAmount);
    doubleDetuneCents = apvts.getRawParameterValue (ParamIDs::doubleDetune);
    doubleWidth = apvts.getRawParameterValue (ParamIDs::doubleWidth);
    mixPercent = apvts.getRawParameterValue (ParamIDs::mix);
    outputDb = apvts.getRawParameterValue (ParamIDs::output);
    doubleMode = apvts.getRawParameterValue (ParamIDs::doubleMode);
    doubleHumanize = apvts.getRawParameterValue (ParamIDs::doubleHumanize);
    doubleFormant = apvts.getRawParameterValue (ParamIDs::doubleFormant);
    deEssLink = apvts.getRawParameterValue (ParamIDs::deEssLink);
    deEssKnee = apvts.getRawParameterValue (ParamIDs::deEssKnee);
    deEssLookahead = apvts.getRawParameterValue (ParamIDs::deEssLookahead);
    airFreq = apvts.getRawParameterValue (ParamIDs::airFreq);
    compLink = apvts.getRawParameterValue (ParamIDs::compLink);

    jassert (doubleMode != nullptr);
    jassert (doubleHumanize != nullptr);
    jassert (doubleFormant != nullptr);
    jassert (deEssLink != nullptr);
    jassert (deEssKnee != nullptr);
    jassert (deEssLookahead != nullptr);
    jassert (airFreq != nullptr);
    jassert (compLink != nullptr);

    jassert (deEssAmount != nullptr);
    jassert (deEssFreqHz != nullptr);
    jassert (deEssWidth != nullptr);
    jassert (deEssListen != nullptr);
    jassert (airDb != nullptr);
    jassert (compAmount != nullptr);
    jassert (doubleAmount != nullptr);
    jassert (doubleDetuneCents != nullptr);
    jassert (doubleWidth != nullptr);
    jassert (mixPercent != nullptr);
    jassert (outputDb != nullptr);

    // M2 default resolution: user "Default" preset > factory "Default"
    // preset > the ParameterLayout defaults apvts was just constructed
    // with above (see PresetManager::applyStartupDefault()'s docs).
    presetManager.applyStartupDefault();
}

SeraphAudioProcessor::~SeraphAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SeraphAudioProcessor::createParameterLayout()
{
    return srph::createParameterLayout();
}

//==============================================================================
const juce::String SeraphAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SeraphAudioProcessor::acceptsMidi() const
{
    return false;
}

bool SeraphAudioProcessor::producesMidi() const
{
    return false;
}

bool SeraphAudioProcessor::isMidiEffect() const
{
    return false;
}

double SeraphAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SeraphAudioProcessor::getNumPrograms()
{
    return 1;
}

int SeraphAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SeraphAudioProcessor::setCurrentProgram (int)
{
}

const juce::String SeraphAudioProcessor::getProgramName (int)
{
    return {};
}

void SeraphAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void SeraphAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    // Seed the engine's parameters from the current APVTS state before
    // prepare() primes filter coefficients, so the very first block after
    // prepareToPlay() already reflects the host/session's actual parameter
    // values rather than the engine's built-in defaults.
    pushParametersToEngine();

    engine.prepare (spec);

    // v0.3.0: the chain can now report latency - the doubler's Shift mode is
    // a phase vocoder and the de-esser's lookahead is a real delay. In the
    // default configuration (Classic mode, no lookahead) this is still 0.
    // See docs/architecture.md.
    lastReportedLatencySamples = engine.getLatencySamples();
    setLatencySamples (lastReportedLatencySamples);
}

void SeraphAudioProcessor::pushParametersToEngine()
{
    engine.setDeEssAmountProportion (deEssAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDeEssFrequencyHz (deEssFreqHz->load (std::memory_order_relaxed));
    engine.setDeEssWidthProportion (deEssWidth->load (std::memory_order_relaxed) * 0.01f);
    engine.setDeEssListenEnabled (deEssListen->load (std::memory_order_relaxed) >= 0.5f);
    engine.setDeEssLinkEnabled (deEssLink->load (std::memory_order_relaxed) >= 0.5f);
    engine.setDeEssKneeDb (deEssKnee->load (std::memory_order_relaxed));
    engine.setDeEssLookaheadMs (deEssLookahead->load (std::memory_order_relaxed));
    engine.setAirDb (airDb->load (std::memory_order_relaxed));
    engine.setAirFrequencyChoice (static_cast<int> (std::lround (airFreq->load (std::memory_order_relaxed))));
    engine.setCompAmountProportion (compAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setCompLinkEnabled (compLink->load (std::memory_order_relaxed) >= 0.5f);
    engine.setDoubleAmountProportion (doubleAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDoubleDetuneCents (doubleDetuneCents->load (std::memory_order_relaxed));
    engine.setDoubleWidthProportion (doubleWidth->load (std::memory_order_relaxed) * 0.01f);
    engine.setDoubleMode (static_cast<Doubler::Mode> (
        juce::jlimit (0, 2, static_cast<int> (std::lround (doubleMode->load (std::memory_order_relaxed))))));
    engine.setDoubleHumanizeProportion (doubleHumanize->load (std::memory_order_relaxed) * 0.01f);
    engine.setDoubleFormantPreserveEnabled (doubleFormant->load (std::memory_order_relaxed) >= 0.5f);
    engine.setMixProportion (mixPercent->load (std::memory_order_relaxed) * 0.01f);
    engine.setOutputDb (outputDb->load (std::memory_order_relaxed));
}

void SeraphAudioProcessor::releaseResources()
{
}

void SeraphAudioProcessor::reset()
{
    engine.reset();
}

bool SeraphAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();

    if (mainOut != mono && mainOut != stereo)
        return false;

    if (mainOut != mainIn)
        return false;

    return true;
}

void SeraphAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Buses are constrained to in == out (mono or stereo), so this is
    // normally a no-op, but it's cheap insurance against stray channels.
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    pushParametersToEngine();

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    // Report a latency change as soon as it happens. setLatencySamples() is
    // safe to call from the audio thread in JUCE 8 (it forwards to the host's
    // own notification path); guarding on an actual change keeps it from
    // notifying the host every single block.
    const auto latencySamples = engine.getLatencySamples();

    if (latencySamples != lastReportedLatencySamples)
    {
        lastReportedLatencySamples = latencySamples;
        setLatencySamples (latencySamples);
    }
}

//==============================================================================
bool SeraphAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SeraphAudioProcessor::createEditor()
{
    return new SeraphAudioProcessorEditor (*this);
}

//==============================================================================
void SeraphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // State schema versioning, introduced in v0.3.0 (SOTA DSP brief ss4).
    // States written by v0.1.x/v0.2.0 carry no such attribute at all and are
    // therefore schema version 1 by absence; see setStateInformation() below
    // for why version 1 needs no value rewriting.
    state.setProperty (stateVersionProperty, stateSchemaVersion, nullptr);

    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

int SeraphAudioProcessor::readStateSchemaVersion (const juce::ValueTree& state) noexcept
{
    // Absent attribute => a pre-v0.3.0 state, which is schema version 1.
    return static_cast<int> (state.getProperty (stateVersionProperty, 1));
}

void SeraphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    // Migration from schema version 1 (v0.1.x/v0.2.0) is deliberately a no-op
    // beyond APVTS's own tolerant import: every parameter added in v0.3.0
    // defaults to the value that reproduces v0.2.0 behaviour exactly, and
    // replaceState() leaves a parameter's live value untouched when its PARAM
    // child is absent from the incoming state (JUCE 8.0.14,
    // updateParameterConnectionsToChildTrees()). So an old state lands on
    // neutral defaults for all eight new parameters with no value rewriting -
    // see tests/StateTests.cpp's migration case, which pins that contract.
    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeraphAudioProcessor();
}
