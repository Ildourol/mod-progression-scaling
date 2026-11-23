# Conflict matrix

| Systems | Resolution | Verification |
|---|---|---|
| Player / guardian / NPC | One actor class. Player-owned guardian damage uses pet curve only. Unresolved summons and charmed actors are neutral. | Source inspection. Live pet and bot matrix pending. |
| AutoBalance / instance creature HP | Default safety and instance participation gates are independent. Map/zone overrides cannot bypass them. | Source audit and pure policy tests. Live HP comparison pending. |
| AutoBalance / NPC output | Both modules can handle the same pre-mitigation callback. Each applies its own factor once. Hook order and integer rounding can differ. Instance creature damage can be suppressed by config. | Source audit. Runtime order and logs pending. |
| AutoBalance / healing | Direct/periodic callback shapes differ. Module avoids damage/heal double-application inside its own stack. Optional NPC healing is outdoor-only. | Source audit. Live matrix pending. |
| Playerbots / ordinary output | Bot Player objects and owned summons already use native paths. Compensation and human-only identification are unavailable. | Source audit. No dependency. |
| Generic / specialized attacks | A category factor specializes the one selected output curve. Spell/entry exclusions win. | Pure policy and live hit tests. |
| Absorb creation / absorb consumption | No module absorb mutation. | Source audit. |
| Flat XP / brackets / core event rates | One highest-specificity non-overlapping bracket replaces flat source factor. Native XP awards, RAF and rested bonuses follow. World-only; AutoBalance instance XP remains separate. | Boundary tests and live bonus tests. |
| Zone/area / map / difficulty | Area replaces zone, which replaces map; exact difficulty replaces wildcard at equal location. Each property resolves independently. | Pure override tests. |
| Gear / item scaling | Gear is read-only and evaluated only on command. No gear contribution to combat or rewards. | Source audit. |
| Solo / AutoBalance | Only eligible world actors without any group; a duo with a bot is grouped. | Source audit. Live tests pending. |
| Reload / health | Combat sees new snapshot; creature generation recalculates HP on respawn or reinitialization. No live scan. | Live lifecycle pending. |
| Zone bands / level delta | No zone-band modifier. Optional delta has its own bounds and scope. | Pure bound tests and live checks. |
