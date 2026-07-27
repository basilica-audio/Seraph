#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "presets/PresetBar.h"

class SeraphAudioProcessor;

// A simple, functional editor: one rotary slider per float parameter, a
// toggle button per boolean and a combo box per choice parameter, bound to
// the APVTS via Slider/Button/ComboBoxAttachment, laid out in rows of six,
// plus a preset bar docked at the top (M2 preset system). v0.3.0 extends the
// same generic grid from 11 to 19 controls rather than redesigning it - a
// custom vector-drawn GUI is a later milestone, and "M3 restyles it, do not
// gold-plate" per .scaffold/specs/preset-system-m2.md.
class SeraphAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SeraphAudioProcessorEditor (SeraphAudioProcessor& processorToEdit);
    ~SeraphAudioProcessorEditor() override;

    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // One knob + label per float parameter, in signal-flow order.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    // One combo box + label per choice parameter.
    struct Chooser
    {
        juce::ComboBox comboBox;
        juce::Label label;
        std::unique_ptr<ComboBoxAttachment> attachment;
    };

    void configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText);
    void configureChooser (Chooser& chooser, const juce::String& parameterId, const juce::String& labelText);
    void configureToggle (juce::ToggleButton& button,
                          std::unique_ptr<ButtonAttachment>& attachment,
                          const juce::String& parameterId,
                          const juce::String& buttonText);

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
    Knob deEssKneeKnob;
    Knob deEssLookaheadKnob;
    Knob doubleHumanizeKnob;

    Chooser doubleModeChooser;
    Chooser airFreqChooser;

    juce::ToggleButton deEssListenButton;
    std::unique_ptr<ButtonAttachment> deEssListenAttachment;
    juce::ToggleButton deEssLinkButton;
    std::unique_ptr<ButtonAttachment> deEssLinkAttachment;
    juce::ToggleButton compLinkButton;
    std::unique_ptr<ButtonAttachment> compLinkAttachment;
    juce::ToggleButton doubleFormantButton;
    std::unique_ptr<ButtonAttachment> doubleFormantAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphAudioProcessorEditor)
};
