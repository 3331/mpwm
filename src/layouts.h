#pragma once

#include "common.h"

/* layouts */
extern void tile(Monitor *m);
extern void monocle(Monitor *m);
extern void centeredmaster(Monitor *m);

extern const Layout glayouts[];
extern const unsigned int glayouts_len;