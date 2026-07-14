#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class SeraphAudioProcessor;

// A simple, functional v0.1 editor: one rotary slider per parameter, bound
// to the APVTS via SliderAttachment, laid out in two rows of four. A custom
// vector-drawn GUI is a later milestone; this is deliberately plain but
// fully wired and usable.
class SeraphAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SeraphAudioProcessorEditor (SeraphAudioProcessor& processorToEdit);
    ~SeraphAudioProcessorEditor() override;

    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    // One knob + label per parameter, in signal-flow order.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText);

    SeraphAudioProcessor& audioProcessor;

    Knob deEssKnob;
    Knob deEssFreqKnob;
    Knob airKnob;
    Knob doubleKnob;
    Knob doubleDetuneKnob;
    Knob doubleWidthKnob;
    Knob mixKnob;
    Knob outputKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphAudioProcessorEditor)
};
