#include "ProgressionScaling.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "Group.h"
#include "Item.h"
#include "Map.h"
#include "MapMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"

namespace ProgressionScaling
{
namespace
{
using namespace Acore::ChatCommands;

bool ShowStatus(ChatHandler* handler)
{
    auto config = GetConfig();
    handler->PSendSysMessage("ProgressionScaling enabled={} safeMode={} levels={}-{}", config->enabled,
        config->safeMode, config->minLevel, config->maxLevel);
    handler->PSendSysMessage("Creature HP world={} dungeon={} raid={}", config->Participates(World, CreatureHealth),
        config->Participates(Dungeon, CreatureHealth), config->Participates(Raid, CreatureHealth));
    handler->SendSysMessage("Playerbots use native player/owner rules. "
        "Bot detection and compensation are unavailable.");
    return true;
}

bool ShowCurve(ChatHandler* handler, std::string name, uint32 level)
{
    auto it = std::find(EffectNames.begin(), EffectNames.end(), name);
    if (it == EffectNames.end() || level < 1 || level > MaxLevel)
    {
        handler->SendSysMessage("Usage .progression curve <effect> <level 1-80>. Use the configuration's effect name.");
        handler->SetSentErrorMessage(true);
        return false;
    }
    auto config = GetConfig();
    handler->PSendSysMessage("{} level {} curve={:.4f}", name, level,
        config->CurveFactor(static_cast<Effect>(it - EffectNames.begin()), level));
    return true;
}

void Preview(ChatHandler* handler, Config const& config, Unit const& unit, Effect effect)
{
    Profile profile = GetProfile(unit.FindMap());
    Player const* owner = GetPlayerOwner(unit);
    Creature const* creature = unit.ToCreature();
    unsigned level = owner ? owner->GetLevel() : unit.GetLevel();
    double curve = config.CurveFactor(effect, level);
    double map = profile == Unknown ? 1 : config.LocationFactor(profile, effect, GetLocation(unit));
    double rank = 1;
    if (creature && creature->GetCreatureTemplate() && (effect == CreatureHealth || effect == CreatureDamage))
    {
        auto index = creature->GetCreatureTemplate()->rank;
        if (index < config.rankDamage.size())
            rank = effect == CreatureHealth ? config.rankHealth[index] : config.rankDamage[index];
    }
    double playerClass = owner && config.classEnabled && owner->getClass() < config.classes.size() ?
        config.classes[owner->getClass()][effect] : 1;
    char const* reason = "eligible preview";
    if (!config.Participates(profile, effect))
        reason = "profile or safety policy bypass";
    else if (owner && owner->IsGameMaster())
        reason = "GM mode bypass";
    else if (creature && (effect == CreatureDamage || effect == CreatureHealth) &&
        !EligibleCreature(config, *creature, effect))
        reason = "creature eligibility bypass";
    double projected = std::string_view(reason) == "eligible preview" ?
        std::clamp(curve * map * rank * playerClass, MinFactor, MaxFactor) : 1;
    handler->PSendSysMessage("{} level={} profile={} curve={:.4f} location={:.4f} rank={:.4f} "
        "class={:.4f} preview={:.4f} ({})",
        EffectNames[effect], level, ProfileNames[profile], curve, map, rank, playerClass, projected, reason);
}

bool ShowPlayer(ChatHandler* handler)
{
    Player* player = handler->getSelectedPlayerOrSelf();
    if (!player)
        return false;
    auto config = GetConfig();
    Preview(handler, *config, *player, PlayerDamage);
    Preview(handler, *config, *player, PlayerHealing);
    return true;
}

bool ShowCreature(ChatHandler* handler)
{
    Creature* creature = handler->getSelectedCreature();
    if (!creature)
        return false;
    auto config = GetConfig();
    Player const* owner = GetPlayerOwner(*creature);
    handler->PSendSysMessage("{} owner={} current HP={} max HP={} generated field={}", creature->GetName(),
        owner ? owner->GetName() : "none", creature->GetHealth(), creature->GetMaxHealth(),
        creature->GetCreateHealth());
    if (owner)
        Preview(handler, *config, *creature, PetDamage);
    else
    {
        Preview(handler, *config, *creature, CreatureDamage);
        Preview(handler, *config, *creature, CreatureHealth);
    }
    return true;
}

bool ShowMap(ChatHandler* handler)
{
    Player* player = handler->getSelectedPlayerOrSelf();
    if (!player || !player->FindMap())
        return false;
    auto location = GetLocation(*player);
    handler->PSendSysMessage("Map {} zone {} area {} difficulty {} profile {}", location.map, location.zone,
        location.area, location.difficulty, ProfileNames[GetProfile(player->FindMap())]);
    return true;
}

bool ShowExplain(ChatHandler* handler)
{
    Unit* target = handler->getSelectedUnit();
    return target && (target->ToPlayer() ? ShowPlayer(handler) : ShowCreature(handler));
}

bool ShowBudget(ChatHandler* handler, uint32 level)
{
    if (level < 1 || level > MaxLevel)
        return false;
    auto config = GetConfig();
    double hp = config->Participates(World, CreatureHealth) ? config->CurveFactor(CreatureHealth, level) : 1;
    double damage = config->Participates(World, PlayerDamage) ? config->CurveFactor(PlayerDamage, level) : 1;
    double incoming = config->Participates(World, CreatureDamage) ? config->CurveFactor(CreatureDamage, level) : 1;
    handler->PSendSysMessage("Level {} world curve estimate: HP={:.3f}, damage={:.3f}, "
        "TTK ratio={:.3f}, incoming per fight ratio={:.3f}",
        level, hp, damage, hp / damage, incoming * hp / damage);
    return true;
}

struct GearSample
{
    uint64 sum = 0;
    uint32 count = 0;

    double Average() const { return count ? double(sum) / count : 0; }
};

GearSample EquippedGear(Player const& player)
{
    GearSample result;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_BODY || slot == EQUIPMENT_SLOT_TABARD)
            continue;
        if (Item const* item = player.GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            uint32 level = item->GetTemplate()->ItemLevel;
            if (!level)
                continue;
            ++result.count;
            result.sum += level;
        }
    }
    return result;
}

bool ShowGear(ChatHandler* handler)
{
    Player* player = handler->getSelectedPlayerOrSelf();
    if (!player)
        return false;
    if (!GetConfig()->gearDiagnostics)
    {
        handler->SendSysMessage("Gear diagnostics disabled.");
        return true;
    }

    auto sample = EquippedGear(*player);
    if (sample.count)
        handler->PSendSysMessage("Equipped template item level mean={:.2f}, occupied slots={}",
            sample.Average(), sample.count);
    else
        handler->SendSysMessage("No equipped items with a positive template item level.");

    if (Group const* group = player->GetGroup())
    {
        double total = 0;
        unsigned samples = 0;
        for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player const* member = ref->GetSource();
            if (!member || !member->IsInWorld() || member->FindMap() != player->FindMap())
                continue;
            auto gear = EquippedGear(*member);
            if (gear.count)
            {
                total += gear.Average();
                ++samples;
            }
        }
        if (samples)
            handler->PSendSysMessage("Same-map group mean={:.2f}, equipped samples={}/{}. Bots included.",
                total / samples, samples, group->GetMembersCount());
        else
            handler->SendSysMessage("No equipped same-map group samples.");
    }
    return true;
}

bool Reload(ChatHandler* handler)
{
    sWorld->LoadConfigSettings(true);
    sMapMgr->InitializeVisibilityDistanceInfo();
    handler->SendSysMessage("World config reload requested. Check the server log for the module reload result.");
    return true;
}

class ProgressionCommands final : public CommandScript
{
public:
    ProgressionCommands() : CommandScript("ProgressionScalingCommands") { }

    ChatCommandTable GetCommands() const override
    {
        constexpr auto diagnostics = rbac::RBAC_PERM_COMMAND_NPC_INFO;
        static ChatCommandTable children{
            { "status", ShowStatus, diagnostics, Console::Yes },
            { "curve", ShowCurve, diagnostics, Console::Yes },
            { "budget", ShowBudget, diagnostics, Console::Yes },
            { "player", ShowPlayer, diagnostics, Console::No },
            { "creature", ShowCreature, diagnostics, Console::No },
            { "map", ShowMap, diagnostics, Console::No },
            { "explain", ShowExplain, diagnostics, Console::No },
            { "gear", ShowGear, diagnostics, Console::No },
            { "reload", Reload, rbac::RBAC_PERM_COMMAND_RELOAD_CONFIG, Console::Yes }
        };
        static ChatCommandTable commands{ { "progression", children } };
        return commands;
    }
};
}

void RegisterCommands()
{
    new ProgressionCommands();
}
}
