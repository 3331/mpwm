#pragma once

#include "common.h"

extern Monitor *createmon(void);
extern void arrange(Monitor *m);
extern void arrangemon(Monitor *m);
extern void insertmon(Monitor *at, Monitor *m);
extern void unlinkmon(Monitor *m);
extern void cleanupmon(Monitor *m);