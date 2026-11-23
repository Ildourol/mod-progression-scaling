# Progression Scaling for AzerothCore

![Progression Scaling Banner](docs/images/progression_scaling_banner.jpg)

[![AzerothCore](https://img.shields.io/badge/AzerothCore-WotLK%203.3.5a-blue.svg)](https://www.azerothcore.org/)
[![C++](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)]()
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-yellow.svg)](LICENSE)

A high-performance, non-intrusive mathematical progression and combat scaling engine for **AzerothCore (WotLK 3.3.5a)**. It dynamically scales player and creature combat output, health, and leveling progression across levels 1–80 through smooth interpolation curves while seamlessly coexisting with **AutoBalance** and **Playerbots**.

---

## 🌟 Highlights & Key Features

* **Mathematical Curve Engine (Levels 1–80)**:
  * Dynamically scales player damage/healing, pet damage, and hostile creature damage/health as characters level up.
  * Powered by bounded mathematical formulas (Linear mode or Polynomial Power exponent curves) rather than static lookup tables.
* **Non-Intrusive & Zero Database Mutations**:
  * Pure C++ execution via native `ScriptMgr` lifecycle hooks (`OnModifyMeleeDamage`, `OnModifySpellDamageDone`, `OnHeal`, `OnCreatureAddWorld`, `OnPlayerGiveXP`).
  * No core modifications, no client patches, and no permanent database table alterations required.
* **AutoBalance & Playerbot Interoperability**:
  * Full native cooperation with `mod-autobalance`: recognizes AutoBalance's authoritative ownership of dungeon/raid instances, automatically suppressing instance creature health scaling to prevent double-scaling.
  * Treats playerbot companions and summoners with native player/owner logic.
* **Granular Multi-Tier Customization**:
  * **Combat Categories**: Fine-tune scaling for White hits (`Swing`), Abilities/Spells (`Spell`), DoTs (`Periodic`), and HoTs/Heals (`DirectHeal`, `PeriodicHeal`).
  * **Zone & Map Profiles**: Independent multipliers for Open World, Dungeons, Raids, Battlegrounds, and Arenas.
  * **Class & Rank Multipliers**: Class-specific balance tuning and creature rank multipliers (`Normal`, `Elite`, `RareElite`, `WorldBoss`).
  * **Level-Delta Penalties**: Configurable penalty slope when engaging orange/red mobs higher than the player's level.
  * **Progression Rewards**: Optional multipliers for Kill/Quest/Exploration XP, crafting/gathering profession skill-ups, and faction reputations.
  * **Safe Mode Interlock**: Instant toggle to guarantee PvP combat, Arenas, and Battlegrounds remain 100% pure vanilla/unscaled.

---

## 📐 Progression Curve Presets

Presets provide ready-to-play power curves out of the box (configured in `ProgressionScaling.Preset`):

| Preset | Start (Lvl 1) | End (Lvl 80) | Power | Pacing Feel |
|---|---|---|---|---|
| `VanillaInspired` *(Default)* | `0.80` | `1.00` | `1.35` | Gritty early-game pacing mimicking classic Vanilla scarcity; challenging outdoor combat. |
| `ConservativeSlow` | `0.85` | `1.00` | `1.20` | Gentle power curve; smooth leveling pacing reaching full native balance by level 80. |
| `Moderate` | `0.75` | `1.00` | `1.25` | Medium progression curve suitable for blended solo and small-group servers. |
| `Steep` | `0.65` | `1.00` | `1.40` | Accelerated power growth for fast-paced or boosted progression realms. |
| `Neutral` | `1.00` | `1.00` | `1.00` | Flat 1.0 multiplier across all levels (passthrough / no scaling applied). |

> [!NOTE]
> All curves can be overridden on a per-effect basis (`PlayerDamage`, `PlayerHealing`, `CreatureDamage`, `CreatureHealth`, `PetDamage`, `CreatureHealing`) using custom numeric values in the configuration file.

---

## 🛠️ Installation & Building

1. Clone or copy this repository into your AzerothCore modules directory:
   ```bash
   cd /path/to/azerothcore-wotlk/modules
   git clone https://github.com/Ildourol/mod-progression-scaling.git
   ```

2. Re-run CMake and recompile `worldserver`:
   ```bash
   # Configure
   cmake -S /path/to/azerothcore-wotlk -B /path/to/build -DMODULES=static

   # Build
   cmake --build /path/to/build --config RelWithDebInfo --target worldserver
   ```

3. Copy the configuration file:
   * **Windows**: Copy `modules/mod-progression-scaling/conf/ProgressionScaling.conf.dist` to `bin/configs/modules/ProgressionScaling.conf`.
   * **Linux**: Copy `modules/mod-progression-scaling/conf/ProgressionScaling.conf.dist` to `etc/modules/ProgressionScaling.conf`.

4. Start your `worldserver`. The module will automatically detect configuration files, verify AutoBalance coexistence status, and log initialization telemetry.

---

## 🎮 In-Game Commands

The module includes comprehensive diagnostic and telemetry commands (RBAC permission matching `server reload config`):

| Command | Description |
|---|---|
| `.progression status` | Displays active preset, level range, curve mode, SafeMode status, and AutoBalance integration. |
| `.progression curve <effect> [level]` | Previews exact mathematical curve multipliers for a given level (1–80). |
| `.progression player [name]` | Displays active combat multipliers affecting the player or companion bot. |
| `.progression creature` | Inspects spawn health and damage multipliers for the selected target NPC. |
| `.progression gear` | Inspects average equipped item level for target and party members (requires `GearAwareness.Enable = 1`). |
| `.progression reload` | Hot-reloads `ProgressionScaling.conf` in real-time without restarting the server. |

---

## 📚 Technical Documentation

Comprehensive architectural references and audit reports are available in the [`docs/`](docs/) directory:

* 📖 **[Configuration & Architecture Guide](docs/CONFIGURATION_GUIDE.md)**: Deep dive into all configuration settings, formulas, and tuning advice.
* ⚙️ **[Configuration Reference](docs/CONFIGURATION.md)**: Quick-reference table of configuration keys and rule syntax.
* 🛡️ **[Compatibility & Safety Limits](docs/COMPATIBILITY.md)**: Details on AutoBalance interoperability, PvP safe modes, and hook boundaries.
* ⚔️ **[Conflict Matrix](docs/CONFLICT_MATRIX.md)**: Analysis of interactions with other popular AzerothCore modules.
* 📊 **[Balance Analysis](docs/BALANCE.md)** & **[Preset Comparison](docs/BALANCE_PROFILES.md)**: Simulation data and curve graphs.
* 🧪 **[Testing Framework](docs/TESTING.md)**: Validation criteria and testing matrix.
* 📋 **[Feature Matrix](docs/FEATURE_MATRIX.md)** & **[Implementation Issues](docs/ISSUES.md)**: Engineering milestones and audited items.
* 🔍 **[Reference Audit](docs/REFERENCE_AUDIT.md)**, **[Provenance](docs/CODE_PROVENANCE.md)** & **[Source-to-Feature Matrix](docs/SOURCE_TO_FEATURE_MATRIX.md)**.

---

## 📄 License

This module is released under the **GNU General Public License v2 (GPL-2.0)** to match the AzerothCore open-source ecosystem. See [LICENSE](LICENSE) for full details.
