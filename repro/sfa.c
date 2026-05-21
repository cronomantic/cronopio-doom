#include <cronopio.h>
typedef struct { const char *name; int v; } ent_t;
extern ent_t tbl[];
static void setup(void){ cron_log(tbl[1].name, 3); cron_trace_i32(1, tbl[1].v); }
static void frame(void){}
CRONOPIO_CART_INIT(setup, frame)
