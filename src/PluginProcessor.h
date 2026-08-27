#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/SeraphEngine.h"
#include "presets/PresetManager.h"

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

    //==============================================================================
    // State schema versioning (v0.3.0, SOTA DSP brief ss4). getStateInformation()
    // stamps `stateVersionProperty` = stateSchemaVersion onto the APVTS root.
    // States written by v0.1.x/v0.2.0 carry no such attribute and are treated as
    // version 1 - see readStateSchemaVersion() and setStateInformation().
    static constexpr int stateSchemaVersion = 2;
    static inline const juce::Identifier stateVersionProperty { "stateVersion" };

    // Schema version of an APVTS state tree; 1 when the attribute is absent.
    static int readStateSchemaVersion (const juce::ValueTree& state) noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // M2 preset system (.scaffold/specs/preset-system-m2.md,
    // src/presets/PresetManager.h). Constructed after apvts (its
    // constructor registers APVTS parameter listeners) and public so
    // SeraphAudioProcessorEditor's PresetBar can talk to it directly - the
    // same "processor owns it, editor references it" pattern apvts itself
    // already uses.
    basilica::presets::PresetManager presetManager;

    // M3 meter surface (issue #4): the de-esser's and the compressor's most
    // recent per-block gain reduction in dB (positive = reduction),
    // refreshed from the engine at the end of every processBlock() call.
    // Safe to read from any thread; consumed by the editor's needle meters
    // via a GUI timer (see PluginEditor.cpp).
    float getDeEssGainReductionMeterDb() const noexcept { return deEssGainReductionMeterDb.load (std::memory_order_relaxed); }
    float getCompGainReductionMeterDb() const noexcept { return compGainReductionMeterDb.load (std::memory_order_relaxed); }

    // Wave-3 meter surface: the most recent block's per-channel output
    // peak (LINEAR gain, post-chain - measured on the buffer processBlock()
    // hands back to the host), refreshed every processBlock() call. RAW
    // linear peaks on purpose: the audio thread does two plain relaxed
    // stores and nothing else - the dB conversion, VU reference alignment
    // and ballistic smoothing are all GUI-side (see PluginEditor.cpp /
    // gui/AnalogMeter.h). Mono buses publish the single channel on both.
    float getOutputPeakLinearL() const noexcept { return outputPeakLinearL.load (std::memory_order_relaxed); }
    float getOutputPeakLinearR() const noexcept { return outputPeakLinearR.load (std::memory_order_relaxed); }

private:
    SeraphEngine engine;

    std::atomic<float> deEssGainReductionMeterDb { 0.0f };
    std::atomic<float> compGainReductionMeterDb { 0.0f };
    std::atomic<float> outputPeakLinearL { 0.0f };
    std::atomic<float> outputPeakLinearR { 0.0f };

    // Raw atomic pointers into the APVTS-managed parameter values, resolved
    // once at construction time so processBlock() never has to search for
    // them (no allocation/locks on the audio thread).
    std::atomic<float>* deEssAmount = nullptr;
    std::atomic<float>* deEssFreqHz = nullptr;
    std::atomic<float>* deEssWidth = nullptr;
    std::atomic<float>* deEssListen = nullptr;
    std::atomic<float>* airDb = nullptr;
    std::atomic<float>* compAmount = nullptr;
    std::atomic<float>* doubleAmount = nullptr;
    std::atomic<float>* doubleDetuneCents = nullptr;
    std::atomic<float>* doubleWidth = nullptr;
    std::atomic<float>* mixPercent = nullptr;
    std::atomic<float>* outputDb = nullptr;
    std::atomic<float>* doubleMode = nullptr;
    std::atomic<float>* doubleHumanize = nullptr;
    std::atomic<float>* doubleFormant = nullptr;
    std::atomic<float>* deEssLink = nullptr;
    std::atomic<float>* deEssKnee = nullptr;
    std::atomic<float>* deEssLookahead = nullptr;
    std::atomic<float>* airFreq = nullptr;
    std::atomic<float>* compLink = nullptr;

    // Pushes the current APVTS values into the engine. Shared by
    // prepareToPlay() and processBlock() so the two can never drift apart -
    // a parameter wired into one but not the other would silently ignore its
    // session value until the first block after a change.
    void pushParametersToEngine();

    // Last value handed to setLatencySamples(). The doubler's Shift mode and
    // the de-esser's lookahead both change reported latency, so this is
    // re-checked every block and reported only on an actual change; both
    // parameters are non-automatable, so in practice it fires once per
    // deliberate user action.
    int lastReportedLatencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphAudioProcessor)
};
