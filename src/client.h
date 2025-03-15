#pragma once

#include "common.h"

extern void manage(Window w, XWindowAttributes *wa);
extern void unmanage(Client *c, int destroyed);

extern void attach(Client *c);
extern void append(Client *c);
extern void detach(Client *c);
extern void attachstack(Client *c);
extern void detachstack(Client *c);
extern Client *nexttiled(Client *c);

extern void setfullscreen(Client *c, int fullscreen);
extern void setfloating(Client *c, int floating, int force, int should_arrange);
extern void seturgent(Client *c, int urg);
extern void setclientstate(Client *c, long state);

extern void updatetitle(Client *c);
extern int updatewindowtype(Client *c);
extern void updatesizehints(Client *c);
extern void updatewmhints(Client *c);
extern void resize(Client *c, int x, int y, int w, int h, int interact);
extern void resizeclient(Client *c, int x, int y, int w, int h);
extern int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
extern int sendevent(Client *c, Atom proto);
extern void configure(Client *c);
