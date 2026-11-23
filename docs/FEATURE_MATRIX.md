# Feature and milestone status

`Built` below means the native module target compiled. Runtime cases remain pending until exercised on an isolated MySQL-backed test worldserver. See [Testing](TESTING.md) for commands, revisions and expected outcomes.

| Phase / subsystem | Default | State and owner | Interaction / verification |
|---|---|---|---|
| 1. Config, presets, curves and context | ConservativeSlow; 1–80 | Implemented in Config and Policy | Pure caps, rounding and level endpoint tests pass. Snapshot reads on audited native callbacks. |
| 1. Player and bot damage | On, 0.85 at 80 | Implemented in Gameplay for audited swing, weapon/direct spell and DoT paths | Native avoidance and mitigation still apply. Reflection and trigger lineage coverage is partial. |
| 1. Player and bot healing | On, 0.90 at 80 | Implemented for ordinary direct-only heals and ordinary HoTs | Mixed direct+HoT casts leave direct component neutral. No absorb, leech, health transfer or resurrection scaling. |
| 1. NPC damage and rank | On for eligible hostile NPCs, 0.97 at 80 | Implemented in Gameplay | AutoBalance composition is independently switchable. Friendly targets and uncontrolled NPC-vs-NPC fights stay neutral. |
| 1. World NPC HP | On, 1.10 at 80 | Implemented for eligible generated spawns | Native HP validation, no cumulative state. Instance bypass tested as pure policy. Live HP matrix pending. |
| 1. Maps, PvP and GM protections | World/dungeon/raid on; safe mode on | Implemented in Gameplay and Policy | Null/unknown maps neutral. PvP settings and GM mode are separate guards. |
| 1. Pet output and bot compatibility | Pet damage 0.85 at 80; native pet HP | Implemented pet output and soft bot routing. HP mutation and hard integration deferred. | Bot APIs differ between checkouts, and pet native inheritance can recalculate stats. No live pet re-summon or bot soak performed. |
| 1. Logging, diagnostics and reload | Debug off | Implemented, native RBAC and config reload | HP switches take effect on eligible regeneration. Headless diagnostics and reload-in-combat cases pending. |
| 6A / 7A. Map, zone, class, spell and entry rules | Off | Implemented as snapshot config rules. SQL storage and effect-index overrides deferred. | Property-specific replacement for location. Exact spell category and entry opt-outs. No runtime DB writes. |
| 6A / 6C. Outdoor attack types, NPC healing and exclusions | Optional factors neutral | Implemented for audited hook categories. Boss, summon and common noncombat guards included. | Separate ranged-swing flag, school-specific/absorb adjustments, quest-script detection and reflected lineage are unavailable at these stages. |
| 6B. Experience | Off, 1.00 | Implemented world kill/quest/explore, inclusive flat or curved brackets | Native bonuses and XP locks remain intact. Boundary tests pass. Live RAF, rested and event tests pending. |
| 6C. NPC armor, attack speed, mana and regeneration | Native | Deferred | Native stats, haste, attack timers and encounter script state need separate lifecycle verification. |
| 6D. Gear awareness | Off | Implemented in commands for equipped templates and same-map online group. Bot/human splits and tuning deferred. | On-demand sampling, no hot-path work or gear mutations. |
| 6E. Combat budget | Read-only command | Curve-stage TTK and incoming-damage projection implemented. Role anchors and measurements deferred. | Curve math only. Live calibration and burn-down data pending. |
| 7A. Visual scale | Native | Unsupported with a verified visual-only public lifecycle | Native object size also has gameplay consumers. |
| 7A / 8. Era profiles, zone bands and release stages | Native | Deferred | Audited references include fork metadata, patches or native content replacements. No gate or level conversions. |
| 7B. Solo compensation | Off, 1.00 | Implemented bounded open-world ungrouped output and healing | Uses real group membership, no instance effect. |
| 7C. Professions, reputation | Off, 1.00 | Implemented source-specific crafting/gathering step and allowlisted positive kill/quest reputation | Native skill chance, skill cap, source and standing restrictions remain. Live tests pending. |
| 7C. Honor, money, personalized rates | Native | Deferred | Generic money/formula paths lack narrow award context for requested cases. No character/account storage. |
| 8. Level delta | Off | Implemented bounded world PvE damage factor | Uses native actual levels. Pure lower/equal/upper clamp tests pass. |
| 8. Primary/final player stats, resources and threat | Native | Deferred | No verified integration that covers all recalculation, pet inheritance, taunts, healing, PvP and AutoBalance requirements. |
| 8. Loot and reward probability | Native | Deferred | Need quest-drop and source-specific coverage with an unchanged reward economy. |
| 8. Telemetry | Debug off | Sampled before/after magnitudes, config previews and synthetic policy benchmark | No server-wide sampling loop, persistent metrics, or measured TTK claims. |
