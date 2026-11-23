#pragma once

#include "ProgressionScalingPolicy.h"
#include <memory>

class Creature;
class Map;
class Player;
class Unit;

namespace ProgressionScaling
{
std::shared_ptr<Config const> GetConfig();
void RegisterConfig();
void RegisterGameplay();
void RegisterCommands();

Profile GetProfile(Map const* map);
Location GetLocation(Unit const& actor);
Player const* GetPlayerOwner(Unit const& actor);
bool EligibleCreature(Config const& config, Creature const& creature, Effect effect);
void DebugEvent(Config const& config, unsigned channel, char const* effect, uint32_t before, uint32_t after);
}
