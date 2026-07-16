#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "presets/Localisation.h"

#include <BinaryData.h>

namespace
{
    constexpr int knobSize = 90;
    constexpr int textBoxHeight = 20;
    constexpr int labelHeight = 20;
    constexpr int margin = 16;
    constexpr int numColumns = 6;
    constexpr int numRows = 2;
    constexpr int presetBarHeight = 28;
    constexpr int editorWidth = margin * 2 + numColumns * knobSize + (numColumns - 1) * margin;
    constexpr int editorHeight = margin * 3 + presetBarHeight + numRows * (labelHeight + knobSize + textBoxHeight) + margin;

    // M2 i18n frame (.scaffold/specs/preset-system-m2.md): selects German
    // (resources/i18n/de.txt) or falls through to English, once, at editor
    // construction - see Localisation.h's docs. `presetBar` is a member
    // initialised via the constructor's initialiser list, and its own
    // constructor already calls TRANS() on every button label - member
    // initialisers run in declaration order regardless of the order they're
    // written in, so this helper (called from presetBar's own initialiser
    // expression below) is what actually guarantees installLocalisation()
    // runs before presetBar exists, not a call in the constructor *body*,
    // which would run too late. See sibling plugin nave's PluginEditor.cpp
    // for the same pattern (the M2 pilot).
    basilica::presets::PresetManager& initLocalisationThenGetPresetManager (SeraphAudioProcessor& processor)
    {
        basilica::presets::installLocalisation (BinaryData::de_txt, BinaryData::de_txtSize);
        return processor.presetManager;
    }
}

SeraphAudioProcessorEditor::SeraphAudioProcessorEditor (SeraphAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      audioProcessor (processorToEdit),
      presetBar (initLocalisationThenGetPresetManager (processorToEdit))
{
    addAndMakeVisible (presetBar);

    configureKnob (deEssKnob, ParamIDs::deEss, "De-Ess");
    configureKnob (deEssFreqKnob, ParamIDs::deEssFreq, "De-Ess Freq");
    configureKnob (deEssWidthKnob, ParamIDs::deEssWidth, "De-Ess Width");
    configureKnob (airKnob, ParamIDs::air, "Air");
    configureKnob (compKnob, ParamIDs::comp, "Comp");
    configureKnob (doubleKnob, ParamIDs::doubleAmount, "Double");
    configureKnob (doubleDetuneKnob, ParamIDs::doubleDetune, "Detune");
    configureKnob (doubleWidthKnob, ParamIDs::doubleWidth, "Width");
    configureKnob (mixKnob, ParamIDs::mix, "Mix");
    configureKnob (outputKnob, ParamIDs::output, "Output");

    deEssListenButton.setButtonText ("Listen");
    addAndMakeVisible (deEssListenButton);
    deEssListenAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::deEssListen, deEssListenButton);

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

    presetBar.setBounds (bounds.removeFromTop (presetBarHeight));
    bounds.removeFromTop (margin);

    const auto rowHeight = bounds.getHeight() / numRows;
    auto topRow = bounds.removeFromTop (rowHeight);
    auto bottomRow = bounds;

    topRow.removeFromTop (labelHeight); // room for the attached labels above each knob
    bottomRow.removeFromTop (labelHeight);

    const auto slotWidth = topRow.getWidth() / numColumns;

    for (auto* knob : { &deEssKnob, &deEssFreqKnob, &deEssWidthKnob })
        knob->slider.setBounds (topRow.removeFromLeft (slotWidth).reduced (margin / 2, 0));

    deEssListenButton.setBounds (topRow.removeFromLeft (slotWidth).reduced (margin / 2, knobSize / 2 - textBoxHeight / 2));

    for (auto* knob : { &airKnob, &compKnob })
        knob->slider.setBounds (topRow.removeFromLeft (slotWidth).reduced (margin / 2, 0));

    for (auto* knob : { &doubleKnob, &doubleDetuneKnob, &doubleWidthKnob, &mixKnob, &outputKnob })
        knob->slider.setBounds (bottomRow.removeFromLeft (slotWidth).reduced (margin / 2, 0));
}
