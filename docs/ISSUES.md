# Issue Registry: `mod-progression-scaling`

This document tracks all audited defects, verified fixes, and architectural invariants for `mod-progression-scaling`.

---

## Issue Index

| Issue ID | Severity | Status | Title | Description & Resolution |
|---|---|---|---|---|
| **ISSUE-001** | Medium | **RESOLVED** | Missing Legacy Script Loader Alias | Older CMake configurations and external script runners expect `AddSC_mod_progression_scaling()`. Resolved by adding the alias to `src/loader.cpp`. |
| **ISSUE-002** | Medium | **RESOLVED** | Missing Automated Python Regression Test Suite | The module lacked a standalone non-server Python test runner runnable via `tests/run_all_tests.py`. Resolved by authoring `tests/test_progression_scaling.py`. |
| **ISSUE-003** | Low | **VERIFIED** | AutoBalance Double Health Scaling Protection | Investigated risk of double-scaling creature health alongside `mod-autobalance`. Verified that `SafeMode = 1` and `MatchesGeneratedHealth()` effectively prevent conflicting mutations. |
| **ISSUE-004** | Low | **RESOLVED** | Missing Module Workspace Integration Artifacts | Missing `AGENTS.md`, `docs/ISSUES.md`, and maintainer skills causing workspace audit warnings. Resolved by authoring sovereign guidelines and maintainer skills. |
| **ISSUE-005** | High | **RESOLVED** | AutoBalance Instance Overlap & Double-Scaling Prevention | Detailed compatibility audit against installed `mod-autobalance` revealed that dungeon creature damage, player damage, and healing could double-apply in instances. Resolved by establishing strict AutoBalance instance exclusivity, suppressing all instance combat factors when AutoBalance is active, and strengthening playerbot/pet ownership resolution. |
| **ISSUE-006** | High | **RESOLVED** | VanillaInspired Preset Full Scope & AutoBalance Double-Scaling Guard | Re-aligned `AutoBalanceCompat.Enable` to default true so `mod-autobalance` retains exclusive ownership of dungeons/raids without double application. Completed the full VanillaInspired scope by enabling authentic elite rank multipliers (1.15 dmg, 1.25 hp), rare elite multipliers (1.20 dmg, 1.30 hp), LevelDelta scaling (punishing orange/red mobs), and classic 1-60 XP leveling rates. |

---

## Detailed Issue Records

### ISSUE-001: Missing Legacy Script Loader Alias
- **Category**: Linkage & Compatibility
- **Description**: `src/loader.cpp` only defined `Addmod_progression_scalingScripts()`. While modern AzerothCore dynamic module loading supports this, legacy setups and standalone test harnesses call `AddSC_mod_progression_scaling()`.
- **Fix**: Added `AddSC_mod_progression_scaling()` forwarding to `Addmod_progression_scalingScripts()`.

### ISSUE-002: Missing Automated Python Regression Test Suite
- **Category**: Quality Assurance & Regression Testing
- **Description**: Unit tests only existed in `tests/policy_tests.cpp` which requires a full C++ compiler. Under Zero-Prompt Auto Mode and non-server testing constraints, an autonomous Python test suite is mandatory.
- **Fix**: Authored `tests/test_progression_scaling.py` covering curve evaluation formulas, boundary clamps, config validation, loader symbol presence, and AutoBalance safety invariants.

### ISSUE-003: AutoBalance Double Health Scaling Protection
- **Category**: Interoperability
- **Description**: When both `mod-autobalance` and `mod-progression-scaling` are active in dungeons/raids, both might attempt to scale creature health.
- **Verification**: Verified that `ProgressionScalingPolicy::IsScalingAllowed` checks `safeMode` and `bypassDungeonHealth`/`bypassRaidHealth`. When safe mode is enabled, dungeon/raid creature health scaling is completely suppressed in ProgressionScaling.

### ISSUE-004: Missing Module Workspace Integration Artifacts
- **Category**: Documentation & Orchestration
- **Description**: `tools/audit_modules.py` flagged `mod-progression-scaling` for missing `AGENTS.md`.
- **Fix**: Generated sovereign `AGENTS.md`, `docs/ISSUES.md`, and maintainer skill `progression-scaling-maintainer`.

### ISSUE-006: VanillaInspired Preset Full Scope & AutoBalance Double-Scaling Guard
- **Category**: Balance, Interoperability & Configuration
- **Description**: While `VanillaInspired` curve was defined, the full scope of the Vanilla profile was incomplete: Elite rank multipliers remained at 1.00 instead of authentic Vanilla danger (1.15/1.25 for Elites, 1.20/1.30 for Rare Elites), `LevelDelta` scaling was disabled (pulling red/orange mobs carried no extra penalty), and leveling rates were at 1.00 instead of classic 1-60 pacing (0.80 KillXP, 0.85 QuestXP, 0.80 ExplorationXP). Furthermore, a previous config update set `AutoBalanceCompat.Enable = 0`, causing `config->autoBalance` to evaluate false and leading to double-scaling inside dungeons and raids.
- **Fix**:
  1. Updated `ProgressionScalingConfig.cpp` AutoBalance detection to default `AutoBalanceCompat.Enable` to `true` and check active `mod-autobalance` configuration.
  2. Set `ProgressionScaling.conf.dist` defaults: `AutoBalanceCompat.Enable = 1`, `DisableDungeonCreatureHealth = 1`, `DisableRaidCreatureHealth = 1`, `AllowDungeonCreatureDamage = 0`, `AllowRaidCreatureDamage = 0`.
  3. Configured `ProgressionScaling.conf.dist` with authentic Vanilla values:
     - Rank: Elite Damage 1.15, Elite Health 1.25, RareElite Damage 1.20, RareElite Health 1.30.
     - LevelDelta: Enabled (1), Limit 10, PerLevel 0.02, Min 0.75, Max 1.20.
     - Rates: Enabled (1), KillXP 0.80, QuestXP 0.85, ExplorationXP 0.80.
     - SoloCompensation: Disabled (0).
  4. Expanded test suite (`test_progression_scaling.py`) with 15 passing unit tests verifying mathematical curves, boundary clamping, VanillaInspired defaults, and AutoBalance exclusivity invariants.

