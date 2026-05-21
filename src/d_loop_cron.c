/* Single-player main-loop driver — replaces Crispy's d_loop.c.
 *
 * d_loop.c is heavily coupled to the networking stack (net_client/server/io).
 * We exclude all net_*.c, so this port-local version implements only the
 * single-player path: net_client_connected is always false, drone is false,
 * no packets. The tic-running logic (BuildNewTic / NetUpdate / TryRunTics)
 * is the vanilla code with the netgame branches removed. d_net.c is kept and
 * calls into the functions defined here (D_RegisterLoopCallbacks,
 * D_StartNetGame, D_InitNetGame, NetUpdate, D_ReceiveTic, ...).
 */
#include <stdlib.h>
#include <string.h>

#include "d_event.h"
#include "d_loop.h"
#include "d_ticcmd.h"

#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

#include "m_argv.h"
#include "m_fixed.h"

#include "net_defs.h"

#include "crispy.h"

typedef struct
{
    ticcmd_t cmds[NET_MAXPLAYERS];
    boolean ingame[NET_MAXPLAYERS];
} ticcmd_set_t;

static ticcmd_set_t ticdata[BACKUPTICS];

static int maketic;
static int recvtic;

int gametic;
int oldleveltime;
boolean singletics = false;

static int localplayer;
static int skiptics = 0;
int ticdup;
fixed_t offsetms;
static boolean new_sync = true;

static loop_interface_t *loop_interface = NULL;
static boolean local_playeringame[NET_MAXPLAYERS];
static int player_class;

/* Always single-player on this console. */
static const boolean net_client_connected = false;

/* Normally defined in net_client.c (excluded). Never a drone here. */
boolean drone = false;

static int GetAdjustedTime(void)
{
    int time_ms = I_GetTimeMS();
    if (new_sync)
        time_ms += (offsetms / FRACUNIT);
    return (time_ms * TICRATE) / 1000;
}

static boolean BuildNewTic(void)
{
    int gameticdiv;
    ticcmd_t cmd;

    gameticdiv = gametic / ticdup;

    I_StartTic();
    loop_interface->ProcessEvents();
    loop_interface->RunMenu();

    if (new_sync)
    {
        if (maketic - gameticdiv > 2)
            return false;
        if (maketic - gameticdiv > 8)
            return false;
    }
    else
    {
        if (maketic - gameticdiv >= 5)
            return false;
    }

    memset(&cmd, 0, sizeof(ticcmd_t));
    loop_interface->BuildTiccmd(&cmd, maketic);

    ticdata[maketic % BACKUPTICS].cmds[localplayer] = cmd;
    ticdata[maketic % BACKUPTICS].ingame[localplayer] = true;

    ++maketic;
    return true;
}

int lasttime;

void NetUpdate(void)
{
    int nowtime;
    int newtics;
    int i;

    if (singletics)
        return;

    nowtime = GetAdjustedTime() / ticdup;
    newtics = nowtime - lasttime;
    lasttime = nowtime;

    if (skiptics <= newtics)
    {
        newtics -= skiptics;
        skiptics = 0;
    }
    else
    {
        skiptics -= newtics;
        newtics = 0;
    }

    for (i = 0; i < newtics; i++)
    {
        if (!BuildNewTic())
            break;
    }
}

void D_ReceiveTic(ticcmd_t *ticcmds, boolean *players_mask)
{
    int i;

    if (ticcmds == NULL && players_mask == NULL)
        return; /* disconnected — impossible in single-player */

    for (i = 0; i < NET_MAXPLAYERS; ++i)
    {
        if (i == localplayer)
        {
            /* this is us; don't overwrite */
        }
        else
        {
            ticdata[recvtic % BACKUPTICS].cmds[i] = ticcmds[i];
            ticdata[recvtic % BACKUPTICS].ingame[i] = players_mask[i];
        }
    }
    ++recvtic;
}

void D_StartGameLoop(void)
{
    lasttime = GetAdjustedTime() / ticdup;
}

void D_StartNetGame(net_gamesettings_t *settings,
                    netgame_startup_callback_t callback)
{
    int i;
    (void)callback;

    offsetms = 0;
    recvtic = 0;

    settings->consoleplayer = 0;
    settings->num_players = 1;
    settings->player_classes[0] = player_class;
    settings->new_sync = !M_ParmExists("-oldsync");
    settings->extratics = 1;
    settings->ticdup = 1;

    localplayer = settings->consoleplayer;

    for (i = 0; i < NET_MAXPLAYERS; ++i)
        local_playeringame[i] = i < settings->num_players;

    ticdup = settings->ticdup;
    new_sync = settings->new_sync;

    if (ticdup < 1)
        ticdup = 1;
}

boolean D_InitNetGame(net_connect_data_t *connect_data)
{
    /* Register quit handler, then report "no netgame". */
    I_AtExit(D_QuitNetGame, true);
    player_class = connect_data->player_class;
    return false;
}

void D_QuitNetGame(void)
{
    /* nothing to do without networking */
}

static int GetLowTic(void)
{
    return maketic;
}

static boolean PlayersInGame(void)
{
    /* single-player, not a drone: always in the game */
    return true;
}

static void TicdupSquash(ticcmd_set_t *set)
{
    ticcmd_t *cmd;
    unsigned int i;

    for (i = 0; i < NET_MAXPLAYERS; ++i)
    {
        cmd = &set->cmds[i];
        cmd->chatchar = 0;
        if (cmd->buttons & BT_SPECIAL)
            cmd->buttons = 0;
    }
}

static void SinglePlayerClear(ticcmd_set_t *set)
{
    unsigned int i;
    for (i = 0; i < NET_MAXPLAYERS; ++i)
    {
        if (i != (unsigned int)localplayer)
            set->ingame[i] = false;
    }
}

void TryRunTics(void)
{
    int i;
    int lowtic;
    int entertic;
    static int oldentertics;
    int realtics;
    int availabletics;
    int counts;

    extern int leveltime;
    #define return_early (crispy->uncapped && counts == 0 && leveltime > oldleveltime && screenvisible)

    entertic = I_GetTime() / ticdup;
    realtics = entertic - oldentertics;
    oldentertics = entertic;

    if (singletics)
        BuildNewTic();
    else
        NetUpdate();

    lowtic = GetLowTic();
    availabletics = lowtic - gametic / ticdup;

    if (new_sync)
    {
        if (crispy->uncapped)
        {
            if (realtics < availabletics - 1)
                counts = realtics + 1;
            else if (realtics < availabletics)
                counts = realtics;
            else
                counts = availabletics;
        }
        else
        {
            counts = availabletics;
        }

        if (return_early)
            return;
    }
    else
    {
        if (realtics < availabletics - 1)
            counts = realtics + 1;
        else if (realtics < availabletics)
            counts = realtics;
        else
            counts = availabletics;

        if (return_early)
            return;

        if (counts < 1)
            counts = 1;
    }

    if (counts < 1)
        counts = 1;

    /* wait for new tics if needed (single-player: just build them) */
    while (lowtic < gametic / ticdup + counts)
    {
        NetUpdate();
        lowtic = GetLowTic();

        if (lowtic < gametic / ticdup)
            I_Error("TryRunTics: lowtic < gametic");

        if (lowtic < gametic / ticdup + counts)
        {
            /* No blocking on this console: bail out and render. */
            return;
        }
    }

    while (counts--)
    {
        ticcmd_set_t *set;

        if (!PlayersInGame())
            return;

        set = &ticdata[(gametic / ticdup) % BACKUPTICS];

        SinglePlayerClear(set);

        for (i = 0; i < ticdup; i++)
        {
            if (gametic / ticdup > lowtic)
                I_Error("gametic>lowtic");

            memcpy(local_playeringame, set->ingame, sizeof(local_playeringame));

            loop_interface->RunTic(set->cmds, set->ingame);
            gametic++;

            TicdupSquash(set);
        }

        NetUpdate();
    }
}

void D_RegisterLoopCallbacks(loop_interface_t *i)
{
    loop_interface = i;
}

/* --- non-vanilla demo gating (verbatim from d_loop.c) ------------------- */
#include "m_misc.h"
#include "w_wad.h"

static boolean StrictDemos(void)
{
    return M_ParmExists("-strictdemos");
}

boolean D_NonVanillaRecord(boolean conditional, const char *feature)
{
    if (!conditional || StrictDemos())
        return false;
    printf("Warning: Recording a demo file with a non-vanilla extension "
           "(%s).\n", feature);
    return true;
}

static boolean IsDemoFile(int lumpnum)
{
    char *lower;
    boolean result;

    lower = M_StringDuplicate(lumpinfo[lumpnum]->wad_file->path);
    M_ForceLowercase(lower);
    result = M_StringEndsWith(lower, ".lmp");
    free(lower);
    return result;
}

boolean D_NonVanillaPlayback(boolean conditional, int lumpnum,
                             const char *feature)
{
    if (!conditional || StrictDemos())
        return false;
    if (!IsDemoFile(lumpnum))
        return false;
    printf("Warning: Playing back a demo file with a non-vanilla extension "
           "(%s).\n", feature);
    return true;
}
