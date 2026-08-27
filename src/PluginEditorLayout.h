#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

// Seraph's wave-3 COMPOSITIONAL faceplate geometry (campaign 2026-08,
// .scaffold/gui-assets/rollout-2026-07): the plate is the accepted EMPTY
// family plate render (resources/gui/plate_seraph.png - obsidian panel,
// gold chamfer + pinstripe, corner filigree, 4 corner screws, central
// divider flourish, 2 amber vent grilles, NO baked controls), and every
// control is composited live from the extracted control-sprite library
// (.scaffold/gui-assets/sprite-library/, see its provenance.md) at the
// coordinates in resources/gui/layout_manifest.json.
//
// This header carries only what is NOT per-control position data (that
// lives in the manifest, the single source of truth - see
// gui/LayoutManifest.h): editor chrome (top strip, stepped scale),
// plate-to-@1x scaling, and the per-SPRITE-FAMILY intrinsic geometry
// (anchor points, cap radii - measured once against the sprite PNGs
// themselves, see each entry's provenance note).
//
// SUPERSEDES the M3 vector editor's runtime-computed panel layout (the
// BusPanel/PointerKnob/NeedleMeter generation). Those components stay in
// the tree per the suite's "superseded, not deleted" convention, but this
// editor no longer uses them.
namespace srph::layout
{
    // Plate render canvas (1k family plate) and its @1x on-screen size -
    // same 900/1264 ratio as silentium's master-05 baseline.
    constexpr int plateCanvasWidthPx = 1264;
    constexpr int plateCanvasHeightPx = 848;
    constexpr int plateWidth1x = 900;
    constexpr int plateHeight1x = 604;

    // plate-render px -> @1x px.
    constexpr float plateToUnit = (float) plateWidth1x / (float) plateCanvasWidthPx;

    // Top chrome strip (preset bar + stepped scale button), same layout
    // family as the merged M3 editors.
    constexpr int topStripHeight1x = 36;
    constexpr int topStripGap1x = 4;
    constexpr int scaleButtonWidth1x = 64;

    constexpr int baseEditorWidth = plateWidth1x;
    constexpr int baseEditorHeight = topStripHeight1x + topStripGap1x + plateHeight1x;

    constexpr std::array<float, 3> scaleSteps { 1.0f, 1.5f, 2.0f };

    // ==================== sprite-family intrinsic geometry ====================
    // Anchor = the point in the sprite's own pixel space that the layout
    // manifest's cx/cy positions. Measured against the sprite PNGs
    // (sprite-library extraction wave 2026-08-27, radial-profile /
    // blob-centroid analysis - see the wave-3 rollout brief's measurement
    // log). Cap radius = the rotating brass cap's radius in sprite px,
    // from the radial luminance profile's plateau edge.

    // sprite_knob_brass.png (148x148, from master-05 row-1 far-right knob):
    // cap centre (75.5, 70.0), cap plateau to r~30, rolloff complete ~r39.
    constexpr float knobAnchorX = 75.5f;
    constexpr float knobAnchorY = 70.0f;
    constexpr float knobCapRadius = 34.0f;
    constexpr float knobSpriteSizePx = 148.0f;

    // sprite_toggle_up.png (117x129, overture wave-1 redo crop - family
    // lever toggle, lever up = ON): housing centre (58, 68).
    constexpr float toggleAnchorX = 58.0f;
    constexpr float toggleAnchorY = 68.0f;

    // sprite_vu_dial.png (364x364, master-05 LEFT VU crop (140,121,472,453)
    // + 16 px pad - canonical -20..+3 VU face, hub + anchor bar, NO
    // needle): the manifest positions the sprite's CANVAS CENTRE (182,
    // 182); the needle pivot inside the sprite is master px (321.17,
    // 355.193) - sprite origin (124, 105) = (197.17, 250.193), i.e.
    // fractions (0.541675, 0.687343) of the 364 px canvas - fed to
    // AnalogMeter as its pivot fractions (needle provenance:
    // resources/gui/sprite_needle_master05.provenance.json).
    constexpr float meterAnchorX = 182.0f;
    constexpr float meterAnchorY = 182.0f;
    constexpr float meterSpriteSizePx = 364.0f;
    constexpr float meterPivotXFraction = 197.17f / meterSpriteSizePx;
    constexpr float meterPivotYFraction = 250.193f / meterSpriteSizePx;
    constexpr float meterBezelRadiusPlatePx = 166.0f; // outer bezel, for layout keep-out tests

    // VU calibration: the dials read the processor's per-block output peak
    // (linear, per channel) through the suite VU face's -20..+3 scale with
    // a -18 dBFS = 0 VU alignment (EBU R68-flavoured; a full-scale digital
    // peak pins at the scale's +3 stop). GUI-side only - the audio thread
    // stores raw linear peaks.
    constexpr float vuReferenceLevelDbfs = -18.0f;

    // Continuous-knob sweep (suite standard, matches MasterCropKnob
    // defaults).
    constexpr float knobSweepDeg = 270.0f;

    // ==================== engraved lettering ====================
    // Label box @plate-px, centred on each control's manifest cx at the
    // manifest's labelCy.
    constexpr float labelBoxWidthPlatePx = 150.0f;
    constexpr float labelBoxHeightPlatePx = 26.0f;
}
