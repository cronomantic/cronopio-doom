/* Automap stub — replaces doom/am_map.c.
 *
 * am_map.c uses int64_t pervasively (the automap coordinate system widens to
 * 64-bit to avoid overflow on long map lines). The CronoVM translator rejects
 * i64, and the automap is not needed to boot/render the game, so we stub the
 * small external AM_* surface that the rest of the engine calls and exclude
 * am_map.c from the build. Opening the automap is a no-op for now.
 *
 * TODO(port): reinstate a real automap by rewriting am_map.c's int64 math with
 * cvm_int64.h struct helpers once gameplay is up.
 */
#include "doomtype.h"
#include "d_event.h"
#include "m_cheat.h"

boolean automapactive = false;

cheatseq_t cheat_amap = CHEAT("iddt", 0);

boolean AM_Responder(event_t *ev) { (void)ev; return false; }
void    AM_Ticker(void) { }
void    AM_Drawer(void) { }
void    AM_Stop(void) { }
void    AM_initVariables(void) { }
void    AM_LevelInit(boolean reinit) { (void)reinit; }
void    AM_ResetIDDTcheat(void) { }

/* Savegame mark-point extension (p_extsaveg.c). No marks without an automap. */
void AM_GetMarkPoints(int *n, long *p) { (void)p; if (n) *n = 0; }
void AM_SetMarkPoints(int n, long *p)  { (void)n; (void)p; }
