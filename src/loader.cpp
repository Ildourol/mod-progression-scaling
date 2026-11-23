#include "ProgressionScaling.h"

void Addmod_progression_scalingScripts()
{
    ProgressionScaling::RegisterConfig();
    ProgressionScaling::RegisterGameplay();
    ProgressionScaling::RegisterCommands();
}

// Legacy script loader alias for older CMake and external runner compatibility
void AddSC_mod_progression_scaling()
{
    Addmod_progression_scalingScripts();
}
