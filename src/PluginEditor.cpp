#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "presets/Localisation.h"

#include <BinaryData.h>

#include <algorithm>

namespace
{
    // ----- M3 vector-editor layout metrics (issue #4) ---------------------
    // All values are design constants, not measurements of pre-rendered
    // art (there is none): the editor computes its own size from these plus
    // the control tables in the constructor, and tests/gui/EditorLayoutTests.cpp
    // asserts the resulting geometry (containment, no overlap) on the real
    // component tree, so a change here can never silently clip a control.
    constexpr int outerMargin = 10;
    constexpr int presetBarHeight = 30;
    constexpr int bandGap = 8;

    constexpr int panelPadding = 10;
    constexpr int panelBottomPadding = 8;
    constexpr int rowGap = 8;

    // A knob slot: attached label above (JUCE 8.0.14 Label::
    // componentMovedOrResized sizes an above-attached label to
    // borderTopAndBottom + 6 + fontHeight ~ 22 px for the 14 px suite
    // serif, so 24 reserved keeps it clear of the row above), then the
    // rotary area, then the value box baked into the slider's own bounds.
    constexpr int labelHeight = 24;
    constexpr int knobSize = 60;
    constexpr int textBoxHeight = 16;
    constexpr int knobSlotWidth = 80;
    constexpr int toggleSlotWidth = 70;
    constexpr int toggleHeight = 32;
    constexpr int slotGap = 6; // trimmed off the right of every slot
    constexpr int rowHeight = labelHeight + knobSize + textBoxHeight;

    // Right-hand meter bay on the two gain-reducing panels.
    constexpr int meterBayWidth = 150;
    constexpr int meterWidth = 134;
    constexpr int meterHeight = 96;

    // M2 i18n frame: selects German (resources/i18n/de.txt) or falls
    // through to English, once, at editor construction - see
    // Localisation.h's docs. `presetBar` is a member initialised via the
    // constructor's initialiser list, and its own constructor already calls
    // TRANS() on every button label - member initialisers run in
    // declaration order, so this helper (called from presetBar's own
    // initialiser expression below) is what guarantees installLocalisation()
    // runs before presetBar exists, not a call in the constructor *body*,
    // which would run too late.
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
    // Propagates to every child, including the preset bar's stock buttons
    // and any menus/dialogs they open.
    setLookAndFeel (&lookAndFeel);

    // FOCUS ORDER (WCAG 2.4.3): children are created and added in signal-
    // flow/reading order - preset bar, then De-Ess, Air, Compressor,
    // Doubler, Output, left-to-right within each row. JUCE's default
    // traverser follows this creation order; do not reorder.
    addAndMakeVisible (presetBar);

    // --- De-Ess: sibilance control, first in the chain --------------------
    auto& deEss = addPanel ("De-Ess");
    deEssPanel = &deEss;
    addKnob (deEss, ParamIDs::deEss, "De-Ess");
    addKnob (deEss, ParamIDs::deEssFreq, "Freq");
    addKnob (deEss, ParamIDs::deEssWidth, "Width");
    addKnob (deEss, ParamIDs::deEssKnee, "Knee");
    addKnob (deEss, ParamIDs::deEssLookahead, "Lookahead");

    addRow (deEss);
    addToggle (deEss, ParamIDs::deEssListen, "Listen");
    addToggle (deEss, ParamIDs::deEssLink, "Link");

    addMeter (deEss, "De-Ess gain reduction meter", "ESS");

    // --- Air: the high-shelf openness stage -------------------------------
    auto& air = addPanel ("Air");
    airPanel = &air;
    addKnob (air, ParamIDs::air, "Air");
    addKnob (air, ParamIDs::airFreq, "Shelf");

    // --- Compressor: gentle broadband glue --------------------------------
    auto& comp = addPanel ("Compressor");
    compPanel = &comp;
    addKnob (comp, ParamIDs::comp, "Comp");
    addToggle (comp, ParamIDs::compLink, "Link");

    addMeter (comp, "Compressor gain reduction meter", "COMP");

    // --- Doubler: the four-voice detune/spread stage ----------------------
    auto& doubler = addPanel ("Doubler");
    doublerPanel = &doubler;
    addKnob (doubler, ParamIDs::doubleMode, "Mode");
    addKnob (doubler, ParamIDs::doubleAmount, "Amount");
    addKnob (doubler, ParamIDs::doubleDetune, "Detune");
    addKnob (doubler, ParamIDs::doubleWidth, "Width");
    addKnob (doubler, ParamIDs::doubleHumanize, "Humanize");
    addToggle (doubler, ParamIDs::doubleFormant, "Formant");

    // --- Output: dry/wet mix + trim ---------------------------------------
    auto& output = addPanel ("Output");
    outputPanel = &output;
    addKnob (output, ParamIDs::mix, "Mix");
    addKnob (output, ParamIDs::output, "Output");

    // --- Size: computed from the control tables above ---------------------
    const auto contentWidth = std::max ({ panelRequiredWidth (deEss),
                                          panelRequiredWidth (air) + bandGap + panelRequiredWidth (comp),
                                          panelRequiredWidth (doubler),
                                          panelRequiredWidth (output) });

    const auto contentHeight = presetBarHeight + bandGap
                             + panelRequiredHeight (deEss) + bandGap
                             + std::max (panelRequiredHeight (air), panelRequiredHeight (comp)) + bandGap
                             + panelRequiredHeight (doubler) + bandGap
                             + panelRequiredHeight (output);

    setResizable (false, false);
    setSize (outerMargin * 2 + contentWidth, outerMargin * 2 + contentHeight);

    // GR meter polling: ~30 Hz GUI-thread timer feeding the ballistic
    // needles; the processor getters are relaxed-atomic loads, so this
    // never touches the audio thread.
    startTimerHz (30);
}

SeraphAudioProcessorEditor::~SeraphAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

SeraphAudioProcessorEditor::Panel& SeraphAudioProcessorEditor::addPanel (const juce::String& stageTitle)
{
    auto panel = std::make_unique<Panel>();
    panel->component = std::make_unique<basilica::gui::BusPanel> (stageTitle);
    panel->rows.emplace_back();

    addAndMakeVisible (*panel->component);

    panels.push_back (std::move (panel));
    return *panels.back();
}

void SeraphAudioProcessorEditor::addRow (Panel& panel)
{
    panel.rows.emplace_back();
}

SeraphAudioProcessorEditor::Knob& SeraphAudioProcessorEditor::addKnob (Panel& panel, const char* parameterId,
                                                                       const juce::String& labelText)
{
    auto knob = std::make_unique<Knob>();

    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobSlotWidth - slotGap, textBoxHeight);
    knob->slider.setTitle (labelText);
    knob->slider.setName (labelText);
    panel.component->addAndMakeVisible (knob->slider);

    knob->label.setText (labelText, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.attachToComponent (&knob->slider, false); // above; auto-repositions with the slider
    panel.component->addAndMakeVisible (knob->label);

    // SliderAttachment MUST be constructed before the textFromValueFunction
    // override below, not after: JUCE 8.0.14's SliderParameterAttachment
    // constructor (juce_ParameterAttachments.cpp:128) itself assigns
    // `slider.textFromValueFunction` as part of wiring the attachment -
    // setting our own function BEFORE this point would be silently
    // clobbered the moment the attachment is created.
    knob->attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, parameterId, knob->slider);

    if (auto* param = audioProcessor.apvts.getParameter (parameterId))
    {
        // A-02 pattern: unit-carrying parameters declare their unit via
        // .withLabel() in ParameterLayout.cpp (%/Hz/dB/ms/cents) - feed it
        // into both the value box and the accessibility value string.
        // Choice parameters have an empty label and getText() already
        // returns the choice NAME, so this is a no-op suffix for them.
        knob->slider.textFromValueFunction = [param] (double v)
        {
            const auto text = param->getText (param->convertTo0to1 ((float) v), 0);
            const auto unit = param->getLabel();
            return unit.isEmpty() ? text : text + " " + unit;
        };
        knob->slider.updateText();
    }

    panel.rows.back().push_back (&knob->slider);
    knobs.push_back (std::move (knob));
    return *knobs.back();
}

SeraphAudioProcessorEditor::Toggle& SeraphAudioProcessorEditor::addToggle (Panel& panel, const char* parameterId,
                                                                           const juce::String& labelText)
{
    auto toggle = std::make_unique<Toggle>();

    // Real juce::ToggleButton on purpose: focusable and Space/Enter-
    // operable by default, and its createAccessibilityHandler() reports
    // AccessibilityRole::toggleButton (JUCE 8.0.14 juce_ToggleButton.cpp:71)
    // so it lands in the VoiceOver rotor as a toggle, not a plain button.
    toggle->button.setButtonText (labelText);
    toggle->button.setTitle (labelText);
    toggle->button.setName (labelText);
    panel.component->addAndMakeVisible (toggle->button);

    toggle->attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, parameterId, toggle->button);

    panel.rows.back().push_back (&toggle->button);
    toggles.push_back (std::move (toggle));
    return *toggles.back();
}

basilica::gui::NeedleMeter& SeraphAudioProcessorEditor::addMeter (Panel& panel, const juce::String& accessibleTitle,
                                                                  const juce::String& faceLegend)
{
    auto meter = std::make_unique<basilica::gui::NeedleMeter> (accessibleTitle, faceLegend);
    panel.component->addAndMakeVisible (*meter);
    panel.meter = meter.get();

    meters.push_back (std::move (meter));
    return *meters.back();
}

void SeraphAudioProcessorEditor::timerCallback()
{
    // Positive dB of gain reduction, straight from the engine's per-block
    // metering (relaxed atomic reads - see PluginProcessor.h).
    if (deEssPanel != nullptr && deEssPanel->meter != nullptr)
        deEssPanel->meter->setTargetDb (audioProcessor.getDeEssGainReductionMeterDb());

    if (compPanel != nullptr && compPanel->meter != nullptr)
        compPanel->meter->setTargetDb (audioProcessor.getCompGainReductionMeterDb());

    constexpr float dtSeconds = 1.0f / 30.0f;

    for (auto& meter : meters)
        meter->tick (dtSeconds);
}

int SeraphAudioProcessorEditor::slotWidthFor (const juce::Component& control) noexcept
{
    return dynamic_cast<const juce::Slider*> (&control) != nullptr ? knobSlotWidth : toggleSlotWidth;
}

int SeraphAudioProcessorEditor::rowWidth (const std::vector<juce::Component*>& row) noexcept
{
    int width = 0;

    for (const auto* control : row)
        width += slotWidthFor (*control);

    return width;
}

int SeraphAudioProcessorEditor::panelRequiredWidth (const Panel& panel) const noexcept
{
    int widest = 0;

    for (const auto& row : panel.rows)
        widest = std::max (widest, rowWidth (row));

    return panelPadding * 2 + widest + (panel.meter != nullptr ? meterBayWidth : 0);
}

int SeraphAudioProcessorEditor::panelRequiredHeight (const Panel& panel) const noexcept
{
    const auto numRows = (int) panel.rows.size();
    return basilica::gui::BusPanel::headerHeight
         + numRows * rowHeight + (numRows - 1) * rowGap
         + panelBottomPadding;
}

void SeraphAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (basilica::gui::BasilicaLookAndFeel::getEditorBackgroundColour());
}

void SeraphAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (outerMargin);

    presetBar.setBounds (bounds.removeFromTop (presetBarHeight));
    bounds.removeFromTop (bandGap);

    const auto layoutPanel = [] (Panel& panel, juce::Rectangle<int> area)
    {
        panel.component->setBounds (area);

        auto content = panel.component->getLocalBounds().reduced (panelPadding, 0);
        content.removeFromTop (basilica::gui::BusPanel::headerHeight);

        if (panel.meter != nullptr)
        {
            auto bay = content.removeFromRight (meterBayWidth);
            panel.meter->setBounds (juce::Rectangle<int> (meterWidth,
                                                          juce::jmin (meterHeight, bay.getHeight()))
                                        .withCentre (bay.getCentre()));
        }

        for (auto& row : panel.rows)
        {
            auto rowArea = content.removeFromTop (rowHeight);
            rowArea.removeFromTop (labelHeight); // attached labels position themselves here

            for (auto* control : row)
            {
                auto slot = rowArea.removeFromLeft (slotWidthFor (*control)).withTrimmedRight (slotGap);

                if (dynamic_cast<juce::Slider*> (control) != nullptr)
                    control->setBounds (slot.withHeight (knobSize + textBoxHeight));
                else
                    control->setBounds (slot.withSizeKeepingCentre (slot.getWidth(), toggleHeight)
                                            .withY (rowArea.getY() + (knobSize - toggleHeight) / 2));
            }

            content.removeFromTop (rowGap);
        }
    };

    layoutPanel (*deEssPanel, bounds.removeFromTop (panelRequiredHeight (*deEssPanel)));
    bounds.removeFromTop (bandGap);

    auto midBand = bounds.removeFromTop (std::max (panelRequiredHeight (*airPanel),
                                                   panelRequiredHeight (*compPanel)));
    layoutPanel (*airPanel, midBand.removeFromLeft (panelRequiredWidth (*airPanel)));
    midBand.removeFromLeft (bandGap);
    layoutPanel (*compPanel, midBand);

    bounds.removeFromTop (bandGap);
    layoutPanel (*doublerPanel, bounds.removeFromTop (panelRequiredHeight (*doublerPanel)));
    bounds.removeFromTop (bandGap);
    layoutPanel (*outputPanel, bounds.removeFromTop (panelRequiredHeight (*outputPanel)));
}
