#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "presets/PresetBar.h"

class SeraphAudioProcessor;

// A simple, functional v0.1/v0.2 editor: one rotary slider per float
// parameter plus a toggle button for the boolean De-Ess Listen parameter,
// bound to the APVTS via SliderAttachment/ButtonAttachment, laid out in two
// rows of six, plus a preset bar docked at the top (M2 preset system). A
// custom vector-drawn GUI is a later milestone; this is deliberately plain
// but fully wired and usable - "M3 restyles it, do not gold-plate" per
// .scaffold/specs/preset-system-m2.md.
class SeraphAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SeraphAudioProcessorEditor (SeraphAudioProcessor& processorToEdit);
    ~SeraphAudioProcessorEditor() override;

    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // One knob + label per float parameter, in signal-flow order.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText);

    SeraphAudioProcessor& audioProcessor;

    // M2 preset system (src/presets/PresetBar.h) - a horizontal strip docked
    // at the top of the editor. Constructed after the localisation frame is
    // installed (see the constructor) so its TRANS()'d strings pick up the
    // right language from the very first paint.
    basilica::presets::PresetBar presetBar;

    Knob deEssKnob;
    Knob deEssFreqKnob;
    Knob deEssWidthKnob;
    Knob airKnob;
    Knob compKnob;
    Knob doubleKnob;
    Knob doubleDetuneKnob;
    Knob doubleWidthKnob;
    Knob mixKnob;
    Knob outputKnob;

    juce::ToggleButton deEssListenButton;
    std::unique_ptr<ButtonAttachment> deEssListenAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphAudioProcessorEditor)
};
