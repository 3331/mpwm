#pragma once

#include "common.h"

extern Monitor *dirtomon(DevPair *dp, int dir);
extern Monitor *anywintomon(Window w);
extern Monitor *wintomon(DevPair *dp, Window w);
extern Monitor *recttomon(DevPair *dp, int x, int y, int w, int h);
extern Client *wintoclient(Window w);