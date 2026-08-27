#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

#include "gui/AnalogMeter.h"
#include "gui/LayoutManifest.h"
#include "gui/MasterCropKnob.h"
#include "gui/PlateTypography.h"
#include "gui/SpriteToggle.h"
#include "presets/PresetBar.h"

class SeraphAudioProcessor;

// Wave-3 COMPOSITIONAL photoreal editor (campaign 2026-08, supersedes the
// M3 vector editor - BusPanel/PointerKnob/NeedleMeter stay in the tree per
// the suite's "superseded, not deleted" convention but are no longer
// used): the accepted EMPTY family plate render
// (resources/gui/plate_seraph.png) is the sole baked background, and every
// control is composited live from the extracted control-sprite library at
// the coordinates in resources/gui/layout_manifest.json (the single source
// of truth - see gui/LayoutManifest.h). Draw order:
//
//   1. plate render (paint())
//   2. static control sprites - knob bodies and the two needle-free VU
//      dial faces at their manifest positions (paint(), under the
//      children)
//   3. engraved lettering - PlateTypography, gilded gold on the dark
//      basalt (paint(), after the sprites so labels sit on top of each
//      sprite's feathered basalt patch)
//   4. rotating cap crops - one MasterCropKnob child per knob, rotating a
//      feathered circular crop of its own sprite's cap (the suite
//      INNER-DISC technique: rim + housing stay static underneath)
//   5. lever toggle - SpriteToggle child (up = ON, mirrored = OFF; see
//      SpriteToggle.h's asset-gap docs)
//   6. needle overlays - one AnalogMeter child per VU dial (glow +
//      master-extracted needle sprite rotated live from the processor's
//      output-peak atomics; ballistics on the GUI timer, NADEL-REGEL
//      compliant - never baked, never hand-drawn)
//
// Seraph-specific control set (rollout-2026-07/seraph/control-inventory.md):
// 10 continuous knobs in 2 rows of 5 (row 2 staggered), the deEssListen
// lever, and 2 VU dials reading post-chain stereo L/R output level - the
// one point in Seraph's chain where left and right genuinely diverge (the
// doubler's panned voices), so the pair shows real DSP state, not dead
// decoration.
//
// Window scaling is STEPPED (100/150/200%, UA-style corner control,
// persisted as a plain property on the APVTS state tree), matching every
// merged M3 editor in the suite.
class SeraphAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit SeraphAudioProcessorEditor (SeraphAudioProcessor& processorToEdit);
    ~SeraphAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // The parsed layout manifest - exposed read-only so tests assert
    // layout invariants against the exact data this editor composites
    // from (tests/gui/EditorLayoutTests.cpp).
    const basilica::gui::LayoutManifest& layoutManifest() const noexcept { return manifest; }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        const basilica::gui::ManifestControl* entry = nullptr;
        std::unique_ptr<basilica::gui::MasterCropKnob> slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Toggle
    {
        const basilica::gui::ManifestControl* entry = nullptr;
        std::unique_ptr<basilica::gui::SpriteToggle> button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct Meter
    {
        const basilica::gui::ManifestControl* entry = nullptr;
        std::unique_ptr<basilica::gui::AnalogMeter> component;
    };

    juce::Image spriteImageFor (const juce::String& spriteKey) const;
    void buildControlsFromManifest();
    void applyScaleStep (int newStepIndex);
    void cycleScale();
    void drawStaticSprites (juce::Graphics& g) const;
    void drawPlateLettering (juce::Graphics& g) const;
    void timerCallback() override;

    // plate-render px -> screen px for the current scale step, and the
    // plate's top-left corner in screen px.
    float plateScale() const noexcept;
    juce::Point<float> plateOrigin() const noexcept;

    SeraphAudioProcessor& audioProcessor;

    basilica::gui::LayoutManifest manifest;

    juce::Image plateImage;
    juce::Image knobSprite, toggleSprite, vuDialSprite, needleSprite;

    basilica::presets::PresetBar presetBar;
    juce::TextButton scaleButton;
    int scaleStepIndex = 0; // 0 = 100%, 1 = 150%, 2 = 200%

    std::vector<Knob> knobs;
    std::vector<Toggle> toggles;
    std::vector<Meter> meters;

    basilica::gui::PlateTypography typography;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphAudioProcessorEditor)
};
