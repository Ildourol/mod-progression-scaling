# Balance profiles

All preset curves start at 1.00. This table gives level 80 curve factors before rank, class, location or event modifiers. Pet health stays native.

| Preset | Player damage | Healing | NPC damage | World HP | Pet damage | Estimated equal-target TTK ratio |
|---|---:|---:|---:|---:|---:|---:|
| Neutral | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.000 |
| ConservativeSlow | 0.85 | 0.90 | 0.97 | 1.10 | 0.85 | 1.294 |
| BotFriendlySlow / GentleSlow | 0.88 | 0.92 | 0.97 | 1.08 | 0.88 | 1.227 |
| VanillaInspired (experimental) | 0.80 | 0.85 | 0.97 | 1.15 | 0.80 | 1.438 |
| Custom | Inherits ConservativeSlow for unset fields | | | | | Depends on overrides |

Conservative damage, healing, creature damage and creature health use exponents 1.35, 1.25, 1.10 and 1.25. Pet damage shares the player damage exponent. `.progression budget <level>` reports the curve-only world estimate. See [Balance](BALANCE.md) for the formula and measurement protocol.
