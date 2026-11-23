# Compatibility and known coverage limits

Build observations apply to the exact revisions in [Testing](TESTING.md). Core code uses C++20. AutoBalance and Playerbots are independent optional peers. This module neither imports their headers nor changes their source.

## AutoBalance and Playerbots Interaction Framework

### AutoBalance Instance Exclusivity Matrix
AutoBalance has authoritative, exclusive ownership over all scaling mechanics inside instances (both 5-man Dungeons and Raids):

| Gameplay Domain | Instance (Dungeon & Raid) | Outdoor World | Responsible Module |
|---|---|---|---|
| **Creature Level Scaling** | AutoBalance (`OnBeforeCreatureSelectLevel`) | Progression / Zone modules | **AutoBalance** in instances |
| **Creature Health & Attributes** | AutoBalance (`ModifyCreatureAttributes`) | ProgressionScaling (`CreatureHealthScript`) | **AutoBalance** in instances |
| **Creature Damage (Melee/Spell/DoT)** | AutoBalance (`ABUnitScript`) | ProgressionScaling (`CombatScript`) | **AutoBalance** in instances |
| **Creature Healing & CC Duration** | AutoBalance (`ABUnitScript`) | ProgressionScaling (`CombatScript`) | **AutoBalance** in instances |
| **Player / Bot Throughput** | Unmodified by ProgressionScaling | ProgressionScaling (`CombatScript`) | **AutoBalance** in instances |
| **Pet / Summon Throughput** | Unmodified by ProgressionScaling | ProgressionScaling (`CombatScript`) | **AutoBalance** in instances |
| **XP & Money Rewards** | AutoBalance (`RewardScaling`) | ProgressionScaling (`RatesScript`) | **AutoBalance** in instances |

To guarantee zero competition or double-application:
1. `ProgressionScalingPolicy::Participates` unconditionally returns `false` for `Profile::Dungeon` and `Profile::Raid` whenever `autoBalance` is active (the default).
2. `CombatFactor()` immediately returns `1.0` if `source->FindMap()->IsDungeon()` and `autoBalance` is enabled.
3. `CreatureHealthScript::OnCreatureSelectLevel` immediately aborts if `creature->FindMap()->IsDungeon()`.
4. `RatesScript` strictly checks `WorldReward()`, ensuring that XP and reputation rate adjustments apply exclusively in `Profile::World`.

### Playerbots Interoperability
Playerbots inherit from `Player` and emit native player events. `GetPlayerOwner()` resolves:
- Human players: via `actor.ToPlayer()`
- Playerbots: via `actor.ToPlayer()`
- Pets / Summons / Guardians / Totems: via `actor.GetCharmerOrOwnerPlayerOrPlayerItself()`, `actor.GetAffectingPlayer()`, and `actor.ToCreature()->GetSummonerGUID()`.
Playerbots and their pets are treated symmetrically with human players without importing `mod-playerbots` companion headers or mutating core code.

## Event boundaries

- **Swing / weapon spell / ordinary direct damage.** The swing hook runs per native weapon damage component before armor, avoidance, crit and absorbs. Direct spell magnitude is before that method's armor, crit, block and absorbs. A weapon special or ranged shot using the spell path is treated as a spell. Some scripted and raw damage calls bypass the audited hooks entirely.
- **DoT / HoT.** The periodic callback also receives HoT amounts on both audited revisions. Ordinary periodic damage is adjusted before later mitigation. Ordinary periodic healing is adjusted after crit and before heal absorb. Periodic percent-health effects, health transfers, leech and mixed damage+heal auras remain neutral.
- **Direct heals.** Native `HealBySpell` supplies caster first, while ordinary HoT dispatch uses target first. A direct-only heal uses the caster-first route. Heal spells that combine direct healing and a HoT, such as Regrowth, only scale their ordinary HoT component. The direct component remains neutral because the shared callback lacks effect and invocation identifiers. Self-recovery through lethal/resurrection, regen, and raw health APIs is outside these routes. Healing that creates an absorb does not change the shield amount.
- **Reflections, scripts and triggers.** Self/own-unit damage and known damage-derived spells are excluded. The module has no generic cast or damage replay hook. Trigger lineage and reflected-hit identity are unavailable for some cases at these stages. Ordinary triggered spells with native damage still use the single native path. Environments with special encounter spells can opt those spell categories out by ID.
- **Creature health.** The native level selection callback writes base, max and create HP. The module checks all three against native base stats and the server rank rate. It changes only eligible stable spawns at creation or dead respawn generation, with no raw object cache. Live entry changes are skipped. Loot-damage eligibility is reset at the same generation boundary. Restricted cases include scripted templates, summon/ownership markers, passive/civilian/noncombat actors, non-regenerating DB spawns, guards, world bosses by default, and dungeon bosses. Outdoor quest scripts can still warrant entry exclusions where metadata is insufficient.

No tracked data is written to the database. Existing creatures already scaled to the old HP policy restore on native respawn/reinitialization, including after the master switch is disabled. A restart naturally generates the current profile. The combat/reward hooks read a validated immutable snapshot, and no combat-time database queries or file operations occur.
