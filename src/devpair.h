#pragma once

#include "common.h"

extern void initdevices(void);
extern DevPair *createdevpair(void);
extern DevPair *getdevpair(int deviceid);
extern void removedevpair(DevPair *dp);
extern void updatedevpair(DevPair *dp);
extern int getrootptr(DevPair *dp, int *x, int *y);

extern void grabkeys(void);
extern void grabdevicekeys(Device *mkbd);
extern void grabdevicebuttons(Device *mptr);
extern void grabbuttons(Device *mptr, Client *c, int focused);

extern void setsel(DevPair *dp, Client *c);
extern void setselmon(DevPair *dp, Monitor *m);
extern void focus(DevPair *dp, Client *c);
extern void unfocus(DevPair *dp, int setfocus);
extern void setfocus(DevPair *dp, Client *c);
extern void sendmon(DevPair *dp, Client *c, Monitor *m, int refocus);


extern Device deviceslots[MAXDEVICES];