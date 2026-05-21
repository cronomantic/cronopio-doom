/* Cronopio timer backend — replaces Crispy's i_timer.c.
 *
 * 35Hz DOOM tics derived from cron_time_ms(). All math is 32-bit (the
 * translator rejects i64); ms*35 overflows int32 after ~61 minutes, which is
 * acceptable for a game session.
 */
#include <cronopio.h>

#include "doomtype.h"
#include "m_fixed.h"
#include "i_timer.h"

static uint32_t basetime = 0;

void I_InitTimer(void)
{
    basetime = cron_time_ms();
}

int I_GetTimeMS(void)
{
    return (int)(cron_time_ms() - basetime);
}

int I_GetTime(void)
{
    uint32_t ms = cron_time_ms() - basetime;
    return (int)((ms * TICRATE) / 1000u);
}

/* I_GetTimeUS is declared uint64_t by Crispy but only referenced from the
 * excluded i_video.c, so we deliberately do NOT define it here (an i64 return
 * type is rejected by the translator). If a future kept caller needs it, return
 * a 32-bit microsecond value via cvm_int64.h instead. */

void I_Sleep(int ms) { (void)ms; }    /* console can't block; no-op */

void I_WaitVBL(int count) { (void)count; }

fixed_t I_GetFracRealTime(void) { return 0; }
