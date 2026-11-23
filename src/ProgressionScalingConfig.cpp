#include "ProgressionScaling.h"
#include "Config.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "WorldScript.h"
#include <atomic>
#include <charconv>
#include <string_view>

namespace ProgressionScaling
{
namespace
{
std::atomic<std::shared_ptr<Config const>> Current{ std::make_shared<Config>() };
constexpr char Prefix[] = "ProgressionScaling.";

template <typename T>
T Read(std::string const& key, T fallback)
{
    return sConfigMgr->GetOption<T>(Prefix + key, fallback, false);
}

template <typename T>
bool Parse(std::string_view value, T& output)
{
    if (value.empty())
        return false;
    auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), output);
    return error == std::errc{} && end == value.data() + value.size();
}

double Number(std::string const& key, double fallback, double minimum = MinFactor, double maximum = MaxFactor)
{
    std::string value = Read(key, std::string("preset"));
    if (value == "preset")
        return fallback;

    double result = 0;
    if (Parse(value, result) && Valid(result, minimum, maximum, -1) != -1)
        return result;

    LOG_WARN("module", "ProgressionScaling: {} invalid; using {}", key, fallback);
    return fallback;
}

template <typename Names>
unsigned FindName(Names const& names, std::string_view name)
{
    return static_cast<unsigned>(std::find(names.begin(), names.end(), name) - names.begin());
}

std::vector<std::string_view> Split(std::string_view value, char delimiter)
{
    std::vector<std::string_view> fields;
    while (!value.empty())
    {
        size_t end = value.find(delimiter);
        auto field = value.substr(0, end);
        size_t first = field.find_first_not_of(" \t");
        fields.push_back(first == field.npos ? std::string_view{} :
            field.substr(first, field.find_last_not_of(" \t") - first + 1));
        if (end == value.npos)
            break;
        value.remove_prefix(end + 1);
    }
    return fields;
}

template <typename AddRow>
void ReadRows(std::string const& key, AddRow addRow)
{
    std::string value = Read(key, std::string{});
    unsigned number = 0;
    for (std::string_view row : Split(value, '|'))
    {
        ++number;
        if (!addRow(Split(row, ':')))
            LOG_WARN("module", "ProgressionScaling: ignored invalid or duplicate {} row {}", key, number);
    }
}

bool ParseFactor(std::string_view text, double& factor)
{
    return Parse(text, factor) && Valid(factor, MinFactor, MaxFactor, -1) != -1;
}

bool ParseRateCurve(std::string_view text, Curve& curve)
{
    auto fields = Split(text, '~');
    if (fields.size() == 1 && ParseFactor(fields[0], curve.start))
    {
        curve.end = curve.start;
        return true;
    }
    return fields.size() == 3 && ParseFactor(fields[0], curve.start) && ParseFactor(fields[1], curve.end) &&
        Parse(fields[2], curve.power) && Valid(curve.power, .10, 8, -1) != -1;
}

void LoadOverrides(Config& config)
{
    auto readLocations = [&](char const* family, bool zones)
    {
        std::string base = family;
        if (!Read(base + ".Enable", false))
            return;
        ReadRows(base + ".Rules", [&](auto const& fields)
        {
            Location key;
            double factor = 1;
            if (fields.size() != 6 || !Parse(fields[0], key.map) || !Parse(fields[1], key.zone) ||
                !Parse(fields[2], key.area) || !Parse(fields[3], key.difficulty) ||
                !ParseFactor(fields[5], factor) || key.difficulty < -1 || key.difficulty > 3 ||
                (key.area && !key.zone) || (zones != (key.zone != 0)))
                return false;
            unsigned effect = FindName(EffectNames, fields[4]);
            return effect < EffectCount && effect != PetHealth && config.locations[effect].emplace(key, factor).second;
        });
    };
    readLocations("MapOverrides", false);
    readLocations("ZoneOverrides", true);

    if (Read("SpellOverrides.Enable", false))
        ReadRows("SpellOverrides.Rules", [&](auto const& fields)
        {
            uint32_t id = 0;
            if (fields.size() != 3 || !Parse(fields[0], id) || !id)
                return false;
            unsigned category = FindName(CategoryNames, fields[1]);
            double factor = 0;
            if (category >= CategoryCount || category == Swing ||
                (fields[2] != "exclude" && !ParseFactor(fields[2], factor)))
                return false;
            return config.spellFactors[category].emplace(id, factor).second;
        });

    if (Read("CreatureExclusions.Enable", false))
        ReadRows("CreatureExclusions.Rules", [&](auto const& fields)
        {
            uint32_t id = 0;
            if (fields.size() != 2 || !Parse(fields[0], id) || !id)
                return false;
            unsigned effect = FindName(EffectNames, fields[1]);
            return effect < EffectCount && effect != PetHealth && config.excludedEntries[effect].insert(id).second;
        });
}

void LoadRewards(Config& config)
{
    config.professionRates = Read("Professions.Enable", false);
    config.craftingStep = Number("Professions.CraftingStep", 1, 1, 3);
    config.gatheringStep = Number("Professions.GatheringStep", 1, 1, 3);
    config.reputationRates = Read("Reputation.Enable", false);
    config.reputationKill = Number("Reputation.KillMultiplier", 1);
    config.reputationQuest = Number("Reputation.QuestMultiplier", 1);
    if (config.reputationRates)
        ReadRows("Reputation.Factions", [&](auto const& fields)
        {
            uint32_t id = 0;
            return fields.size() == 1 && Parse(fields[0], id) && id != 0 && config.reputationFactions.insert(id).second;
        });
}

void LoadRates(Config& config)
{
    config.ratesEnabled = Read("Rates.Enable", false);
    for (unsigned rate = 0; rate < RateCount; ++rate)
        config.rates[rate] = Number(std::string("Rates.") + RateNames[rate], 1);

    if (config.ratesEnabled && Read("Rates.Brackets.Enable", false))
        ReadRows("Rates.Brackets.Rules", [&](auto const& fields)
        {
            unsigned first = 0;
            unsigned last = 0;
            std::array<Curve, RateCount> curves;
            if (fields.size() != 5 || !Parse(fields[0], first) || !Parse(fields[1], last) ||
                first < 1 || first > last || last > MaxLevel)
                return false;
            for (unsigned rate = 0; rate < RateCount; ++rate)
                if (!ParseRateCurve(fields[rate + 2], curves[rate]))
                    return false;
            for (unsigned level = first; level <= last; ++level)
                if (config.brackets[level].defined)
                    return false;
            for (unsigned level = first; level <= last; ++level)
            {
                auto& bracket = config.brackets[level];
                bracket.defined = true;
                for (unsigned rate = 0; rate < RateCount; ++rate)
                    bracket.factors[rate] = Evaluate(level, first, last, curves[rate]);
            }
            return true;
        });
}

void LoadCurves(Config& config, std::string const& preset)
{
    auto curves = VanillaInspired;
    if (preset == "Neutral")
        curves.fill(Curve{});
    else if (preset == "ConservativeSlow")
        curves = Conservative;
    else if (preset == "BotFriendlySlow" || preset == "GentleSlow")
    {
        curves[PlayerDamage].end = curves[PetDamage].end = .88;
        curves[PlayerHealing].end = .92;
        curves[CreatureHealth].end = 1.08;
    }
    else if (preset == "VanillaInspired")
        curves = VanillaInspired;

    bool curveEnabled = Read("Curve.Enable", true);
    unsigned mode = Read("Curve.Mode", 1u);
    if (mode > 1)
    {
        LOG_WARN("module", "ProgressionScaling: unknown Curve.Mode; using power mode");
        mode = 1;
    }
    for (unsigned effect = 0; effect < EffectCount; ++effect)
    {
        std::string key = EffectNames[effect];
        Curve curve = curves[effect];
        curve.start = Number(key + ".Start", curve.start);
        curve.end = Number(key + ".End", curve.end);
        curve.power = Number(key + ".Power", curve.power, .10, 8);
        if (mode == 0)
            curve.power = 1;
        bool active = effect != PetHealth && curveEnabled && Read(key + ".Enable", effect != CreatureHealing);
        if (effect == CreatureHealing)
            config.creatureHealing = Read(key + ".Enable", false);
        for (unsigned level = 1; level <= MaxLevel; ++level)
            config.curves[level][effect] = active ? Evaluate(level, config.minLevel, config.maxLevel, curve) : 1;
    }
}

void LoadProfiles(Config& config)
{
    for (unsigned profile = 0; profile < Unknown; ++profile)
    {
        auto& target = config.profiles[profile];
        std::string prefix = std::string("Profile.") + ProfileNames[profile] + '.';
        target.enabled = Read(prefix + "Enable", true);
        for (unsigned effect = 0; effect < EffectCount; ++effect)
        {
            std::string key = prefix + EffectNames[effect];
            bool participates = effect != PetHealth;
            target.participates[effect] = effect != PetHealth && Read(key + ".Enable", participates);
            target.factors[effect] = effect == PetHealth ? 1 : Number(key + "Multiplier", 1);
        }
    }

    constexpr std::array rankNames{ "Normal", "Elite", "RareElite", "WorldBoss", "Rare" };
    for (unsigned rank = 0; rank < rankNames.size(); ++rank)
    {
        std::string key = std::string("Rank.") + rankNames[rank];
        config.rankDamage[rank] = Number(key + ".DamageMultiplier", 1);
        config.rankHealth[rank] = Number(key + ".HealthMultiplier", 1);
    }

    constexpr std::array classNames{ "", "Warrior", "Paladin", "Hunter", "Rogue", "Priest", "DeathKnight", "Shaman",
        "Mage", "Warlock", "", "Druid" };
    config.classEnabled = Read("Class.Enable", false);
    for (unsigned id = 0; id < classNames.size(); ++id)
    {
        config.classes[id] = Neutral<EffectCount>();
        if (config.classEnabled && *classNames[id])
            for (Effect effect : { PlayerDamage, PlayerHealing, PetDamage })
                config.classes[id][effect] = Number(
                    std::string("Class.") + classNames[id] + '.' + EffectNames[effect], 1);
    }

    for (Effect effect : { PlayerDamage, CreatureDamage, PetDamage, PlayerHealing, CreatureHealing })
        for (unsigned category = 0; category < CategoryCount; ++category)
        {
            bool healing = effect == PlayerHealing || effect == CreatureHealing;
            if (healing != (category >= DirectHeal))
                continue;
            std::string key = std::string(EffectNames[effect]) + '.' + CategoryNames[category];
            config.categoryEnabled[effect][category] = Read(key + ".Enable", true);
            config.categories[effect][category] = Number(key + "Multiplier", 1);
        }
}

class ConfigScript final : public WorldScript
{
public:
    ConfigScript() : WorldScript("ProgressionScalingConfig", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        auto config = std::make_shared<Config>();
        config->enabled = Read("Enable", true);
        config->safeMode = Read("Compatibility.SafeMode", false);
        config->allowOutdoorPvP = Read("Compatibility.AllowOutdoorPvP", true);
        config->allowCharmedUnits = Read("Compatibility.AllowCharmedUnits", true);

        // AutoBalance detection: check if AutoBalance is active in world server configuration
        bool autoBalanceDetected = sConfigMgr->GetOption<bool>("AutoBalance.Enable.Global",
            sConfigMgr->GetOption<bool>("AutoBalance.enable", true, false), false);
        bool autoBalanceCompat = Read("AutoBalanceCompat.Enable", true);
        config->autoBalance = autoBalanceCompat && autoBalanceDetected;
        config->bypassDungeonHealth = Read("AutoBalanceCompat.DisableDungeonCreatureHealth", true);
        config->bypassRaidHealth = Read("AutoBalanceCompat.DisableRaidCreatureHealth", true);
        config->dungeonDamage = Read("AutoBalanceCompat.AllowDungeonCreatureDamage", false);
        config->raidDamage = Read("AutoBalanceCompat.AllowRaidCreatureDamage", false);
        config->worldBossExempt = Read("WorldDifficulty.WorldBossExempt", false);
        config->npcSummons = Read("CreatureDamage.NpcSummons", true);
        config->gearDiagnostics = Read("GearAwareness.Enable", false);
        config->debug = Read("Debug", false);
        config->debugChannel = { Read("Debug.Combat", false), Read("Debug.Creatures", false),
            Read("Debug.Pets", false), Read("Debug.Compatibility", false) };
        config->minLevel = std::clamp(Read("MinLevel", 1u), 1u, MaxLevel);
        config->maxLevel = std::clamp(Read("MaxLevel", MaxLevel), 1u, MaxLevel);
        if (config->minLevel > config->maxLevel)
        {
            LOG_WARN("module", "ProgressionScaling: reversed level range; using 1-80");
            config->minLevel = 1;
            config->maxLevel = MaxLevel;
        }

        std::string preset = Read("Preset", std::string("VanillaInspired"));
        if (preset != "Neutral" && preset != "BotFriendlySlow" && preset != "GentleSlow" &&
            preset != "VanillaInspired" && preset != "ConservativeSlow" && preset != "Custom")
        {
            LOG_WARN("module", "ProgressionScaling: unknown preset {}; using VanillaInspired", preset);
            preset = "VanillaInspired";
        }
        LoadCurves(*config, preset);
        LoadProfiles(*config);
        LoadOverrides(*config);
        LoadRates(*config);
        LoadRewards(*config);

        config->soloEnabled = Read("SoloCompensation.Enable", false);
        config->soloDamage = Number("SoloCompensation.Damage", 1, 1, 1.15);
        config->soloHealing = Number("SoloCompensation.Healing", 1, 1, 1.15);
        config->deltaEnabled = Read("LevelDelta.Enable", false);
        config->deltaLimit = std::clamp(Read("LevelDelta.Limit", 10), 0, 30);
        config->deltaPerLevel = Number("LevelDelta.PerLevel", .01, 0, .03);
        config->deltaMin = Number("LevelDelta.MinMultiplier", .9, .75, 1);
        config->deltaMax = Number("LevelDelta.MaxMultiplier", 1.1, 1, 1.25);

        if (config->Participates(Dungeon, CreatureHealth) || config->Participates(Raid, CreatureHealth))
            LOG_WARN("module", "ProgressionScaling: instance creature HP opt-in may overlap AutoBalance");
        if (config->debug && config->debugChannel[3])
            LOG_INFO("module", "ProgressionScaling: safe mode={}, instance HP dungeon/raid={}/{}, "
                "bot detection unavailable", config->safeMode, config->Participates(Dungeon, CreatureHealth),
                config->Participates(Raid, CreatureHealth));
        LOG_INFO("module", "ProgressionScaling: {} {} ({}, levels {}-{})", reload ? "reloaded" : "loaded",
            config->enabled ? "enabled" : "disabled", preset, config->minLevel, config->maxLevel);
        Current.store(std::move(config), std::memory_order_release);
    }
};
}

std::shared_ptr<Config const> GetConfig()
{
    return Current.load(std::memory_order_acquire);
}

void RegisterConfig()
{
    new ConfigScript();
}
}
