# Configuration

All keys use the `ProgressionScaling.` prefix. The [distribution config](../conf/ProgressionScaling.conf.dist) lists implemented options. Core config reload publishes a new snapshot. Existing outdoor creature HP changes at eligible respawns or clean creation.

| Key family | Behavior |
|---|---|
| `Enable`, `Preset`, `MinLevel`, `MaxLevel` | Master gate, preset and curve interpolation range. Range is bounded to 1–80. A reversed range falls back to 1–80. A collapsed range steps from Start to End at the boundary. |
| `Curve.Enable`, `Curve.Mode`, `<Effect>.Enable`, `.Start`, `.End`, `.Power` | Curve switches. Mode `0` is linear and `1` uses the configured exponent. `preset` inherits the selected preset. Numeric entries take precedence. A disabled curve contributes 1.0. Profile and category switches independently gate the whole effect. |
| `Compatibility.SafeMode`, `.AllowOutdoorPvP` | Safe mode keeps battlegrounds, arenas, outdoor player-vs-player damage, active PvP participants, and dungeon/raid creature HP neutral. Outdoor PvP also requires its own explicit opt-in. Battleground and arena scaling requires the applicable profile. |
| `AutoBalanceCompat.*` | Allows independently bypassing instance NPC damage, and protects instance NPC HP even if safe mode is off. The dungeon and raid HP profile flags default off separately. |
| `Profile.<Profile>.Enable`, `.<Effect>.Enable`, `.<Effect>Multiplier` | Profile gate, per-effect gate, and profile factor. Profiles are `World`, `Dungeon`, `Raid`, `Battleground`, `Arena`. World, dungeon and raid are on. Unknown map metadata stays neutral. |
| `<Effect>.<Category>.Enable`, `.<Category>Multiplier` | Damage categories are `Swing` (native swing hook), `Spell` (including weapon spells) and `Periodic`. Heal categories are `DirectHeal` and `PeriodicHeal`. Each selects one optional factor on the same event path. The native swing hook has no melee/ranged flag. |
| `Rank.<Rank>.DamageMultiplier`, `.HealthMultiplier` | Native `Normal`, `Elite`, `Rare`, `RareElite`, `WorldBoss` rank factors. All start at 1.0. `WorldDifficulty.WorldBossExempt` defaults on. Unsupported HP lifecycles remain ineligible. |
| `CreatureDamage.NpcSummons` | Allows qualifying NPC-summoned actors on the creature damage path. Unresolved or player-controlled summons stay protected. |
| `CreatureHealing.Enable` | Enables the independent outdoor NPC healing policy. Its preset curve and subcategory multipliers start at 1.0. |
| `Class.Enable`, `Class.<Class>.<Effect>` | Optional class factors for `PlayerDamage`, `PlayerHealing` and `PetDamage`, using the owner class for pets. No specialization assumptions. |
| `SoloCompensation.*` | Outdoor player/owned-pet output assistance when the owner has no group. Damage and healing factors are 1.00–1.15. Any group, including a group made of bots, is grouped. |
| `LevelDelta.*` | Optional additive slope by actual source minus target level for world-only player-versus-creature damage. Level difference and factor are bounded. Default off. |
| `GearAwareness.Enable` | Read-only own and same-map group gear output on `.progression gear`. Includes bots where present. Equipment mean omits missing items, body/tabard and zero template levels. A two-handed weapon naturally fills one slot. Level is the template level, including for heirlooms. |
| `Rates.*` | Independent world-only kill, quest and exploration XP factors. Dungeon Finder and battleground awards bypass. Optional level bracket overrides. |
| `Professions.*` | World-only crafting and gathering step multipliers of 1.0–3.0. Native profession type, award chance and skill cap remain in force. Fishing uses a separate native call and is outside this policy. |
| `Reputation.*` | Optional world-only positive kill and quest gains for explicit faction IDs in `Factions`, separated by `&vert;`. Native standing caps and source restrictions still apply. Empty list means no change. |
| `Debug`, `Debug.Combat`, `Debug.Creatures`, `Debug.Pets`, `Debug.Compatibility` | Up to one changed sample per ten seconds per combat/creature/pet channel. Compatibility policy is logged on config load. |

Numeric factors must be finite. Most factors accept 0.10–3.00, curve power accepts 0.10–8.00, and a final combat factor stays within 0.10–3.00. Invalid numbers use the documented default. Positive combat magnitudes use nearest integer, with halves rounded upward, bounded output and a minimum of one at the module stage. Native mitigation may still yield zero.

## Rules

Enable each rule family separately. Use a quoted string and separate rows with `|`. Leading and trailing whitespace in a field is ignored. Invalid or duplicate rows log a warning. The first valid duplicate and first valid overlapping XP row take precedence. IDs are numeric server data identifiers. Unknown IDs stay inert.

```ini
ProgressionScaling.MapOverrides.Enable = 1
ProgressionScaling.MapOverrides.Rules = "0:0:0:-1:PlayerDamage:0.98|33:0:0:1:PlayerDamage:0.95"
ProgressionScaling.ZoneOverrides.Enable = 1
ProgressionScaling.ZoneOverrides.Rules = "0:12:0:-1:CreatureHealth:0.95"
ProgressionScaling.SpellOverrides.Enable = 1
ProgressionScaling.SpellOverrides.Rules = "133:Spell:0.98|172:Periodic:exclude"
ProgressionScaling.CreatureExclusions.Enable = 1
ProgressionScaling.CreatureExclusions.Rules = "123:CreatureHealth|123:PlayerDamage"
ProgressionScaling.Rates.Enable = 1
ProgressionScaling.Rates.Brackets.Enable = 1
ProgressionScaling.Rates.Brackets.Rules = "1:60:0.85~0.95~1:1:1|61:70:0.95:1:1|71:80:1:1:1"
```

- Map/zone format is `map:zone:area:difficulty:effect:factor`. Map ID `0` is a real map. Zone and area `0` mean unspecified, and an area requires its zone. Difficulty `-1` is a wildcard and `0`–`3` are native mode indices. Exact area beats zone, zone beats map, and exact difficulty beats wildcard at the same location. A location factor replaces the corresponding profile factor for that effect.
- Spell format is `spell:category:factor-or-exclude`. Keys match exact spell rank IDs and audited categories. A factor multiplies the one event result. An exclusion bypasses all ordinary factors for that category. The hook lacks a per-effect index and a trigger lineage, so ambiguous multi-effect events and known derived spells stay neutral. `Swing` has no spell ID.
- Creature format is `entry:effect`. Matching source or target creature entries bypass the specified combat effect. In the health subsystem the rule applies to that creature's HP.
- Bracket format is `first:last:kill:quest:exploration`. Bounds are inclusive. Each source field accepts a flat factor or `start~end~power`. A bracket replaces that source's flat multiplier. Gaps inherit flat factors.

## Order and opt-outs

Master, source and target safety gates run first. Profile and effect participation gates come next. Exclusions then win over curve, map/zone, rank, class, category, spell, solo and level-delta factors. Compatible factors compose in floating point and the module writes one integer value. Comparison commands preview curve, location, rank and class without simulating a specific spell, target or combat state.

For neutral operation, set `Enable = 0`. For a neutral curve profile with independent options available, select `Preset = Neutral`, keep numeric overrides at `preset`, and keep optional factors at 1.0. For instance health, enabling the profile flag and switching off safe mode still leaves the corresponding AutoBalance bypass active until explicitly cleared. This overlap mode requires independent validation with any instance scaling module present.
