# State-file validation

Current Replay v63 loading rejects truncated command records and validates represented
player, market resource, resource kind, diplomacy, civilization, stance,
building, unit, technology, and formation enum values before conversion.
Unexpected trailing tokens become unknown records and are rejected.
Replay v61 remains the formation-command compatibility gate; v62 adds queued
farm-reseed commands while retaining the legacy immediate `reseed` record.

Current Save v109 persists objective state, executable trigger runtime
(`enabled`, activation/last-fire ticks, fire count, typed condition/effect
operands), and player-scoped message expiry. Save v100 is the original
objective/trigger/message gate; v101 adds relative timer state and message
expiry; v102 adds message ownership. Both historical layouts migrate through
explicit version branches.

Save loading validates each production queue extraction, caps queues at
five orders, rejects unknown unit encodings, invalid ticks/costs/work
remainders, and impossible producers, including the Anarchy Barracks exception.
Malformed records fail immediately rather than creating default-valued orders.
Save v98 remains the beach/shallows terrain compatibility gate; v99 adds
farm-reseed queues and animal carcass-decay state.

After state replacement, loaded entities are checked for nonzero unique IDs,
valid owners/kinds, map bounds and land/naval terrain domains, distinct
ungarrisoned unit occupancy, legal nonoverlapping building footprints, unit to
building overlap, authoritative building garrison eligibility/capacity, and
transport eligibility/capacity.

Current Scenario v66 preserves the strict executable trigger syntax and
bounded ordered condition/effect vectors introduced by v64, and rejects
unknown conditions/effects.
Scenario v61 remains the compatibility gate for inert
unknown expressions. Both versions use the same runtime garrison eligibility and
capacity rules. Friendly completed surviving Town Centers, Castles, Watch
Towers (including Guard Tower/Keep upgrades), and Bombard Towers work according
to their represented domains; invalid ships, siege, cavalry, enemy shelters,
and overflow assignments are rejected.

Campaign manifests and progress are separate reconstruction formats:
`aoe-campaign 1` and `aoe-campaign-progress 1`. Manifest loading rejects
absolute/traversing/symlink-escaping paths, duplicate resolved scenarios,
missing or malformed scenarios, and invalid ordering. Progress loading rejects
duplicate singleton fields and noncontiguous unlock state; digest mismatch is
reported as stale rather than silently remapped. Victory commits use a unique
same-directory temporary file, file synchronization, atomic replacement, and
directory synchronization.
