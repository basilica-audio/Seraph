#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterIds.h"
#include "params/ParameterLayout.h"

//==============================================================================
SeraphAudioProcessor::SeraphAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    deEssAmount = apvts.getRawParameterValue (ParamIDs::deEss);
    deEssFreqHz = apvts.getRawParameterValue (ParamIDs::deEssFreq);
    deEssListen = apvts.getRawParameterValue (ParamIDs::deEssListen);
    airDb = apvts.getRawParameterValue (ParamIDs::air);
    compAmount = apvts.getRawParameterValue (ParamIDs::comp);
    doubleAmount = apvts.getRawParameterValue (ParamIDs::doubleAmount);
    doubleDetuneCents = apvts.getRawParameterValue (ParamIDs::doubleDetune);
    doubleWidth = apvts.getRawParameterValue (ParamIDs::doubleWidth);
    mixPercent = apvts.getRawParameterValue (ParamIDs::mix);
    outputDb = apvts.getRawParameterValue (ParamIDs::output);

    jassert (deEssAmount != nullptr);
    jassert (deEssFreqHz != nullptr);
    jassert (deEssListen != nullptr);
    jassert (airDb != nullptr);
    jassert (compAmount != nullptr);
    jassert (doubleAmount != nullptr);
    jassert (doubleDetuneCents != nullptr);
    jassert (doubleWidth != nullptr);
    jassert (mixPercent != nullptr);
    jassert (outputDb != nullptr);
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
    engine.setDeEssAmountProportion (deEssAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDeEssFrequencyHz (deEssFreqHz->load (std::memory_order_relaxed));
    engine.setDeEssListenEnabled (deEssListen->load (std::memory_order_relaxed) >= 0.5f);
    engine.setAirDb (airDb->load (std::memory_order_relaxed));
    engine.setCompAmountProportion (compAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDoubleAmountProportion (doubleAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDoubleDetuneCents (doubleDetuneCents->load (std::memory_order_relaxed));
    engine.setDoubleWidthProportion (doubleWidth->load (std::memory_order_relaxed) * 0.01f);
    engine.setMixProportion (mixPercent->load (std::memory_order_relaxed) * 0.01f);
    engine.setOutputDb (outputDb->load (std::memory_order_relaxed));

    engine.prepare (spec);

    // Nothing in the chain (de-esser, Air shelf, doubler) adds reported host
    // latency - the doubler's short delay lines are the effect itself, not
    // a compensation delay (see docs/architecture.md).
    setLatencySamples (engine.getLatencySamples());
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

    engine.setDeEssAmountProportion (deEssAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDeEssFrequencyHz (deEssFreqHz->load (std::memory_order_relaxed));
    engine.setDeEssListenEnabled (deEssListen->load (std::memory_order_relaxed) >= 0.5f);
    engine.setAirDb (airDb->load (std::memory_order_relaxed));
    engine.setCompAmountProportion (compAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDoubleAmountProportion (doubleAmount->load (std::memory_order_relaxed) * 0.01f);
    engine.setDoubleDetuneCents (doubleDetuneCents->load (std::memory_order_relaxed));
    engine.setDoubleWidthProportion (doubleWidth->load (std::memory_order_relaxed) * 0.01f);
    engine.setMixProportion (mixPercent->load (std::memory_order_relaxed) * 0.01f);
    engine.setOutputDb (outputDb->load (std::memory_order_relaxed));

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);
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
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SeraphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeraphAudioProcessor();
}
