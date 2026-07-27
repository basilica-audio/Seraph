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
    // v0.3.0: 19 controls instead of 11, so the same grid grows from two rows
    // to four (6 + 6 + 6 + 1) rather than being redesigned.
    constexpr int numRows = 4;
    constexpr int comboBoxHeight = 24;
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
    configureKnob (deEssKneeKnob, ParamIDs::deEssKnee, "Knee");
    configureKnob (deEssLookaheadKnob, ParamIDs::deEssLookahead, "Lookahead");
    configureKnob (doubleHumanizeKnob, ParamIDs::doubleHumanize, "Humanize");

    configureChooser (doubleModeChooser, ParamIDs::doubleMode, "Mode");
    configureChooser (airFreqChooser, ParamIDs::airFreq, "Air Freq");

    configureToggle (deEssListenButton, deEssListenAttachment, ParamIDs::deEssListen, "Listen");
    configureToggle (deEssLinkButton, deEssLinkAttachment, ParamIDs::deEssLink, "DS Link");
    configureToggle (compLinkButton, compLinkAttachment, ParamIDs::compLink, "Comp Link");
    configureToggle (doubleFormantButton, doubleFormantAttachment, ParamIDs::doubleFormant, "Formant");

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

void SeraphAudioProcessorEditor::configureChooser (Chooser& chooser, const juce::String& parameterId, const juce::String& labelText)
{
    // ComboBoxAttachment binds an existing item list to the parameter - it
    // does NOT populate the box. Reading the choices off the parameter itself
    // keeps the two in sync automatically, with item IDs 1..N because JUCE
    // treats item ID 0 as "nothing selected".
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (parameterId)))
    {
        const auto& choices = parameter->choices;

        for (int index = 0; index < choices.size(); ++index)
            chooser.comboBox.addItem (choices[index], index + 1);
    }

    addAndMakeVisible (chooser.comboBox);

    chooser.label.setText (labelText, juce::dontSendNotification);
    chooser.label.setJustificationType (juce::Justification::centred);
    chooser.label.attachToComponent (&chooser.comboBox, false);
    addAndMakeVisible (chooser.label);

    chooser.attachment = std::make_unique<ComboBoxAttachment> (audioProcessor.apvts, parameterId, chooser.comboBox);
}

void SeraphAudioProcessorEditor::configureToggle (juce::ToggleButton& button,
                                                  std::unique_ptr<ButtonAttachment>& attachment,
                                                  const juce::String& parameterId,
                                                  const juce::String& buttonText)
{
    button.setButtonText (buttonText);
    addAndMakeVisible (button);
    attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, parameterId, button);
}

void SeraphAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);

    presetBar.setBounds (bounds.removeFromTop (presetBarHeight));
    bounds.removeFromTop (margin);

    const auto rowHeight = bounds.getHeight() / numRows;

    // Each row is laid out the same way: strip off the space the attached
    // labels occupy, then hand out equal-width slots left to right.
    auto makeRow = [&bounds, rowHeight]
    {
        auto row = bounds.removeFromTop (rowHeight);
        row.removeFromTop (labelHeight); // room for the attached labels above each control
        return row;
    };

    auto placeKnob = [] (juce::Rectangle<int>& row, int slotWidth, Knob& knob)
    {
        knob.slider.setBounds (row.removeFromLeft (slotWidth).reduced (margin / 2, 0));
    };

    // Toggles and combo boxes are much shorter than a knob, so they are
    // centred vertically inside their slot rather than stretched to fill it.
    auto placeCentred = [] (juce::Rectangle<int>& row, int slotWidth, juce::Component& component, int height)
    {
        auto slot = row.removeFromLeft (slotWidth).reduced (margin / 2, 0);
        component.setBounds (slot.withSizeKeepingCentre (slot.getWidth(), height));
    };

    const auto slotWidth = bounds.getWidth() / numColumns;

    // Row 1 - de-esser.
    {
        auto row = makeRow();
        placeKnob (row, slotWidth, deEssKnob);
        placeKnob (row, slotWidth, deEssFreqKnob);
        placeKnob (row, slotWidth, deEssWidthKnob);
        placeKnob (row, slotWidth, deEssKneeKnob);
        placeKnob (row, slotWidth, deEssLookaheadKnob);
        placeCentred (row, slotWidth, deEssListenButton, textBoxHeight);
    }

    // Row 2 - de-esser link, tone and dynamics.
    {
        auto row = makeRow();
        placeCentred (row, slotWidth, deEssLinkButton, textBoxHeight);
        placeKnob (row, slotWidth, airKnob);
        placeCentred (row, slotWidth, airFreqChooser.comboBox, comboBoxHeight);
        placeKnob (row, slotWidth, compKnob);
        placeCentred (row, slotWidth, compLinkButton, textBoxHeight);
        placeKnob (row, slotWidth, mixKnob);
    }

    // Row 3 - doubler.
    {
        auto row = makeRow();
        placeCentred (row, slotWidth, doubleModeChooser.comboBox, comboBoxHeight);
        placeKnob (row, slotWidth, doubleKnob);
        placeKnob (row, slotWidth, doubleDetuneKnob);
        placeKnob (row, slotWidth, doubleWidthKnob);
        placeKnob (row, slotWidth, doubleHumanizeKnob);
        placeCentred (row, slotWidth, doubleFormantButton, textBoxHeight);
    }

    // Row 4 - output.
    {
        auto row = makeRow();
        placeKnob (row, slotWidth, outputKnob);
    }
}
