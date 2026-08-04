# Villager death presentation evidence

## Original and asset evidence

The supplied VER 5.7 DAT record for Villager unit 83 selects dying graphic
1281. That graphic is `VMBAS_DN`, selects SLP 1476, declares 15 frames per
angle, eight display angles, mirroring mode 6, 0.1 seconds per frame, no replay
delay, and sound 294. SLP 1476 physically contains 75 frames: five stored
angles which supply the eight displayed directions by native mirroring.
`generated/animation_evidence.json` and
`generated/defensive_dat_metadata.json` preserve these extracted fields.

The read-only original decompile was also checked. Its stripped C output does
not embed graphic 1281/SLP 1476 as literals because object graphics are DAT
driven. RTTI retained in `decompiled/AoK-HD-patched.strings.txt` separately
identifies `RGE_Static_Object`, `RGE_Animated_Object`, `RGE_Action_Object`,
`RGE_Moving_Object`, and `RGE_Combat_Object`; no evidence supports routing a
combat-unit death through a building-rubble presentation. Thus DAT/SLP records
are authoritative for exact death art and runtime object classes support the
separate unit-effect lifecycle.

## Reconstruction behavior

`Simulation` creates only `UnitDeathEffect` for a killed Villager and keeps
`BuildingRubbleEffect` empty. Renderer loads SLP 1476 for each required owner,
decodes every native frame hotspot, maps eight display directions onto five
stored angles using mirroring, plays the action once, then holds frame 14 as
corpse art until the existing 18-tick unit-death effect expires. Building
death composites, debris, and fire remain reachable only from
`BuildingRubbleEffect`.

## Deterministic background proof

`tests/villager_death_sdl_smoke.sh` runs `combat-pose-audit.scenario` twice at
ticks 10 and 16 with SDL dummy video/audio and software rendering. Both runs
must be byte-identical. Overlap manifests prove entity 2 is classified
`unit-death-villager`, drawing only SLP 1476 frame 4 while falling and frame 14
for the held corpse. Native hotspot-derived isolation bounds are pinned at
`(310,95) 25x46` and `(296,100) 48x31`. Test rejects any building-rubble case
for entity 2. Scenario SHA-256 is
`850f694f2028090e9282e00fee6f5d9c14d4214e1cb28f84a6e0e7c493f8a511`.
