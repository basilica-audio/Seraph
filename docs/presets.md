# Factory presets

Twelve factory presets ship with Seraph v0.3.0, embedded via BinaryData from
`presets/factory/*.json` (see sibling plugin nave's
`docs/preset-system-notes.md` for the build wiring - Seraph follows the same
M2 preset system, `.scaffold/specs/preset-system-m2.md`). Eight of the nine
v0.2.0 presets are sourced starting points from `docs/design-brief.md`'s
"Factory Presets" section - see that document's own Honesty section (ss5) for
what those numbers are and aren't calibrated against (research/manual/forum-
derived, not measured hardware or A/B'd by ear against Seraph).

| Preset | Category | Intent |
|---|---|---|
| **Default** | Init | The plugin's out-of-the-box parameter state (see the M2 default-resolution order in nave's `docs/preset-system-notes.md`), exposed as an explicit preset so there's always a one-click way back to it. |
| **Lead - Cut Through** | Vocals | Solo operatic lead over dense guitars/orchestra: moderate de-essing, a healthy Air lift, light glue, a subtle tight double. |
| **Lead - Intimate/Close-Mic** | Vocals | Close-mic'd solo take: heavier (but narrower) de-essing for proximity sibilance, minimal Air/Comp/Double. |
| **Choir - Wide Spread** | Vocals | Turns a handful of real takes into a full small-choir width: light de-essing/Air, moderate glue, heavy wide doubling. |
| **Choir - Tight Blend** | Vocals | Layered backing vocals that need to sit underneath a lead: no Air, moderate glue, moderate doubling at reduced width. |
| **Spoken/Growled Interlude** | Vocals | Level-consistent narration/growl against a quiet orchestral backing - minimal de-essing (per the manual's own guidance that it's often unnecessary here), heavier glue for consistency. |
| **Glue Only** | FX | Dynamics/level-consistency utility - only the Gentle Compressor stage engaged, everything else neutral. |
| **De-Ess Only (Surgical)** | FX | Isolates sibilance control as a standalone utility insert. |
| **Wide Double (No Dynamics)** | FX | Doubler-only utility for engineers routing de-essing/dynamics elsewhere in the chain. |
| **Choir - Sacred Shift** *(new in v0.3.0)* | Vocals | The Shift engine at its most characteristic: spectral pitch shifting with formants held in place, 35% humanisation so the voices drift apart, and a soft-kneed de-esser underneath. The most expensive preset here, and the one to reach for on an exposed choir stack. **Reports ~30 ms of latency** - see the manual's latency section. |
| **Doubler - Vintage Micro** *(new in v0.3.0)* | Vocals | The Micro engine doing what a hardware micropitch box does: a fixed 12 cent detune, full width, no dynamics processing to speak of. Zero latency. A drop-in widener for a lead line. |
| **Lead - Tight Stack** *(new in v0.3.0)* | Vocals | A modern lead-vocal chain end to end: Micro at a tight 7 cents with 25% humanisation, plus everything the v0.3.0 de-esser added - stereo link, a 6 dB soft knee and 1 ms of lookahead - over a linked compressor. **Reports 1 ms of latency** from the lookahead. |

The nine v0.2.0 presets are sonically unchanged in v0.3.0: they gained the
eight new parameter keys at their neutral values, which a test asserts rather
than assumes. The eight non-Default ones among them are described in full
(per-parameter settings and rationale) in `docs/design-brief.md` ss3
("Factory Presets").

No preset references a specific vocal take or hardware unit - loading source
material onto the track Seraph is inserted on is always a separate, prior step
(see `docs/manual.md`).
