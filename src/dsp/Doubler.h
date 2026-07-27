#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <vector>

#include "AlignmentDelay.h"
#include "MicroPitchShifter.h"
#include "SpectralShifter.h"
#include "VoiceHumanizer.h"

// A four-voice vocal doubler. A mono sum of the input feeds four independent
// voices; each voice is delayed and detuned by its own engine, then panned to
// its own fixed stereo position (a "per-voice pan" choir spread, not a single
// symmetric L/R pair) scaled by DoubleWidth and added back onto the buffer.
//
// Modes (v0.3.0, SOTA DSP brief ss3.1-ss3.3)
// -----------------------------------------
// * Classic - the v0.1/v0.2 engine, unchanged byte for byte: four short delay
//   lines whose length is modulated by a slow sine, which is a click-free
//   detune trick rather than a true pitch shift (a slowly ramping/sawtooth
//   delay would be a true pitch shifter but clicks on every reset, which is
//   exactly what this avoids). The four LFOs run at different, non-integer-
//   related rates from different phases, so the voices decorrelate over time
//   instead of moving in lockstep - that is what gives this mode its small-
//   choir character rather than a plain two-voice chorus. Reports no latency.
//
// * Micro - constant-offset dual-head micropitch (MicroPitchShifter.h). A real
//   fixed detune rather than a wobble, so a stack holds its interval instead
//   of breathing in and out of tune. Reports no latency; the inherent ~25 ms
//   mean sweep delay is treated as part of the doubler sound.
//
// * Shift - STFT pitch shifting with optional formant preservation
//   (SpectralShifter.h). The only mode that reports host latency.
//
// Mode is not automatable precisely because those latency behaviours differ;
// switching is masked by a 10 ms fade-out / reset / fade-in of the doubled
// voices, which is click-safe without requiring a glitch-free crossfade
// between two simultaneously running engines.
//
// Humanisation (VoiceHumanizer.h) applies in every mode: slow per-voice
// timing and level drift, plus pitch drift in the two modes that can express
// a constant pitch offset. At Humanize == 0 every offset is exactly zero, so
// Classic mode stays bit-identical to v0.2.0.
//
// At amount == 0 the buffer is left bit-exact untouched in every mode. The
// internal delay-line / LFO / shifter state still advances, fed from the
// current input, so re-enabling Double mid-stream doesn't start from stale or
// discontinuous state - this is what keeps Double == 0% part of the plugin's
// null test.
class Doubler
{
public:
    enum class Mode
    {
        classic = 0,
        micro = 1,
        shift = 2
    };

    Doubler() = default;

    // Allocates the delay lines, shifters and scratch buffers. Must be called
    // before the first process() call, and again whenever sample rate/block
    // size change.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears delay-line, LFO, shifter and humaniser state without
    // deallocating.
    void reset();

    // Amount, 0-100%: gain of the four doubled voices added on top of the
    // existing (centered) signal already in the buffer. 0% is a bit-exact
    // bypass.
    void setAmountProportion (float newAmount01);

    // Detune depth in cents. In Classic mode this is the peak instantaneous
    // pitch deviation the modulated delay produces; in Micro and Shift it is
    // a constant offset, distributed across the voices by voiceDetuneScalers.
    void setDetuneCents (float newDetuneCents);

    // Stereo pan spread, 0-100%: 0% keeps all four voices centered (summed
    // equally into both channels), 100% spreads them across the full stereo
    // field at their fixed per-voice pan positions (see voiceConfigs).
    void setWidthProportion (float newWidth01);

    // Doubler engine mode. Changing this while running triggers the 10 ms
    // fade described above; setting it before prepare()/at reset() takes
    // effect immediately.
    void setMode (Mode newMode);

    // Per-voice random-walk drift depth, 0-1. Exactly 0 means exactly no
    // offset (see VoiceHumanizer.h).
    void setHumanizeProportion (float newHumanize01);

    // Formant preservation for Shift mode; inert in Classic and Micro.
    void setFormantPreserveEnabled (bool shouldPreserve);

    // Processes `block` in place, adding the doubled voices on top of
    // whatever is already there. Mono buffers get all voices summed
    // (unpanned, width has no audible effect, matching the documented v0.1
    // behaviour). A zero-sample block is a safe no-op. No allocation occurs
    // here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    // Reported latency in samples for the *target* mode: 0 for Classic and
    // Micro, the STFT engine's analysis+synthesis latency for Shift. The
    // target rather than the currently active mode, so the host's delay
    // compensation is already correct by the time the 10 ms mode fade
    // finishes.
    int getLatencySamples() const noexcept;

    // Worst-case reported latency for the current configuration, i.e. what
    // Shift mode would report. SeraphEngine sizes its dry-path compensation
    // delay from this in prepare(), so no mode change can ever need a longer
    // delay line than was allocated.
    int getMaximumLatencySamples() const noexcept;

    // Currently active (post-fade) mode, for tests and metering.
    Mode getActiveMode() const noexcept { return activeMode; }

private:
    static constexpr int numVoices = 4;

    // Per-voice static configuration: base delay (deliberately offset per
    // voice so they don't start from a correlated comb-filtered
    // relationship with each other), LFO rate (non-integer-related across
    // voices so their modulation drifts in and out of phase rather than
    // locking together), starting LFO phase, and fixed pan position in
    // [-1, +1] (-1 = hard left, 0 = center, +1 = hard right) reached at
    // DoubleWidth == 100%. Voices 0/1 are the original v0.1 outer pair
    // (hard L/R at full width); voices 2/3 are inner voices added in M1 for
    // a fuller small-choir spread.
    struct VoiceConfig
    {
        float baseDelayMs;
        float lfoRateHz;
        double startPhase;
        float panSpread;
    };

    // v0.2.0: base delays re-centered from 13/17/23/29 ms into the 9-24 ms
    // neighborhood of the doubler reference class documented in
    // docs/research-notes.md (tight end ~8-12 ms, outer end ~6-25 ms - see
    // docs/design-brief.md ss2.4). LFO rates/phases/pan roles are unchanged
    // (no reference source published exact 4-voice LFO rates).
    //
    // In Micro and Shift these base delays keep the same numbers but a
    // different job: they are per-voice decorrelation offsets (and, in Micro,
    // the sweep floor) rather than Haas pre-delays - see MicroPitchShifter.h.
    static constexpr std::array<VoiceConfig, numVoices> voiceConfigs { {
        { 9.0f, 0.23f, 0.0, -1.0f },                                     // outer left
        { 24.0f, 0.31f, juce::MathConstants<double>::pi, 1.0f },         // outer right
        { 13.0f, 0.17f, juce::MathConstants<double>::halfPi, -1.0f / 3.0f }, // inner left
        { 19.0f, 0.37f, juce::MathConstants<double>::pi * 1.5, 1.0f / 3.0f } // inner right
    } };

    // How the Detune knob's cents are distributed across voices in Micro and
    // Shift mode (brief ss3.1). The outer pair takes the full interval either
    // side; the inner pair is deliberately asymmetric (-0.45/+0.55 rather
    // than -0.5/+0.5) so the four voices never settle into a coherent beating
    // relationship. Follows the H3000 MicroPitch -9/+11 cent lineage.
    static constexpr std::array<float, numVoices> voiceDetuneScalers { { -1.0f, 1.0f, -0.45f, 0.55f } };

    static constexpr float maxDetuneCents = 50.0f;
    static constexpr double smoothingTimeSeconds = 0.05;
    static constexpr double modeFadeSeconds = 0.010;

    enum class FadeState
    {
        idle,
        fadingOut,
        fadingIn
    };

    void resetActiveModeState();

    double sampleRate = 44100.0;

    // Single-channel modulated delay lines: the doubler always derives its
    // voices from a mono sum of the input, so only one channel of state is
    // needed per voice regardless of the host's channel count. Default-
    // constructed here; prepare() calls setMaximumDelayInSamples() on each
    // before any process() call, so the default constructor's initial
    // capacity is never relied upon.
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, numVoices> delayLines;

    // Micro/Shift engines and the Shift path's per-voice base pre-delay.
    std::array<MicroPitchShifter, numVoices> microShifters;
    std::array<SpectralShifter, numVoices> spectralShifters;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, numVoices> shiftPreDelays;
    std::array<VoiceHumanizer, numVoices> humanizers;

    // Keeps the signal the voices are added onto aligned with the voices
    // themselves. In Shift mode the STFT engine delays each voice by its
    // analysis+synthesis latency, but the main path does not pass through it;
    // without this delay the plugin would emit audio ahead of the latency it
    // reports. Zero - and bit-exact - in Classic and Micro.
    AlignmentDelay mainPathDelay;

    // Scratch, sized in prepare(): the mono sum, and one buffer per voice.
    // Classic and Micro could work sample by sample in place, but Shift
    // cannot - the STFT engine needs a contiguous run of samples - so all
    // three modes share the same two-pass shape. Building the mono sum up
    // front is bit-identical to v0.2.0's interleaved version because the
    // doubler only ever *adds* to the buffer, never reads a sample it has
    // already written.
    std::vector<float> monoScratch;
    std::array<std::vector<float>, numVoices> voiceScratch;

    std::array<double, numVoices> phases {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> detuneSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> humanizeSmoothed;

    float lastAmount01 = 0.25f;
    float lastDetuneCents = 10.0f; // v0.2.0 default (was 15), see ParameterLayout.cpp
    float lastWidth01 = 1.0f;
    float lastHumanize01 = 0.0f;

    Mode activeMode = Mode::classic;
    Mode targetMode = Mode::classic;
    FadeState fadeState = FadeState::idle;
    float fadeGain = 1.0f;
    float fadeIncrement = 1.0f;
    bool prepared = false;

    bool formantPreserve = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Doubler)
};
