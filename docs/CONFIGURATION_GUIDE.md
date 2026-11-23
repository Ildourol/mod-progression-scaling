# ProgressionScaling Configuration Guide & Architectural Reference

![Progression Scaling](images/progression_scaling_banner.jpg)

`mod-progression-scaling` is a high-performance, non-intrusive scaling engine designed for AzerothCore (WotLK 3.3.5a). It provides fine-grained, mathematical curve-based scaling across character levels (1–80), world zones, creature ranks, and player classes, while maintaining strict architectural isolation and complete compatibility with companion modules like `mod-autobalance` and `mod-playerbots`.

---

## 1. Architectural Overview: How the Module Works

### 1.1 The Mathematical Curve Engine
Progression scaling uses bounded power-law equations rather than arbitrary lookups or database overrides. For any character or creature at level $L \in [L_{min}, L_{max}]$:

$$\text{Progress} = \frac{L - L_{min}}{L_{max} - L_{min}}$$

$$\text{Factor}(L) = \text{Start} + (\text{End} - \text{Start}) \times \text{Progress}^{\text{Power}}$$

* **Linear Mode (`Curve.Mode = 0`)**: Progress is strictly linear ($\text{Power} = 1.0$).
* **Power Mode (`Curve.Mode = 1`)**: Non-linear ease-in or ease-out scaling governed by $\text{Power} \in [0.10, 8.00]$.
* **Safety Bounding**: All calculated factors are hard-clamped to $[0.10, 3.00]$ to prevent zero-damage locks or server-crashing numeric overflows.

```
       Multiplier
          ^
     1.00 +--------------------------------------+ (End Level: 80)
          |                                   ...*
          |                             ..''''
          |                       ..''''
     0.82 +                  ..''''   (Curve: ConservativeSlow)
          |            ..''''
          |      ..''''
     0.65 +*'''''--------------------------------+ (Start Level: 1)
          +--------------------------------------> Level
         Min (1)                                Max (80)
```

### 1.2 Preset Curve Catalog
When `<Effect>.Start`, `<Effect>.End`, or `<Effect>.Power` are set to `preset`, values are derived from `ProgressionScaling.Preset`:

| Preset | Start | End | Power | Intended Progression Feel |
|---|---|---|---|---|
| `Neutral` | `1.00` | `1.00` | `1.00` | Pure 1.0 passthrough. No stat adjustments. |
| `ConservativeSlow` *(Default)* | `0.65` | `1.00` | `1.20` | Gentle power curve; smooth early-game pacing with full vanilla fidelity by level 80. |
| `BotFriendlySlow` | `0.70` | `1.00` | `1.15` | Tailored for solo/bot companion survivability in low-level leveling. |
| `GentleSlow` | `0.80` | `1.00` | `1.10` | Very subtle assist curve for slightly smoother early game. |
| `VanillaInspired` | `0.55` | `1.00` | `1.35` | Gritty early-game difficulty mimicking classic power scarcity. |
| `Custom` | `User` | `User` | `User` | Allows granular numeric definition of each individual effect. |

### 1.3 Event Interception & Non-Intrusive Hooking
The module acts strictly via `ScriptMgr` lifecycle hooks:
* **`PlayerScript::OnModifyMeleeDamage`**: Intercepts auto-attacks / melee swings.
* **`PlayerScript::OnModifySpellDamageDone`**: Intercepts spells, ranged abilities, and periodic ticks.
* **`PlayerScript::OnHeal`**: Intercepts direct heals and HoTs.
* **`PlayerScript::OnPlayerGiveXP`**: Modifies quest, kill, and exploration XP.
* **`CreatureScript::OnCreatureAddWorld`**: Scales outdoor creature health at spawn time in memory without touching world database tables.

### 1.4 AutoBalance Coexistence Doctrine
`mod-autobalance` possesses exclusive authority over dungeon and raid encounters. To prevent conflicting modifications or double-scaling:
1. `ProgressionScaling.AutoBalanceCompat.Enable = 1` immediately disables instance creature health modifications.
2. `ProgressionScaling.AutoBalanceCompat.AllowDungeonCreatureDamage = 0` and `AllowRaidCreatureDamage = 0` suppress instance creature damage scaling.
3. Player and bot instance combat factors are neutralized whenever AutoBalance instance ownership is recognized.

---

## 2. In-Depth Feature Spotlight

### 2.1 `ProgressionScaling.Compatibility.AllowOutdoorPvP`
```ini
ProgressionScaling.Compatibility.AllowOutdoorPvP = 0
```
* **Purpose**: Prevents progression scaling curves from distorting player-versus-player combat in the open world.
* **Mechanism**:
  When two players engage in combat in the open world (`Profile.World`), their level difference or low-level curve multipliers could artificially increase or decrease damage.
  - When set to `0` (or when `SafeMode = 1`): If either the attacker or target is a player (or player-owned pet/bot) in open-world PvP combat, `CombatFactor()` bypasses all scaling and returns `1.0` (pure vanilla combat calculations).
  - When set to `1` (and `SafeMode = 0`): Outdoor PvP encounters will scale based on the attacker's progression curve factor.
* **Recommendation**: Keep set to `0` on servers where fair world PvP, dueling outside capital gates, or contested zone skirmishes are expected.

### 2.2 `ProgressionScaling.GearAwareness.Enable`
```ini
ProgressionScaling.GearAwareness.Enable = 0
```
* **Purpose**: Unlocks real-time gear diagnostics via the `.progression gear` in-game chat command without touching combat formulas.
* **Mechanism**:
  When enabled (`1`), players or GMs can select a target and type `.progression gear`. The module:
  1. Inspects all equipped item slots from `EQUIPMENT_SLOT_START` (Head) to `EQUIPMENT_SLOT_END` (Tabard).
  2. Ignores empty slots, shirts, tabards, and unitemized templates.
  3. Queries `item->GetTemplate()->ItemLevel` for each valid piece of equipment.
  4. Calculates and prints the arithmetic average item level (Average Item Level / ilvl) for the target and all party members on the same map.
* **Recommendation**: Enable (`1`) if players or GMs want quick in-game inspection of group gear readiness without requiring heavy third-party addons.

---

## 3. Comprehensive Configuration Parameter Reference

All configuration options reside in `conf/ProgressionScaling.conf` (staged from `conf/ProgressionScaling.conf.dist`).

### Section 1: Master Controls & Curve Presets

| Parameter | Type | Default | Description |
|---|---|---|---|
| `ProgressionScaling.Enable` | Boolean | `1` | Master toggle for the entire module. `0` disables all hooks and returns 1.0 everywhere. |
| `ProgressionScaling.Preset` | String | `ConservativeSlow` | Curve preset (`Neutral`, `ConservativeSlow`, `BotFriendlySlow`, `GentleSlow`, `VanillaInspired`, `Custom`). |
| `ProgressionScaling.MinLevel` | Integer | `1` | Bottom level clamp for curve evaluation (1–80). |
| `ProgressionScaling.MaxLevel` | Integer | `80` | Top level clamp for curve evaluation (1–80). |
| `ProgressionScaling.Curve.Enable` | Boolean | `1` | Enables evaluation of mathematical power curves. When `0`, effect curves return 1.0. |
| `ProgressionScaling.Curve.Mode` | Integer | `1` | `0` = Linear interpolation; `1` = Non-linear power curve using `Power` exponent. |
| `ProgressionScaling.Compatibility.SafeMode` | Boolean | `1` | Master safety guard. Forces battlegrounds, arenas, outdoor PvP, and instance boss HP to stay neutral (1.0). |
| `ProgressionScaling.Compatibility.AllowOutdoorPvP` | Boolean | `0` | If `1` (and SafeMode=0), permits curve scaling in open-world PvP fights. Default `0` keeps PvP untouched. |

---

### Section 2: Observability & Telemetry Logging

| Parameter | Type | Default | Description |
|---|---|---|---|
| `ProgressionScaling.Debug` | Boolean | `0` | Master debug flag. Enables diagnostic startup and lifecycle logging. |
| `ProgressionScaling.Debug.Combat` | Boolean | `0` | Logs real-time combat calculation breakdowns (rate-limited to 1 entry per 10s per actor). |
| `ProgressionScaling.Debug.Creatures` | Boolean | `0` | Logs creature health adjustments upon entering world maps. |
| `ProgressionScaling.Debug.Pets` | Boolean | `0` | Logs pet damage calculation and master-linkage resolutions. |
| `ProgressionScaling.Debug.Compatibility` | Boolean | `0` | Logs AutoBalance handshakes and instance bypass status during startup and zone transitions. |

---

### Section 3: AutoBalance Interoperability

| Parameter | Type | Default | Description |
|---|---|---|---|
| `ProgressionScaling.AutoBalanceCompat.Enable` | Boolean | `1` | Yields all dungeon and raid instance scaling authority to `mod-autobalance`. |
| `ProgressionScaling.AutoBalanceCompat.DisableDungeonCreatureHealth` | Boolean | `1` | Ensures dungeon creature HP is never scaled by ProgressionScaling. |
| `ProgressionScaling.AutoBalanceCompat.DisableRaidCreatureHealth` | Boolean | `1` | Ensures raid creature HP is never scaled by ProgressionScaling. |
| `ProgressionScaling.AutoBalanceCompat.AllowDungeonCreatureDamage` | Boolean | `0` | When `0`, suppresses ProgressionScaling dungeon creature damage, preventing double-application. |
| `ProgressionScaling.AutoBalanceCompat.AllowRaidCreatureDamage` | Boolean | `0` | When `0`, suppresses ProgressionScaling raid creature damage, preventing double-application. |

---

### Section 4: Core Combat Effect Curves

Each effect supports `Enable`, `Start`, `End`, and `Power`. Set numeric values (e.g. `0.70`, `1.00`, `1.20`) or use `preset` to inherit from `ProgressionScaling.Preset`.

| Parameter Family | Default | Target Effect Description |
|---|---|---|
| `ProgressionScaling.PlayerDamage.*` | `preset` | Modifies outgoing player damage (physical swings, spells, DoTs). |
| `ProgressionScaling.PlayerHealing.*` | `preset` | Modifies outgoing player healing (direct heals, HoTs). |
| `ProgressionScaling.CreatureDamage.*` | `preset` | Modifies outgoing creature damage against players/bots. |
| `ProgressionScaling.CreatureHealth.*` | `preset` | Modifies outdoor creature max health upon map spawn. |
| `ProgressionScaling.PetDamage.*` | `preset` | Modifies outgoing damage of player/bot-controlled pets and totems. |
| `ProgressionScaling.CreatureHealing.*` | `preset` (Disabled) | Modifies outgoing healing cast by NPC creatures. |

---

### Section 5: Combat Subcategory Multipliers

Multipliers compose on top of the base curve for specific attack and heal types (clamped to $[0.10, 3.00]$):

| Parameter | Type | Default | Subcategory Description |
|---|---|---|---|
| `ProgressionScaling.PlayerDamage.Swing.Enable` | Boolean | `1` | Toggles auto-attack swing modifier. |
| `ProgressionScaling.PlayerDamage.SwingMultiplier` | Float | `1.00` | Flat multiplier applied to player melee auto-attacks. |
| `ProgressionScaling.PlayerDamage.Spell.Enable` | Boolean | `1` | Toggles direct spell damage modifier. |
| `ProgressionScaling.PlayerDamage.SpellMultiplier` | Float | `1.00` | Flat multiplier applied to player direct cast spells. |
| `ProgressionScaling.PlayerDamage.Periodic.Enable` | Boolean | `1` | Toggles DoT (Damage over Time) modifier. |
| `ProgressionScaling.PlayerDamage.PeriodicMultiplier` | Float | `1.00` | Flat multiplier applied to player periodic damage ticks. |
| `ProgressionScaling.CreatureDamage.Swing.*` | Mixed | `1` / `1.00` | Toggles and multiplier for creature auto-attacks. |
| `ProgressionScaling.CreatureDamage.Spell.*` | Mixed | `1` / `1.00` | Toggles and multiplier for creature direct spells. |
| `ProgressionScaling.CreatureDamage.Periodic.*` | Mixed | `1` / `1.00` | Toggles and multiplier for creature DoTs. |
| `ProgressionScaling.PetDamage.Swing.*` | Mixed | `1` / `1.00` | Toggles and multiplier for pet melee auto-attacks. |
| `ProgressionScaling.PetDamage.Spell.*` | Mixed | `1` / `1.00` | Toggles and multiplier for pet abilities and spells. |
| `ProgressionScaling.PetDamage.Periodic.*` | Mixed | `1` / `1.00` | Toggles and multiplier for pet periodic ticks. |
| `ProgressionScaling.PlayerHealing.DirectHeal.*` | Mixed | `1` / `1.00` | Toggles and multiplier for player direct heals. |
| `ProgressionScaling.PlayerHealing.PeriodicHeal.*` | Mixed | `1` / `1.00` | Toggles and multiplier for player HoTs (Heals over Time). |

---

### Section 6: World Bosses & NPC Summons

| Parameter | Type | Default | Description |
|---|---|---|---|
| `ProgressionScaling.WorldDifficulty.WorldBossExempt` | Boolean | `1` | When enabled, creatures with rank `WorldBoss` or flagged as boss difficulty are completely exempt from scaling. |
| `ProgressionScaling.CreatureDamage.NpcSummons` | Boolean | `0` | If `1`, allows qualifying NPC-summoned minions to receive creature damage scaling. When `0`, summons remain unscaled. |

---

### Section 7: Zone Profiles (World, Dungeon, Raid, Battleground, Arena)

Controls whether scaling is active in particular map contexts:

| Parameter Family | Default | Behavior |
|---|---|---|
| `ProgressionScaling.Profile.World.*` | `Enable = 1` | Full scaling active for open world maps (Continents, Eastern Kingdoms, Kalimdor, Outland, Northrend). |
| `ProgressionScaling.Profile.Dungeon.*` | `Enable = 1` | Instance profile for 5-man dungeons. Note that CreatureHealth is defaulted off (`0`) for AutoBalance safety. |
| `ProgressionScaling.Profile.Raid.*` | `Enable = 1` | Instance profile for raid maps. CreatureHealth is defaulted off (`0`). |
| `ProgressionScaling.Profile.Battleground.*` | `Enable = 0` | PvP Battleground profile (Warsong Gulch, Arathi Basin, Alterac Valley, etc.). Defaulted off. |
| `ProgressionScaling.Profile.Arena.*` | `Enable = 0` | PvP Arena profile (Nagrand Arena, Blade's Edge Arena, etc.). Defaulted off. |

---

### Section 8: Creature Rank Multipliers

Fine-tune scaling multipliers depending on creature rank in the `creature_template` database:

| Rank | Default Damage Multiplier | Default Health Multiplier | Description |
|---|---|---|---|
| `Rank.Normal.*` | `1.00` | `1.00` | Standard outdoor mobs. |
| `Rank.Elite.*` | `1.00` | `1.00` | Elite mobs (gold dragon border). |
| `Rank.RareElite.*` | `1.00` | `1.00` | Rare elite mobs (silver dragon with wings). |
| `Rank.WorldBoss.*` | `1.00` | `1.00` | Outdoor world bosses (e.g. Azuregos, Kazzak). |
| `Rank.Rare.*` | `1.00` | `1.00` | Standard rare spawns (silver dragon border). |

---

### Section 9: Class Balance Multipliers

Optional class-specific scaling for fine-tuning hybrid or scaling disparities:

```ini
ProgressionScaling.Class.Enable = 0
```
When enabled (`1`), each class (`Warrior`, `Paladin`, `Hunter`, `Rogue`, `Priest`, `DeathKnight`, `Shaman`, `Mage`, `Warlock`, `Druid`) can have independent modifiers for:
* `PlayerDamage` (e.g. `ProgressionScaling.Class.Warrior.PlayerDamage = 1.05`)
* `PlayerHealing` (e.g. `ProgressionScaling.Class.Priest.PlayerHealing = 1.00`)
* `PetDamage` (applied to hunter pets, warlock demons, mage water elementals, death knight ghouls).

---

### Section 10: Granular Rule Overrides

Rules allow surgical targeting of maps, zones, spells, or creature entries. Syntax uses pipe-delimited (`|`) strings.

#### A. Map & Zone Overrides
```ini
ProgressionScaling.MapOverrides.Enable = 1
ProgressionScaling.MapOverrides.Rules = "0:0:0:-1:PlayerDamage:0.98|33:0:0:1:PlayerDamage:0.95"
```
* **Format**: `map:zone:area:difficulty:effect:factor`
* **Resolution**: Exact area > Zone > Map; Exact difficulty > Wildcard (`-1`).

#### B. Spell Overrides & Exclusions
```ini
ProgressionScaling.SpellOverrides.Enable = 1
ProgressionScaling.SpellOverrides.Rules = "133:Spell:0.98|172:Periodic:exclude"
```
* **Format**: `spellId:category:factor-or-exclude`
* Use `exclude` to completely bypass progression scaling for that specific spell rank.

#### C. Creature Exclusions
```ini
ProgressionScaling.CreatureExclusions.Enable = 1
ProgressionScaling.CreatureExclusions.Rules = "674:CreatureHealth|674:PlayerDamage"
```
* **Format**: `creatureEntry:effect`
* Prevents scaling from applying to specific creature templates (e.g. quest escorts or training dummies).

---

### Section 11: Leveling Rates, Professions & Reputations

#### XP Rates
```ini
ProgressionScaling.Rates.Enable = 1
ProgressionScaling.Rates.KillXP = 1.00
ProgressionScaling.Rates.QuestXP = 1.00
ProgressionScaling.Rates.ExplorationXP = 1.00
```
* Applies purely in open-world context; does not alter Dungeon Finder or Battleground XP awards.
* **Bracket Rules**: `Rates.Brackets.Rules = "1:60:0.85~0.95~1.0:1.0:1.0|61:70:0.95:1.0:1.0"` allows dynamic XP bracket curves.

#### Profession Step Scaling
```ini
ProgressionScaling.Professions.Enable = 0
ProgressionScaling.Professions.CraftingStep = 1.00
ProgressionScaling.Professions.GatheringStep = 1.00
```
* Adjusts skill-up gain chances for tradeskills and gathering (Mining, Herbalism, Skinning). Fishing is handled natively.

#### Reputation Multipliers
```ini
ProgressionScaling.Reputation.Enable = 0
ProgressionScaling.Reputation.Factions = "72|69|930"
ProgressionScaling.Reputation.KillMultiplier = 1.00
ProgressionScaling.Reputation.QuestMultiplier = 1.00
```
* Selective reputation multiplier for specified faction IDs separated by `|`.

---

### Section 12: Solo Compensation & Level Delta Assistance

#### Solo Player Compensation
```ini
ProgressionScaling.SoloCompensation.Enable = 0
ProgressionScaling.SoloCompensation.Damage = 1.05
ProgressionScaling.SoloCompensation.Healing = 1.05
```
* Provides an output boost ($1.00$ to $1.15$) when an adventurer is playing solo without a party. If any party member or Playerbot joins, this bonus turns off automatically.

#### Level Delta Dynamic Adjustment
```ini
ProgressionScaling.LevelDelta.Enable = 0
ProgressionScaling.LevelDelta.Limit = 10
ProgressionScaling.LevelDelta.PerLevel = 0.01
ProgressionScaling.LevelDelta.MinMultiplier = 0.90
ProgressionScaling.LevelDelta.MaxMultiplier = 1.10
```
* Gradually adjusts outgoing damage against world mobs based on level disparity:
  $$\Delta = \text{Level}_{\text{player}} - \text{Level}_{\text{creature}}$$
  Adds a subtle, configurable slope ($PerLevel \times \Delta$) bounded by $[MinMultiplier, MaxMultiplier]$.

---

## 4. In-Game Chat Commands

Authorized GM characters (`SEC_MODERATOR` / `SEC_ADMINISTRATOR`) can diagnose and preview scaling calculations in real-time:

* `.progression status` - Shows active preset, level range, curve mode, SafeMode status, and AutoBalance integration.
* `.progression gear` - Calculates average equipped item level for target and same-map party members (requires `GearAwareness.Enable = 1`).
* `.progression curve <effect> [level]` - Previews exact mathematical curve multipliers for a given level.
* `.progression player [name]` - Displays active multipliers affecting the player or bot companion.
* `.progression creature [target]` - Inspects spawn health and damage multipliers for the selected NPC.
