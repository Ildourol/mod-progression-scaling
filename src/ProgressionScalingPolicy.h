#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ProgressionScaling
{
enum Effect : unsigned
{
    PlayerDamage, PlayerHealing, CreatureDamage, CreatureHealth, PetDamage, PetHealth, CreatureHealing, EffectCount
};
enum Profile : unsigned { World, Dungeon, Raid, Battleground, Arena, Unknown };
enum Category : unsigned { Swing, Spell, Periodic, DirectHeal, PeriodicHeal, CategoryCount };
enum Rate : unsigned { KillXP, QuestXP, ExplorationXP, RateCount };

inline constexpr std::array EffectNames{
    "PlayerDamage", "PlayerHealing", "CreatureDamage", "CreatureHealth", "PetDamage", "PetHealth", "CreatureHealing"
};
inline constexpr std::array ProfileNames{ "World", "Dungeon", "Raid", "Battleground", "Arena", "Unknown" };
inline constexpr std::array CategoryNames{ "Swing", "Spell", "Periodic", "DirectHeal", "PeriodicHeal" };
inline constexpr std::array RateNames{ "KillXP", "QuestXP", "ExplorationXP" };

constexpr double MinFactor = 0.10;
constexpr double MaxFactor = 3.00;
constexpr unsigned MaxLevel = 80;

struct Curve
{
    double start = 1;
    double end = 1;
    double power = 1;
};

inline constexpr std::array<Curve, EffectCount> Conservative{{
    { 1, .85, 1.35 }, { 1, .90, 1.25 }, { 1, .97, 1.10 }, { 1, 1.10, 1.25 },
    { 1, .85, 1.35 }, { 1, 1, 1 }, { 1, 1, 1 }
}};

inline constexpr std::array<Curve, EffectCount> VanillaInspired{{
    { 1, .80, 1.35 }, { 1, .85, 1.25 }, { 1, .97, 1.10 }, { 1, 1.15, 1.25 },
    { 1, .80, 1.35 }, { 1, 1, 1 }, { 1, 1, 1 }
}};

inline double Valid(double value, double minimum, double maximum, double fallback)
{
    return std::isfinite(value) && value >= minimum && value <= maximum ? value : fallback;
}

inline double Evaluate(unsigned level, unsigned first, unsigned last, Curve curve)
{
    curve.start = Valid(curve.start, MinFactor, MaxFactor, 1);
    curve.end = Valid(curve.end, MinFactor, MaxFactor, 1);
    curve.power = Valid(curve.power, .10, 8, 1);
    if (last <= first)
        return level < first ? curve.start : curve.end;

    double progress = double(std::clamp(level, first, last) - first) / (last - first);
    return std::clamp(curve.start + (curve.end - curve.start) * std::pow(progress, curve.power),
        MinFactor, MaxFactor);
}

inline uint32_t Scale(uint32_t amount, double factor, uint32_t maximum = std::numeric_limits<uint32_t>::max())
{
    if (amount == 0 || factor == 1 || !std::isfinite(factor) || factor <= 0 || maximum == 0)
        return amount;

    return static_cast<uint32_t>(std::clamp(std::floor(amount * factor + .5), 1.0, double(maximum)));
}

template <size_t Count>
using Factors = std::array<double, Count>;

template <size_t Count>
constexpr Factors<Count> Neutral()
{
    Factors<Count> result{};
    result.fill(1);
    return result;
}

struct MapProfile
{
    bool enabled = false;
    std::array<bool, EffectCount> participates{};
    Factors<EffectCount> factors = Neutral<EffectCount>();
};

// The longest location match wins per effect. Difficulty -1 means any mode.
struct Location
{
    uint32_t map = 0;
    uint32_t zone = 0;
    uint32_t area = 0;
    int difficulty = -1;

    bool operator==(Location const&) const = default;
};

struct LocationHash
{
    size_t operator()(Location const& key) const noexcept
    {
        size_t hash = key.map;
        for (uint32_t value : { key.zone, key.area, uint32_t(key.difficulty) })
            hash ^= size_t(value) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        return hash;
    }
};

struct RateBracket
{
    bool defined = false;
    Factors<RateCount> factors = Neutral<RateCount>();
};

struct Config
{
    bool enabled = false;
    bool safeMode = false;
    bool allowOutdoorPvP = true;
    bool allowCharmedUnits = true;
    bool autoBalance = true;
    bool bypassDungeonHealth = true;
    bool bypassRaidHealth = true;
    bool dungeonDamage = true;
    bool raidDamage = true;
    bool worldBossExempt = false;
    bool npcSummons = true;
    bool creatureHealing = false;
    bool classEnabled = false;
    bool ratesEnabled = false;
    bool soloEnabled = false;
    bool deltaEnabled = false;
    bool gearDiagnostics = false;
    bool professionRates = false;
    bool reputationRates = false;
    bool debug = false;
    std::array<bool, 4> debugChannel{};
    unsigned minLevel = 1;
    unsigned maxLevel = MaxLevel;
    int deltaLimit = 10;
    double deltaPerLevel = .01;
    double deltaMin = .9;
    double deltaMax = 1.1;
    double soloDamage = 1;
    double soloHealing = 1;
    double craftingStep = 1;
    double gatheringStep = 1;
    double reputationKill = 1;
    double reputationQuest = 1;
    std::array<MapProfile, Unknown> profiles{};
    std::array<Factors<EffectCount>, MaxLevel + 1> curves = []
    {
        std::array<Factors<EffectCount>, MaxLevel + 1> result;
        result.fill(Neutral<EffectCount>());
        return result;
    }();
    std::array<Factors<EffectCount>, 12> classes{};
    Factors<5> rankDamage = Neutral<5>();
    Factors<5> rankHealth = Neutral<5>();
    std::array<Factors<CategoryCount>, EffectCount> categories{};
    std::array<std::array<bool, CategoryCount>, EffectCount> categoryEnabled{};
    std::array<RateBracket, MaxLevel + 1> brackets{};
    Factors<RateCount> rates = Neutral<RateCount>();
    std::array<std::unordered_map<Location, double, LocationHash>, EffectCount> locations;
    std::array<std::unordered_map<uint32_t, double>, CategoryCount> spellFactors;
    std::array<std::unordered_set<uint32_t>, EffectCount> excludedEntries;
    std::unordered_set<uint32_t> reputationFactions;

    bool Participates(Profile profile, Effect effect) const
    {
        if (!enabled || profile == Unknown || !profiles[profile].enabled || !profiles[profile].participates[effect])
            return false;

        // PvP protection: safeMode blocks Battleground and Arena scaling
        if (safeMode && (profile == Battleground || profile == Arena))
            return false;

        // AutoBalance Instance Ownership Invariant:
        // Inside Dungeons and Raids, AutoBalance owns all level, creature, player, bot,
        // and pet scaling (health, damage, healing, armor, rewards).
        // ProgressionScaling MUST NOT compete with or double-apply changes in instances.
        if (profile == Dungeon || profile == Raid)
        {
            if (autoBalance)
                return false;

            if (safeMode)
                return false;

            if (effect == CreatureHealth)
                return !(profile == Raid ? bypassRaidHealth : bypassDungeonHealth);

            if (effect == CreatureDamage)
                return (profile != Dungeon || dungeonDamage) && (profile != Raid || raidDamage);
        }

        return true;
    }

    double CurveFactor(Effect effect, unsigned level) const
    {
        return curves[std::clamp(level, 1u, MaxLevel)][effect];
    }

    double LocationFactor(Profile profile, Effect effect, Location key) const
    {
        auto const& rules = locations[effect];
        if (!rules.empty())
        {
            for (Location const& match : { key, { key.map, key.zone, key.area, -1 },
                { key.map, key.zone, 0, key.difficulty }, { key.map, key.zone, 0, -1 },
                { key.map, 0, 0, key.difficulty }, { key.map, 0, 0, -1 } })
                if (auto it = rules.find(match); it != rules.end())
                    return it->second;
        }
        return profiles[profile].factors[effect];
    }

    double RateFactor(Rate rate, unsigned level) const
    {
        auto const& bracket = brackets[std::clamp(level, 1u, MaxLevel)];
        return bracket.defined ? bracket.factors[rate] : rates[rate];
    }

    double Delta(unsigned source, unsigned target) const
    {
        int difference = std::clamp(int(source) - int(target), -deltaLimit, deltaLimit);
        return std::clamp(1 + difference * deltaPerLevel, deltaMin, deltaMax);
    }
};
}
