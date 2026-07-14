#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "params/ParameterIds.h"

namespace
{
    constexpr int knobSize = 90;
    constexpr int textBoxHeight = 20;
    constexpr int labelHeight = 20;
    constexpr int margin = 16;
    constexpr int numColumns = 4;
    constexpr int numRows = 2;
    constexpr int editorWidth = margin * 2 + numColumns * knobSize + (numColumns - 1) * margin;
    constexpr int editorHeight = margin * 2 + numRows * (labelHeight + knobSize + textBoxHeight) + margin;
}

SeraphAudioProcessorEditor::SeraphAudioProcessorEditor (SeraphAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      audioProcessor (processorToEdit)
{
    configureKnob (deEssKnob, ParamIDs::deEss, "De-Ess");
    configureKnob (deEssFreqKnob, ParamIDs::deEssFreq, "De-Ess Freq");
    configureKnob (airKnob, ParamIDs::air, "Air");
    configureKnob (doubleKnob, ParamIDs::doubleAmount, "Double");
    configureKnob (doubleDetuneKnob, ParamIDs::doubleDetune, "Detune");
    configureKnob (doubleWidthKnob, ParamIDs::doubleWidth, "Width");
    configureKnob (mixKnob, ParamIDs::mix, "Mix");
    configureKnob (outputKnob, ParamIDs::output, "Output");

    setResizable (false, false);
    setSize (editorWidth, editorHeight);
}

SeraphAudioProcessorEditor::~SeraphAudioProcessorEditor() = default;

void SeraphAudioProcessorEditor::configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobSize, textBoxHeight);
    addAndMakeVisible (knob.slider);

    knob.label.setText (labelText, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    // false => label sits above the slider it tracks; JUCE repositions it
    // automatically whenever the slider's bounds change, so resized() only
    // needs to place the sliders themselves.
    knob.label.attachToComponent (&knob.slider, false);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, parameterId, knob.slider);
}

void SeraphAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);

    const auto rowHeight = bounds.getHeight() / numRows;
    auto topRow = bounds.removeFromTop (rowHeight);
    auto bottomRow = bounds;

    topRow.removeFromTop (labelHeight); // room for the attached labels above each knob
    bottomRow.removeFromTop (labelHeight);

    const auto slotWidth = topRow.getWidth() / numColumns;

    for (auto* knob : { &deEssKnob, &deEssFreqKnob, &airKnob, &doubleKnob })
        knob->slider.setBounds (topRow.removeFromLeft (slotWidth).reduced (margin / 2, 0));

    for (auto* knob : { &doubleDetuneKnob, &doubleWidthKnob, &mixKnob, &outputKnob })
        knob->slider.setBounds (bottomRow.removeFromLeft (slotWidth).reduced (margin / 2, 0));
}
