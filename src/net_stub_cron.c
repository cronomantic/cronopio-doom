// [cronopio] Networking stubs for Crispy Doom.
// All net_*.c sources are excluded (SDL_net / sockets). Cronopio is strictly
// single-player, so the small set of NET_* entry points the kept engine code
// still calls (config binding, dedicated-server / query command-line paths)
// are stubbed here. d_loop_cron.c provides the single-player game loop, so
// none of the NET_CL_*/NET_SV_* runtime paths are reachable.

#include "doomtype.h"

void NET_Init(void)
{
}

void NET_BindVariables(void)
{
}

void NET_DedicatedServer(void)
{
    // No dedicated server in single-player Cronopio.
}

void NET_LANQuery(void)
{
}

void NET_MasterQuery(void)
{
}

void NET_QueryAddress(const char *addr)
{
    (void)addr;
}
