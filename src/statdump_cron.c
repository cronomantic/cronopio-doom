// [cronopio] Stub for doom/statdump.c (the -statdump end-of-level statistics
// dumper). statdump.c is excluded; the feature writes stats to a file at exit,
// which Cronopio has no use for. StatCopy is called after each level and
// StatDump is registered as an I_AtExit handler; both are no-ops here.

#include "doomtype.h"
#include "d_player.h"

void StatCopy(const wbstartstruct_t *stats)
{
    (void)stats;
}

void StatDump(void)
{
}
