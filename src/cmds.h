#pragma once

#include "common.h"

/* commandable functions */
extern void quit(DevPair *dp, const Arg *arg);
extern void reloadconfig(DevPair *dp, const Arg *arg);
extern void focusmon(DevPair *dp, const Arg *arg);
extern void swapmon(DevPair *dp, const Arg *arg);
extern void focusstack(DevPair *dp, const Arg *arg);
extern void cyclestack(DevPair *dp, const Arg *arg);
extern void incnmaster(DevPair *dp, const Arg *arg);
extern void killclient(DevPair *dp, const Arg *arg);
extern void movemouse(DevPair *dp, const Arg *arg);
extern void resizemouse(DevPair *dp, const Arg *arg);
extern void setlayout(DevPair *dp, const Arg *arg);
extern void setmfact(DevPair *dp, const Arg *arg);
extern void spawn(DevPair *dp, const Arg *arg);
extern void tag(DevPair *dp, const Arg *arg);
extern void tagmon(DevPair *dp, const Arg *arg);
extern void togglebar(DevPair *dp, const Arg *arg);
extern void togglefloating(DevPair *dp, const Arg *arg);
extern void togglefullscreen(DevPair *dp, const Arg *arg);
extern void toggleautoswapmon(DevPair *dp, const Arg *arg);
extern void togglermaster(DevPair *dp, const Arg *arg);
extern void togglemouse(DevPair *dp, const Arg *arg);
extern void toggletag(DevPair *dp, const Arg *arg);
extern void toggleview(DevPair *dp, const Arg *arg);
extern void view(DevPair *dp, const Arg *arg);
extern void zoom(DevPair *dp, const Arg *arg);
