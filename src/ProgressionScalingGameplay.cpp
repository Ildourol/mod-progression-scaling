#include "ProgressionScaling.h"
#include "AllCreatureScript.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "UnitScript.h"
#include "World.h"
#include <atomic>
#include <chrono>

namespace ProgressionScaling
{
Profile GetProfile(Map const* map)
{
    if (!map || !map->GetEntry())
        return Unknown;
    if (map->IsBattleArena())
        return Arena;
    if (map->IsBattleground())
        return Battleground;
    if (map->IsRaid())
        return Raid;
    if (map->IsDungeon())
        return Dungeon;
    return map->GetEntry()->IsWorldMap() ? World : Unknown;
}

Location GetLocation(Unit const& actor)
{
    Location result{ actor.GetMapId() };
    if (Map const* map = actor.FindMap())
    {
        actor.GetZoneAndAreaId(result.zone, result.area);
        result.difficulty = int(map->GetDifficulty());
    }
    return result;
}

Player const* GetPlayerOwner(Unit const& actor)
{
    if (Player const* player = actor.ToPlayer())
        return player;
    if (Player const* player = actor.GetCharmerOrOwnerPlayerOrPlayerItself())
        return player;
    if (actor.GetCharmerOrOwnerGUID())
    {
        if (Player const* player = actor.GetAffectingPlayer())
            return player;
    }
    if (Creature const* creature = actor.ToCreature())
    {
        ObjectGuid summoner = creature->GetSummonerGUID();
        if (summoner.IsPlayer())
            return ObjectAccessor::GetPlayer(actor, summoner);
    }
    return nullptr;
}

bool EligibleCreature(Config const& config, Creature const& creature, Effect effect)
{
    CreatureTemplate const* info = creature.GetCreatureTemplate();
    if (!info || creature.IsTrigger() || creature.IsTotem() || creature.IsPet() || creature.IsGuardian() ||
        creature.IsControlledByPlayer() || creature.GetCharmerOrOwnerGUID() ||
        creature.GetCreatureType() == CREATURE_TYPE_CRITTER ||
        creature.GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET ||
        creature.HasFlagsExtra(CREATURE_FLAG_EXTRA_CIVILIAN) || creature.GetReactState() == REACT_PASSIVE ||
        (config.worldBossExempt && info->rank == CREATURE_ELITE_WORLDBOSS) ||
        config.excludedEntries[effect].contains(creature.GetEntry()))
        return false;

    if (effect == CreatureHealth)
    {
        auto const* faction = creature.GetFactionTemplateEntry();
        return creature.GetSpawnId() && creature.GetCreatureData() && !creature.IsSummon() &&
            !creature.GetScriptId() && !info->npcflag && !creature.GetNpcFlags() && info->RegenHealth &&
            faction && (faction->IsHostileToPlayers() || faction->IsHostileToAlliancePlayers() ||
                faction->IsHostileToHordePlayers() || faction->IsNeutralToAll()) && !creature.IsContestedGuard() &&
            !creature.HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE) && !creature.IsDungeonBoss();
    }

    return !creature.IsSummon() || (config.npcSummons && effect == CreatureDamage &&
        creature.GetSummonerGUID().IsCreature());
}

void DebugEvent(Config const& config, unsigned channel, char const* effect, uint32_t before, uint32_t after)
{
    if (!config.debug || !config.debugChannel[channel] || before == after)
        return;
    static std::array<std::atomic<int64_t>, 3> next{};
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous = next[channel].load(std::memory_order_relaxed);
    if (now >= previous && next[channel].compare_exchange_strong(previous, now + 10, std::memory_order_relaxed))
        LOG_INFO("module", "ProgressionScaling sample {}: {} -> {}", effect, before, after);
}

namespace
{
struct SpellKind
{
    bool directDamage = false;
    bool directHeal = false;
    bool periodicDamage = false;
    bool periodicHeal = false;
    bool special = false;
};

SpellKind ClassifySpell(SpellInfo const& spell)
{
    SpellKind kind;
    kind.special = spell.HasAttribute(SPELL_ATTR3_IGNORE_CASTER_MODIFIERS);
    for (auto const& effect : spell.Effects)
    {
        switch (effect.Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                kind.directDamage = true;
                break;
            case SPELL_EFFECT_HEAL:
            case SPELL_EFFECT_HEAL_MECHANICAL:
                kind.directHeal = true;
                break;
            case SPELL_EFFECT_HEAL_MAX_HEALTH:
            case SPELL_EFFECT_HEAL_PCT:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_INSTAKILL:
                kind.special = true;
                break;
            default:
                break;
        }

        kind.periodicDamage |= effect.IsAura(SPELL_AURA_PERIODIC_DAMAGE);
        kind.periodicHeal |= effect.IsAura(SPELL_AURA_PERIODIC_HEAL);
        kind.special |= effect.IsAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT) ||
            effect.IsAura(SPELL_AURA_PERIODIC_LEECH) || effect.IsAura(SPELL_AURA_PERIODIC_HEALTH_FUNNEL) ||
            effect.IsAura(SPELL_AURA_OBS_MOD_HEALTH) || effect.IsAura(SPELL_AURA_POWER_BURN);
    }

    // These amounts can be derived from a hit that was already scaled.
    switch (spell.Id)
    {
        case 12654: // Ignite
        case 50536: // Unholy Blight
        case 12721: // Deep Wounds
        case 54172: // Divine Storm heal
        case 34299: // Leader of the Pack heal
        case 30294: // Soul Leech heal
        case 50475: // Blood Presence heal
            kind.special = true;
            break;
        default:
            break;
    }
    return kind;
}

unsigned Rank(Creature const& creature)
{
    unsigned rank = creature.GetCreatureTemplate()->rank;
    return rank <= CREATURE_ELITE_RARE ? rank : CREATURE_ELITE_NORMAL;
}

bool MatchesGeneratedHealth(Creature const& creature, uint32 health)
{
    static constexpr std::array rates{ RATE_CREATURE_NORMAL_HP, RATE_CREATURE_ELITE_ELITE_HP,
        RATE_CREATURE_ELITE_RAREELITE_HP, RATE_CREATURE_ELITE_WORLDBOSS_HP, RATE_CREATURE_ELITE_RARE_HP };
    CreatureTemplate const* info = creature.GetCreatureTemplate();
    if (info->rank >= rates.size())
        return false;
    auto const* stats = CreatureBaseStats::GetBaseStats(creature.GetLevel(), info->unit_class);
    if (!stats)
        return false;
    float expected = std::max(1u, stats->GenerateHealth(info)) * sWorld->getRate(rates[info->rank]);
    return expected >= 1 && expected < float(1u << 24) && static_cast<uint32>(expected) == health;
}

double CombatFactor(Config const& config, Unit const* source, Unit const* target, Category category, uint32 spell)
{
    bool heal = category >= DirectHeal;
    if (!source || !target || (!heal && source == target) || !source->IsInWorld() || !target->IsInWorld() ||
        source->FindMap() != target->FindMap() || source->IsCharmed() || target->IsCharmed())
        return 1;

    Profile profile = GetProfile(source->FindMap());
    if (profile == Unknown || !config.profiles[profile].enabled ||
        (config.safeMode && (profile == Arena || profile == Battleground)))
        return 1;

    // AutoBalance Instance Ownership Invariant:
    // AutoBalance owns all combat scaling (creatures, players, bots, pets) inside instances.
    if (config.autoBalance && (profile == Dungeon || profile == Raid || (source->FindMap() && source->FindMap()->IsDungeon())))
        return 1;

    Player const* owner = GetPlayerOwner(*source);
    Player const* receiver = GetPlayerOwner(*target);
    bool pvpAllowed = !config.safeMode && (profile == Arena || profile == Battleground ||
        (profile == World && config.allowOutdoorPvP));
    if ((!receiver && target->IsControlledByPlayer()) || (owner && owner->IsGameMaster()) ||
        (receiver && receiver->IsGameMaster()) || (!heal && owner && owner == receiver) ||
        (!pvpAllowed && ((owner && receiver && !heal) ||
            (owner && owner->GetCombatManager().HasPvPCombat()) ||
            (receiver && receiver->GetCombatManager().HasPvPCombat()))))
        return 1;

    Effect effect;
    unsigned level = source->GetLevel();
    Creature const* creature = source->ToCreature();
    double rank = 1;
    if (owner)
    {
        level = owner->GetLevel();
        if (creature && (creature->IsTrigger() || creature->IsVehicle()))
            return 1;
        if (creature && !source->IsPet() && !source->IsHunterPet() && !source->IsGuardian() && !source->IsTotem() && !source->IsSummon())
            return 1;
        if (creature && heal)
            return 1;
        effect = heal ? PlayerHealing : creature ? PetDamage : PlayerDamage;
    }
    else
    {
        effect = heal ? CreatureHealing : CreatureDamage;
        if (!creature || !EligibleCreature(config, *creature, effect) ||
            (heal && (!config.creatureHealing || profile != World)) ||
            (!heal && (!receiver || creature->IsFriendlyTo(target))))
            return 1;
        if (!heal)
            rank = config.rankDamage[Rank(*creature)];
    }

    if (!config.Participates(profile, effect) || !config.categoryEnabled[effect][category])
        return 1;
    if (Creature const* victim = target->ToCreature();
        victim && config.excludedEntries[effect].contains(victim->GetEntry()))
        return 1;
    if (creature && config.excludedEntries[effect].contains(creature->GetEntry()))
        return 1;

    double spellFactor = 1;
    if (spell)
    {
        auto const& rules = config.spellFactors[category];
        if (auto match = rules.find(spell); match != rules.end())
        {
            if (!match->second)
                return 1;
            spellFactor = match->second;
        }
    }

    Location location{ source->GetMapId() };
    if (!config.locations[effect].empty())
        location = GetLocation(*source);
    double factor = config.CurveFactor(effect, level) * config.LocationFactor(profile, effect, location) *
        rank * config.categories[effect][category] * spellFactor;
    if (owner && config.classEnabled && owner->getClass() < config.classes.size())
        factor *= config.classes[owner->getClass()][effect];
    if (profile == World && owner && !owner->GetGroup() && config.soloEnabled)
        factor *= heal ? config.soloHealing : config.soloDamage;
    if (profile == World && !heal && config.deltaEnabled && ((owner && !receiver) || (!owner && receiver)))
        factor *= config.Delta(level, receiver ? receiver->GetLevel() : target->GetLevel());
    return std::clamp(factor, MinFactor, MaxFactor);
}

void ApplyCombat(Config const& config, Unit const* source, Unit const* target, Category category,
    uint32& amount, uint32 spell = 0)
{
    double factor = CombatFactor(config, source, target, category, spell);
    uint32 original = amount;
    // Leave headroom for signed conversions, native crits and mitigation after this hook.
    amount = Scale(amount, factor, std::max(amount, uint32(std::numeric_limits<int32>::max() / 8)));
    if (source)
        DebugEvent(config, source->IsPet() || source->IsGuardian() || source->IsTotem() ? 2 : 0,
            CategoryNames[category], original, amount);
}

class CombatScript final : public UnitScript
{
public:
    CombatScript() : UnitScript("ProgressionScalingCombat", true, { UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_HEAL_RECEIVED }) { }

    void ModifyMeleeDamage(Unit* target, Unit* source, uint32& amount) override
    {
        auto config = GetConfig();
        if (config->enabled && amount)
            ApplyCombat(*config, source, target, Swing, amount);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* source, int32& amount, SpellInfo const* spell) override
    {
        auto config = GetConfig();
        if (!config->enabled || !spell || amount <= 0 || source == target)
            return;
        auto kind = ClassifySpell(*spell);
        if (!kind.directDamage || kind.special)
            return;
        uint32 damage = static_cast<uint32>(amount);
        ApplyCombat(*config, source, target, Spell, damage, spell->Id);
        amount = static_cast<int32>(damage);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* source, uint32& amount, SpellInfo const* spell) override
    {
        auto config = GetConfig();
        if (!config->enabled || !amount || !spell)
            return;
        auto kind = ClassifySpell(*spell);
        // AzerothCore also invokes this callback from HandlePeriodicHealAurasTick.
        if (!kind.special && kind.periodicDamage != kind.periodicHeal)
            ApplyCombat(*config, source, target, kind.periodicHeal ? PeriodicHeal : Periodic, amount, spell->Id);
    }

    void ModifyHealReceived(Unit* first, Unit* second, uint32& amount, SpellInfo const* spell) override
    {
        auto config = GetConfig();
        if (!config->enabled || !amount || !spell)
            return;
        auto kind = ClassifySpell(*spell);
        // HealBySpell passes caster first; the aura path passes target first. There is no effect index here.
        // A spell with both direct and periodic heals cannot be safely attributed in this callback.
        if (kind.directHeal && !kind.periodicHeal && !kind.periodicDamage && !kind.directDamage && !kind.special)
            ApplyCombat(*config, first, second, DirectHeal, amount, spell->Id);
    }
};

class CreatureHealthScript final : public AllCreatureScript
{
public:
    CreatureHealthScript() : AllCreatureScript("ProgressionScalingCreature") { }

    void OnCreatureSelectLevel(CreatureTemplate const*, Creature* creature) override
    {
        auto config = GetConfig();
        if (!config->enabled || creature->IsInCombat() || (creature->IsInWorld() && creature->IsAlive()))
            return;

        // AutoBalance Instance Ownership Invariant:
        // When AutoBalance compatibility is active, AutoBalance owns creature attributes in instances.
        if (config->autoBalance && creature->FindMap() && creature->FindMap()->IsDungeon())
            return;

        Profile profile = GetProfile(creature->FindMap());
        if (!config->Participates(profile, CreatureHealth) || !EligibleCreature(*config, *creature, CreatureHealth))
            return;

        // Verify the generated amount as well as the three fields, in case another hook already changed them.
        uint32 original = creature->GetCreateHealth();
        if (!original || original != creature->GetMaxHealth() ||
            static_cast<float>(original) != creature->GetFlatModifierValue(UNIT_MOD_HEALTH, BASE_VALUE) ||
            !MatchesGeneratedHealth(*creature, original))
            return;

        Location location{ creature->GetMapId() };
        if (!config->locations[CreatureHealth].empty())
            location = GetLocation(*creature);
        double factor = config->CurveFactor(CreatureHealth, creature->GetLevel()) *
            config->LocationFactor(profile, CreatureHealth, location) * config->rankHealth[Rank(*creature)];
        uint32 scaled = Scale(original, std::clamp(factor, MinFactor, MaxFactor), 1u << 24);
        if (scaled == original)
            return;

        uint32 current = creature->GetHealth();
        creature->SetCreateHealth(scaled);
        creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, static_cast<float>(scaled));
        creature->SetMaxHealth(scaled);
        double ratio = double(scaled) / original;
        if (creature->IsAlive() && current)
            creature->SetHealth(Scale(current, ratio, creature->GetMaxHealth()));
        creature->ResetPlayerDamageReq();
        DebugEvent(*config, 1, "CreatureHealth", original, creature->GetMaxHealth());
    }
};

bool WorldReward(Config const& config, Player const& player)
{
    return config.enabled && config.profiles[World].enabled && !player.IsGameMaster() &&
        !player.GetCombatManager().HasPvPCombat() && GetProfile(player.FindMap()) == World;
}

bool IsProfession(uint32 id)
{
    SkillLineEntry const* skill = sSkillLineStore.LookupEntry(id);
    return skill && (skill->categoryId == SKILL_CATEGORY_PROFESSION || skill->categoryId == SKILL_CATEGORY_SECONDARY);
}

class RatesScript final : public PlayerScript
{
public:
    RatesScript() : PlayerScript("ProgressionScalingRates", { PLAYERHOOK_ON_GIVE_EXP,
        PLAYERHOOK_ON_GIVE_REPUTATION, PLAYERHOOK_ON_UPDATE_GATHERING_SKILL, PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL }) { }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit*, uint8 source) override
    {
        auto config = GetConfig();
        if (!config->ratesEnabled || !amount || !WorldReward(*config, *player))
            return;

        switch (source)
        {
            case XPSOURCE_KILL:
                amount = Scale(amount, config->RateFactor(KillXP, player->GetLevel()), 10000000);
                break;
            case XPSOURCE_QUEST:
                amount = Scale(amount, config->RateFactor(QuestXP, player->GetLevel()), 10000000);
                break;
            case XPSOURCE_EXPLORE:
                amount = Scale(amount, config->RateFactor(ExplorationXP, player->GetLevel()), 10000000);
                break;
            default:
                break;
        }
    }

    void OnPlayerGiveReputation(Player* player, int32 faction, float& amount, ReputationSource source) override
    {
        auto config = GetConfig();
        if (!config->reputationRates || faction <= 0 || !std::isfinite(amount) || amount <= 0 ||
            !config->reputationFactions.contains(static_cast<uint32>(faction)) || !WorldReward(*config, *player))
            return;

        double factor = 1;
        switch (source)
        {
            case REPUTATION_SOURCE_KILL:
                factor = config->reputationKill;
                break;
            case REPUTATION_SOURCE_QUEST:
            case REPUTATION_SOURCE_DAILY_QUEST:
            case REPUTATION_SOURCE_WEEKLY_QUEST:
            case REPUTATION_SOURCE_MONTHLY_QUEST:
            case REPUTATION_SOURCE_REPEATABLE_QUEST:
                factor = config->reputationQuest;
                break;
            default:
                return;
        }
        if (factor != 1)
            amount = static_cast<float>(std::min(double(amount) * factor, std::max(double(amount), 1000000.0)));
    }

    void OnPlayerUpdateGatheringSkill(Player* player, uint32 skill, uint32, uint32, uint32, uint32,
        uint32& gain) override
    {
        auto config = GetConfig();
        if (config->professionRates && config->gatheringStep != 1 &&
            WorldReward(*config, *player) && IsProfession(skill))
            gain = Scale(gain, config->gatheringStep, std::max(gain, 10u));
    }

    void OnPlayerUpdateCraftingSkill(Player* player, SkillLineAbilityEntry const* spell, uint32, uint32& gain) override
    {
        auto config = GetConfig();
        if (config->professionRates && config->craftingStep != 1 && spell &&
            WorldReward(*config, *player) && IsProfession(spell->SkillLine))
            gain = Scale(gain, config->craftingStep, std::max(gain, 10u));
    }
};
}

void RegisterGameplay()
{
    new CombatScript();
    new CreatureHealthScript();
    new RatesScript();
}
}
