#include "PluginEditor.h"
#include "PluginEditorLayout.h"
#include "PluginProcessor.h"

#include <BinaryData.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

// Wave-3 compositional-layout invariants, asserted against the SAME parsed
// manifest the editor composites from (PluginEditor::layoutManifest() /
// gui/LayoutManifest.h) - never a second hand-maintained coordinate list.
// The expected control census comes from the rollout control inventory
// (.scaffold/gui-assets/rollout-2026-07/seraph/control-inventory.md):
// 10 knobs (2 rows of 5, row 2 staggered), 1 toggle, 2 VU dial meters,
// 0 selectors.
namespace
{
    basilica::gui::LayoutManifest parseManifest()
    {
        return basilica::gui::LayoutManifest::parse (BinaryData::layout_manifest_json,
                                                     BinaryData::layout_manifest_jsonSize);
    }

    // The plate's usable control field (inside the gold pinstripe border),
    // measured on plate_seraph.png - same family plate geometry the
    // overture wave-3 measurement log established. Slightly conservative
    // on purpose.
    constexpr float fieldLeft = 85.0f, fieldRight = 1162.0f;
    constexpr float fieldTop = 85.0f, fieldBottom = 768.0f;

    // Baked central divider flourish (family plate: y ~448..459,
    // x ~510..740) - no control cap may cover it.
    const juce::Rectangle<float> dividerKeepOut (500.0f, 444.0f, 250.0f, 20.0f);

    // Baked amber vent grilles (family plate, measured): no control cap
    // may intrude into either grille.
    const juce::Rectangle<float> ventLeftKeepOut (150.0f, 488.0f, 142.0f, 195.0f);
    const juce::Rectangle<float> ventRightKeepOut (963.0f, 488.0f, 142.0f, 195.0f);

    float capRadiusPlatePx (const basilica::gui::ManifestControl& control)
    {
        using namespace srph::layout;

        if (control.kind == "toggle")
            return 48.0f * control.scale; // housing half-height, conservative

        if (control.kind == "meter")
            return meterBezelRadiusPlatePx * control.scale;

        return knobCapRadius * control.scale;
    }
}

TEST_CASE ("Manifest parses and matches the rollout control inventory census", "[gui][layout]")
{
    const auto manifest = parseManifest();

    REQUIRE (manifest.isValid());
    CHECK (manifest.plateWidthPx == srph::layout::plateCanvasWidthPx);
    CHECK (manifest.plateHeightPx == srph::layout::plateCanvasHeightPx);

    CHECK (manifest.ofKind ("knob").size() == 10);
    CHECK (manifest.ofKind ("selector").empty()); // no choice params surface this generation
    CHECK (manifest.ofKind ("toggle").size() == 1);
    CHECK (manifest.ofKind ("meter").size() == 2); // stereo L/R output VU pair
    CHECK (manifest.controls.size() == 13);
}

TEST_CASE ("Every non-meter manifest control id resolves to a real APVTS parameter of the right type", "[gui][layout]")
{
    const auto manifest = parseManifest();
    REQUIRE (manifest.isValid());

    SeraphAudioProcessor processor;

    for (const auto& control : manifest.controls)
    {
        if (control.kind == "meter")
            continue; // meter ids are editor-defined display elements, not parameters

        auto* parameter = processor.apvts.getParameter (control.id);
        INFO ("manifest id \"" << control.id.toStdString() << "\"");
        REQUIRE (parameter != nullptr);

        if (control.kind == "toggle")
            CHECK (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr);
        else if (control.kind == "knob")
            CHECK (dynamic_cast<juce::AudioParameterFloat*> (parameter) != nullptr);
    }
}

TEST_CASE ("Knob rows follow the 5+5 staggered family signature", "[gui][layout]")
{
    const auto manifest = parseManifest();
    REQUIRE (manifest.isValid());

    std::map<float, std::vector<float>> rows; // cy -> sorted cx list

    for (const auto* knob : manifest.ofKind ("knob"))
        rows[knob->cy].push_back (knob->cx);

    REQUIRE (rows.size() == 2);

    auto it = rows.begin();
    auto& row1 = it->second;
    auto& row2 = std::next (it)->second;

    CHECK (row1.size() == 5);
    CHECK (row2.size() == 5);

    for (auto* row : { &row1, &row2 })
    {
        std::sort (row->begin(), row->end());

        // Uniform spacing within a row (the LAYOUT-INVARIANTE: same-role
        // elements share a common axis and even rhythm).
        for (size_t i = 2; i < row->size(); ++i)
            CHECK (std::abs (((*row)[i] - (*row)[i - 1]) - ((*row)[1] - (*row)[0])) < 1.0f);
    }

    // Staggered: the second row's grid must not simply reuse the first
    // row's x positions.
    std::set<float> row1Xs (row1.begin(), row1.end());
    int shared = 0;
    for (const auto x : row2)
        shared += row1Xs.count (x) > 0 ? 1 : 0;

    CHECK (shared < (int) row2.size());
}

TEST_CASE ("The VU pair sits symmetric about the plate's vertical centre line", "[gui][layout]")
{
    const auto manifest = parseManifest();
    REQUIRE (manifest.isValid());

    const auto meters = manifest.ofKind ("meter");
    REQUIRE (meters.size() == 2);

    CHECK (meters[0]->cy == meters[1]->cy);
    CHECK (meters[0]->scale == meters[1]->scale);

    // Mirror symmetry about x = 632 (the 1264 px plate's centre line).
    const auto centre = (float) srph::layout::plateCanvasWidthPx * 0.5f;
    CHECK (std::abs ((centre - meters[0]->cx) - (meters[1]->cx - centre)) < 1.0f);
}

TEST_CASE ("Every control stays inside the pinstripe field and off the baked plate art", "[gui][layout]")
{
    const auto manifest = parseManifest();
    REQUIRE (manifest.isValid());

    for (const auto& control : manifest.controls)
    {
        const auto r = capRadiusPlatePx (control);
        INFO ("control \"" << control.id.toStdString() << "\"");

        CHECK (control.cx - r >= fieldLeft);
        CHECK (control.cx + r <= fieldRight);
        CHECK (control.cy - r >= fieldTop);
        CHECK (control.cy + r <= fieldBottom);

        const juce::Rectangle<float> capBox (control.cx - r, control.cy - r, 2.0f * r, 2.0f * r);
        CHECK_FALSE (capBox.intersects (dividerKeepOut));

        // Meters are circular dials in a square box - the box corners may
        // overhang the rectangular vent keep-outs without the BEZEL doing
        // so; the circle-accurate check below covers them. Everything else
        // uses the plain box test.
        if (control.kind != "meter")
        {
            CHECK_FALSE (capBox.intersects (ventLeftKeepOut));
            CHECK_FALSE (capBox.intersects (ventRightKeepOut));
        }
        else
        {
            for (const auto& vent : { ventLeftKeepOut, ventRightKeepOut })
            {
                const auto nearestX = juce::jlimit (vent.getX(), vent.getRight(), control.cx);
                const auto nearestY = juce::jlimit (vent.getY(), vent.getBottom(), control.cy);
                const auto dx = control.cx - nearestX;
                const auto dy = control.cy - nearestY;
                CHECK (dx * dx + dy * dy >= r * r);
            }
        }

        if (control.labelCy > 0.0f)
        {
            using namespace srph::layout;
            const juce::Rectangle<float> labelBox (control.cx - labelBoxWidthPlatePx * 0.5f,
                                                   control.labelCy - labelBoxHeightPlatePx * 0.5f,
                                                   labelBoxWidthPlatePx, labelBoxHeightPlatePx);

            CHECK (labelBox.getY() >= fieldTop);
            CHECK (labelBox.getBottom() <= fieldBottom);

            // Lettering never intrudes into its own control's rotating cap.
            CHECK (labelBox.getY() >= control.cy + r - 1.0f);
        }
    }
}

TEST_CASE ("No two composited elements overlap", "[gui][layout]")
{
    const auto manifest = parseManifest();
    REQUIRE (manifest.isValid());

    for (size_t a = 0; a < manifest.controls.size(); ++a)
    {
        for (size_t b = a + 1; b < manifest.controls.size(); ++b)
        {
            const auto& ca = manifest.controls[a];
            const auto& cb = manifest.controls[b];

            const auto minGap = capRadiusPlatePx (ca) + capRadiusPlatePx (cb);
            const auto dx = ca.cx - cb.cx;
            const auto dy = ca.cy - cb.cy;

            INFO (ca.id.toStdString() << " vs " << cb.id.toStdString());
            CHECK (dx * dx + dy * dy >= minGap * minGap);
        }
    }
}

TEST_CASE ("Editor base size derives from the plate geometry", "[gui][layout]")
{
    using namespace srph::layout;

    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    SeraphAudioProcessorEditor editor (processor);

    // A fresh processor carries no stored uiScaleStep, so the editor
    // constructs at the 100% step and its size IS the base geometry.
    CHECK (editor.getWidth() == baseEditorWidth);
    CHECK (editor.getHeight() == baseEditorHeight);
    CHECK (editor.layoutManifest().isValid());
}
