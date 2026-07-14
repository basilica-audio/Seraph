#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/SeraphEngine.h"

// Seraph: a choir/vocal processor for operatic vocals (de-esser, "Air"
// high-shelf, and a click-free two-voice doubler). Signal flow lives in
// SeraphEngine (src/dsp) so it stays unit-testable independent of this
// AudioProcessor; this class is just APVTS + host plumbing around it.
class SeraphAudioProcessor final : public juce::AudioProcessor
{
public:
    SeraphAudioProcessor();
    ~SeraphAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    SeraphEngine engine;

    // Raw atomic pointers into the APVTS-managed parameter values, resolved
    // once at construction time so processBlock() never has to search for
    // them (no allocation/locks on the audio thread).
    std::atomic<float>* deEssAmount = nullptr;
    std::atomic<float>* deEssFreqHz = nullptr;
    std::atomic<float>* deEssListen = nullptr;
    std::atomic<float>* airDb = nullptr;
    std::atomic<float>* compAmount = nullptr;
    std::atomic<float>* doubleAmount = nullptr;
    std::atomic<float>* doubleDetuneCents = nullptr;
    std::atomic<float>* doubleWidth = nullptr;
    std::atomic<float>* mixPercent = nullptr;
    std::atomic<float>* outputDb = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphAudioProcessor)
};
