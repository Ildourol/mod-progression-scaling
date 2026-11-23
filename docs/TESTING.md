# Verification and reproducible commands

## Build and policy tests

| Target | Revision / environment | Observed |
|---|---|---|
| Stock AzerothCore, module alone | `7712427425dd893e18481826e4bbed56c31467a8` (`master`), MSVC x64 RelWithDebInfo, MySQL client toolchain | Native `modules` target compiled the loader and sources, generated `modules.lib`, and staged the distribution config. |
| Playerbots AzerothCore fork, module alone | `be923cbc45c3f1fab657f91d51fda999f6b81cc5` (`Playerbot`), MSVC x64 RelWithDebInfo, MySQL client toolchain | Native `modules` target compiled the loader and sources, generated `modules.lib`, and staged the distribution config. |
| Playerbot fork, dependency build with core PCH forced off | Same checkout | Core `common` failed before worldserver linkage. First observed errors were `ASSERT` in `OpenSSLCrypto.cpp`, `M_PI` in `Geometry.h` and missing `std::string` in `Tokenize.h`. The native module target was verified independently using `/p:BuildProjectReferences=false`. |
| Stock/Playerbot full worldserver link and live MySQL tests | Isolated stack required | Pending. No instance HP, bot behavior, combat or DB runtime observations are recorded. |
| Pure C++ policy | MSVC x64 Release, CTest | Pass. No server or database is required by this test executable. |

Create a clean test checkout with the named branch, check out the revision listed above, and stage `mod-progression-scaling` at `modules/mod-progression-scaling` in that checkout. Keep reference-only projects outside the test core. From the module root on Windows, using the host's Visual Studio and vcpkg paths:

```powershell
$cmake = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
$core = 'C:/path/to/test-core'
$build = 'C:/path/to/test-build'
& $cmake -S $core -B $build -G 'Visual Studio 17 2022' -A x64 `
    '-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake' `
    '-DVCPKG_TARGET_TRIPLET=x64-windows' '-DSCRIPTS=none' `
    '-DUSE_COREPCH=OFF' '-DUSE_SCRIPTPCH=OFF'
& $cmake --build $build --config RelWithDebInfo --target modules -- /p:BuildProjectReferences=false
& $cmake -S tests -B "$build/policy" -G 'Visual Studio 17 2022' -A x64
& $cmake --build "$build/policy" --config Release
& $ctest --test-dir "$build/policy" -C Release --output-on-failure
& "$build/policy/Release/progression_scaling_tests.exe" --benchmark
```

The module-only check omits dependency builds and is separate from a complete server build. For a server test, use its normal core settings and PCH, static gameplay scripts, `cmake --build ... --target worldserver`, an isolated MySQL database and matching data extract. Test stock alone, stock+AutoBalance, the Playerbot fork+Playerbots, and the Playerbot fork+Playerbots+AutoBalance as separate cases. The supported reference pair is Playerbots `master` at `63a27bf038a2215dc96d82b6a4b5027d4e8e9273` and the matching core branch `Playerbot`. AutoBalance was source-audited at `1d707cb61a0962ab01e4123681e2d3b66cc558a2`.

## Live test matrix

All rows below are pending. Record full build and module SHAs, map/difficulty, configuration, actors, baseline and enabled results for each. Capture actual worldserver values, especially absolute max HP for AutoBalance comparisons.

| Method | Expected | Observed / status |
|---|---|---|
| Two otherwise identical test runs across RFC, Deadmines, Scarlet Monastery, Stratholme, Ramparts, Utgarde Keep, Nexus and ICC. Compare max HP and group counts with AutoBalance alone versus AutoBalance + this module. | Exact same creature max HP and group counts under default policy. NPC damage composes once, or is identical to AutoBalance alone when opted out. | Pending |
| Compare targets at levels 1/10/20/40/60/70/80. Distinguish melee crit/noncrit, Hunter Auto Shot, other ranged and melee specials, direct casts, periodic ticks and ordinary triggers. | Native misses, mitigation, block and absorb remain. Each eligible magnitude is adjusted once. Practice dummy used for player output. | Pending |
| Cast direct, self, HoT, mixed direct/HoT, leech, health transfer and shield spells. Compare log and actual HP. | Eligible heals scale once. Mixed direct component, absorb, leech and health transfer remain native. | Pending |
| Reflect spell, trigger Ignite/Deep Wounds, fall, drown, use a GM ability and compare a scripted encounter mechanic. | Audited exclusions stay neutral. Identify any unclassified triggered/reflected events and record spell IDs. | Pending |
| Check ordinary/elite/rare mobs, spawn-loaded HP, friendly, passive, quest NPCs, world/dungeon bosses and target dummies. Reload mid-fight, evade, respawn, reload with master off. | Eligible fresh spawn/respawn uses generated HP once. No module HP changes mid-fight. Restricted actors remain native. | Pending |
| Summon hunter/warlock/DK/mage/shaman pets or guardians and move maps. Repeat with Playerbot owners. | One pet output factor based on owner level. Native pet HP survives transfers and resummons. Ownership loss stays neutral. | Pending |
| Test duel, open-world PvP, arena and battleground, including PvP healing. | Native output under default settings. | Pending |
| Install specific location and spell rules, including a conflicting area/map, a duplicate row and a disabled category. Reload. | Most-specific location wins per effect. Exclusion overrides factors. Invalid rows log. Instance HP remains protected. | Pending |
| Check native XP at 1, 59→60, 60→61, 69→70, 70→71 and 79→80 with kills, quests, exploration, rested, RAF, XP lock and event bonuses. | Only relevant award stage scales. Native locks, bonus handling and maximum level remain. | Pending |
| Gather, craft, fish and earn positive/negative reputation at standing limits and on opted-out factions. | Eligible skill-step and reputation changes only. Native skill chance, fishing, item products and standing enforcement remain. | Pending |
| Use `.progression gear` on empty and equipped characters, dual wield and two-handed gear, bots-only and mixed parties. Join/leave and change equipment or map. | Read-only own and same-map group sample, all actor types included, no stale cache and no gear modifications. | Pending |
| Compare no group, a human+bot duo, five-player and raid contexts. Record threat, mana, deaths, duration and pet contribution. | Solo compensation only for an ungrouped outdoor owner when enabled. Optional level delta remains PvE world-only. | Pending |

## Synthetic benchmark

The benchmark performs five million simple policy events per mode. It reads the per-level cache and measures arithmetic and an optional hash lookup. It deliberately excludes world objects, script dispatch, atomic snapshot contention, server logging and MySQL. Use its checksum to verify disabled and neutral cases are identical. Live map-worker CPU and memory measurements remain pending.

| MSVC x64 Release mode | 5,000,000 events | Checksum |
|---|---:|---:|
| Disabled | 6.17 ms | 702500000 |
| Neutral | 10.24 ms | 702500000 |
| Cached curve | 30.86 ms | 652437500 |
| Cached curve and location override | 115.29 ms | 632812500 |

The snapshot object occupied 9,936 bytes before allocated rule nodes and shared ownership bookkeeping. Timings are one host run and vary with system load.
