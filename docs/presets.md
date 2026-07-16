# Factory presets

Nine factory presets ship with Seraph v0.2.0, embedded via BinaryData from
`presets/factory/*.json` (see sibling plugin nave's
`docs/preset-system-notes.md` for the build wiring - Seraph follows the same
M2 preset system, `.scaffold/specs/preset-system-m2.md`). Eight are sourced
starting points from `docs/design-brief.md`'s "Factory Presets" section - see
that document's own Honesty section (ss5) for what these numbers are and
aren't calibrated against (research/manual/forum-derived, not measured
hardware or A/B'd by ear against Seraph).

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

All eight non-Default presets are described in full (per-parameter settings
and rationale) in `docs/design-brief.md` ss3 ("Factory Presets"). No preset
references a specific vocal take or hardware unit - loading source material
onto the track Seraph is inserted on is always a separate, prior step (see
`docs/manual.md`).
