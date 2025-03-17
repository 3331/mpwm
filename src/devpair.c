#include "devpair.h"
#include "config.h"
#include "util.h"
#include "barwin.h"
#include "monitor.h"
#include "client.h"
#include "events.h"
#include "resolvers.h"

Device deviceslots[MAXDEVICES] = {0};

void initdevices(void)
{
    unsigned int i, ndevices = 0;
    int32_t idx;
    XIDeviceInfo *devs;
    DevPair *dp;
    Device *d;
    Monitor *m;
    int x, y;

    devs = XIQueryDevice(gwm.dpy, XIAllDevices, (int*)&ndevices);
    if(!devs)
        die("fatal: can't get list of input devices\n");
    
    /* cache devices */
    for(i = 0; i < ndevices; i++) {
        idx = devs[i].deviceid;
        deviceslots[idx].info.deviceid = devs[i].deviceid;
        deviceslots[idx].info.attachment = devs[i].attachment;
        deviceslots[idx].info.use = devs[i].use;
        deviceslots[idx].info.enabled = devs[i].enabled;
    }
    
    /* create device pairs */
    for(i = 0; i < LENGTH(deviceslots); i++) {
        if (deviceslots[i].info.use != XIMasterKeyboard && deviceslots[i].info.use != XIMasterPointer)
            continue;

        if (!(dp = getdevpair(deviceslots[i].info.attachment))) {
            dp = createdevpair();
        }

        *(deviceslots[i].info.use == XIMasterPointer ? &dp->mptr : &dp->mkbd) = &deviceslots[i];
        deviceslots[i].self = dp;
    }

    /* attach slaves to device pairs */
    for(i = 0; i < LENGTH(deviceslots); i++) {
        if (deviceslots[i].info.use != XISlaveKeyboard && deviceslots[i].info.use != XISlavePointer)
            continue;
        if (!(dp = getdevpair(deviceslots[i].info.attachment)))
            die("could not find device pair for slave device\n");
        if (!dp->slaves) {
            d = &deviceslots[i];
            dp->slaves = d;
        } else {
            for (d = dp->slaves; d && d->snext; d = d->snext);
            d->snext = &deviceslots[i];
        }
        deviceslots[i].self = dp;
    }

    /* initialize device pairs */
    for (dp = gwm.devpairs; dp; dp = dp->next) {
        if(!getrootptr(dp, &x, &y))
            continue;
        if(!(m = recttomon(dp, x, y, 1, 1)))
            continue;
        setselmon(dp, m);
        updatedevpair(dp);
    }
    XIFreeDeviceInfo(devs);
}

DevPair *createdevpair(void)
{
    DevPair *last_dp;
    DevPair *dp;

    dp = ecalloc(1, sizeof(DevPair));
    for (last_dp = gwm.devpairs; last_dp && last_dp->next; last_dp = last_dp->next);
    if (last_dp)
        last_dp->next = dp;
    else
        gwm.devpairs = dp;
    return dp;
}

DevPair *getdevpair(int deviceid)
{
    return deviceid ? deviceslots[deviceid].self : NULL;
}

void removedevpair(DevPair *dp)
{
    DevPair **pdp;

    setsel(dp, NULL);
    setselmon(dp, NULL);

    /* pop devpair from devpairs */
    for (pdp = &gwm.devpairs; *pdp && *pdp != dp; pdp = &(*pdp)->next);
    *pdp = dp->next;

    free(dp);
}

void updatedevpair(DevPair *dp)
{
    Monitor *m;
    Client *c;

    grabdevicekeys(dp->mkbd);
    grabdevicebuttons(dp->mptr);

    for (m = gwm.mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            grabbuttons(dp->mptr, c, 0);
}

int getrootptr(DevPair *dp, int *__restrict x, int *__restrict y)
{
    double dx, dy;
    int ret = XIQueryPointer(gwm.dpy, dp->mptr->info.deviceid, gwm.root, &(Window){0},
        &(Window){0}, &dx, &dy, &(double){0.0}, &(double){0.0}, &(XIButtonState){0},
        &(XIModifierState){0}, &(XIGroupState){0}
    );
    *x = dx;
    *y = dy;
    return ret;
}

void updatenumlockmask(void)
{
    int i, j;
    XModifierKeymap *modmap;

    gwm.numlockmask = 0;
    modmap = XGetModifierMapping(gwm.dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++)
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
                == XKeysymToKeycode(gwm.dpy, XK_Num_Lock))
                gwm.numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

void grabdevicekeys(Device *mkbd)
{
    memset(kbdmask, 0, sizeof(kbdmask));
    XISetMask(kbdmask, XI_KeyPress);
    kbdevm.deviceid = mkbd->info.deviceid;

    updatenumlockmask();
    {
        unsigned int i;
        KeyCode code;
        XSync(gwm.dpy, False);
        XIUngrabKeycode(gwm.dpy, kbdevm.deviceid, XIAnyKeycode, gwm.root, ganymodifier_len, ganymodifier);
        XSync(gwm.dpy, False);
        for (i = 0; i < gkeys_len; i++) {
            if (!(code = XKeysymToKeycode(gwm.dpy, gkeys[i].keysym)))
                continue;
            XIGrabModifiers modifiers[] = {
                { gkeys[i].mod, 0 },
                { gkeys[i].mod|LockMask, 0 },
                { gkeys[i].mod|gwm.numlockmask, 0 },
                { gkeys[i].mod|gwm.numlockmask|LockMask, 0 }
            };
            XSync(gwm.dpy, False);
            XIGrabKeycode(gwm.dpy, kbdevm.deviceid, code, gwm.root,
                XIGrabModeAsync, XIGrabModeAsync, True, &kbdevm, LENGTH(modifiers), modifiers);
            XSync(gwm.dpy, False);
        }
    }
}

void grabdevicebuttons(Device *mptr)
{
    ptrevm.deviceid = mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XSync(gwm.dpy, False);
    XISelectEvents(gwm.dpy, gwm.root, &ptrevm, 1);
    XSync(gwm.dpy, False);
}

void grabbuttons(Device *mptr, Client *c, int focused)
{
    ptrevm.deviceid = XIAllDevices;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Enter);
    XISetMask(ptrmask, XI_FocusIn);
    XSync(gwm.dpy, False);
    XISelectEvents(gwm.dpy, c->win, &ptrevm, 1);
    XSync(gwm.dpy, False);

    ptrevm.deviceid = mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Motion);
    XISetMask(ptrmask, XI_ButtonPress);
    XISetMask(ptrmask, XI_ButtonRelease);

    updatenumlockmask();
    {
        unsigned int i;
        XSync(gwm.dpy, False);
        XIUngrabButton(gwm.dpy, ptrevm.deviceid, XIAnyButton, c->win, ganymodifier_len, ganymodifier);
        XSync(gwm.dpy, False);
        if(!c->grabbed)
            return;
        if (!focused) {
            XSync(gwm.dpy, False);
            XIGrabButton(gwm.dpy, ptrevm.deviceid, XIAnyButton, c->win, None, XIGrabModeAsync,
                XIGrabModeAsync, False, &ptrevm, ganymodifier_len, ganymodifier);
            XSync(gwm.dpy, False);
        }
        for (i = 0; i < gbuttons_len; i++) {
            if (gbuttons[i].click != ClkClientWin)
                continue;
            XIGrabModifiers modifiers[] = {
                { gbuttons[i].mask, 0 },
                { gbuttons[i].mask|LockMask, 0 },
                { gbuttons[i].mask|gwm.numlockmask, 0 },
                { gbuttons[i].mask|gwm.numlockmask|LockMask, 0 }
            };
            XSync(gwm.dpy, False);
            XIGrabButton(gwm.dpy, ptrevm.deviceid, gbuttons[i].button, c->win, None, XIGrabModeAsync,
                XIGrabModeAsync, False, &ptrevm, LENGTH(modifiers), modifiers);
            XSync(gwm.dpy, False);
        }
    }
}

void grabkeys(void)
{
    DevPair *dp;
    for (dp = gwm.devpairs; dp; dp = dp->next)
        grabdevicekeys(dp->mkbd);
}

void format_client_prefix_name(Client *c)
{
    DevPair *dp;
    int ri = 0, fi = 0;
    int i;

    if ((ri = snprintf(&c->prefix_name[fi], sizeof(c->prefix_name) - fi, "[")) < 0)
        return;
    fi += ri;
    for (dp = c->devstack, i = 0; dp; dp = dp->fnext, i++) {
        if(!i) {
            if ((ri = snprintf(&c->prefix_name[fi], sizeof(c->prefix_name) - fi, "%d", dp->mptr->info.deviceid)) < 0)
                return;
        }
        else {
            if ((ri = snprintf(&c->prefix_name[fi], sizeof(c->prefix_name) - fi, ", %d", dp->mptr->info.deviceid)) < 0)
                return;
        }

        fi += ri;
    }

    snprintf(&c->prefix_name[fi], sizeof(c->prefix_name) - fi, "]");
}

void setsel(DevPair *dp, Client *c)
{
    DevPair **tdp;
    DevPair *ndp;
    Clr **cur_scheme;

    if (dp->sel == c)
        return;

    DBG("+setsel %lu -> %lu\n", dp->sel ? dp->sel->win : 0, c ? c->win : 0);

    if(gwm.forcedfocusmon)
        cur_scheme = gwm.ff_scheme;
    else
        cur_scheme = gwm.scheme;
    
    if (dp->sel) {
        dp->sel->devices--;
        if(updatewindowtype(dp->sel) < 2)
            XSetWindowBorder(gwm.dpy, dp->sel->win, cur_scheme[CLAMP(SchemeNorm + dp->sel->devices, SchemeNorm, SchemeSel3)][ColBorder].pixel);
        for (tdp = &dp->sel->devstack; *tdp && *tdp != dp; tdp = &(*tdp)->fnext);
        *tdp = dp->fnext;
        dp->fnext = NULL;
        format_client_prefix_name(dp->sel);
    }
    
    dp->sel = c;

    if (dp->sel) {
        dp->sel->devices++;
        if(updatewindowtype(dp->sel) < 2)
            XSetWindowBorder(gwm.dpy, dp->sel->win, cur_scheme[CLAMP(SchemeNorm + dp->sel->devices, SchemeNorm, SchemeSel3)][ColBorder].pixel);
        for (ndp = dp->sel->devstack; ndp && ndp->fnext; ndp = ndp->fnext);
        if (ndp)
            ndp->fnext = dp;
        else
            dp->sel->devstack = dp;
        format_client_prefix_name(dp->sel);
    }
}

void setselmon(DevPair *dp, Monitor *m)
{
    DBG("+setselmon\n");
    DevPair **tdp;
    DevPair *ndp;
    XEvent ev;
    int cur_bar_offset;
    int tar_bar_offset;

    if(dp->selmon == m || gwm.forcing_focus)
        return;

    if(gwm.forcedfocusmon && m != gwm.forcedfocusmon)
    {
        gwm.forcing_focus = 1;
        Monitor *tar = m;
        Monitor *tar2 = NULL;
        int x, y;

        // only 1 screen, no need...
        if (!gwm.mons->next)
            return;

        Monitor *cur = dp->selmon;
        int dir = (cur->mx + (cur->mw / 2)) - (tar->mx + (tar->mw / 2));
        tar2 = dirtomon(dp, -dir);
        
        if(getrootptr(dp, &x, &y) && (recttomon(dp, x, y, 1, 1) != cur || dp->move.c))
        {
            if(cur->showbar)
                if(cur->topbar)
                    cur_bar_offset = -gwm.bh;
                else
                    cur_bar_offset = gwm.bh;
            else
                cur_bar_offset = 0;
            
            if(tar->showbar)
                if(tar->topbar)
                    tar_bar_offset = -gwm.bh;
                else
                    tar_bar_offset = gwm.bh;
            else
                tar_bar_offset = 0;

            XIWarpPointer(gwm.dpy, dp->mptr->info.deviceid, None, None, 0, 0, 0, 0, cur->wx - tar->wx, (cur->wy + cur_bar_offset) - (tar->wy + tar_bar_offset));
        }

        gwm.forcedfocusmon = tar;

        XSync(gwm.dpy, False);
        
        swap_int(&cur->nmaster, &tar->nmaster);
        swap_ulong(&cur->barwin, &tar->barwin);
        swap_float(&cur->mfact, &tar->mfact);
        swap_int(&cur->rmaster, &tar->rmaster);
        swap_int(&cur->num, &tar->num);
        swap_int(&cur->mx, &tar->mx);
        swap_int(&cur->my, &tar->my);
        swap_int(&cur->mw, &tar->mw);
        swap_int(&cur->mh, &tar->mh);
        swap_int(&cur->wx, &tar->wx);
        swap_int(&cur->wy, &tar->wy);
        swap_int(&cur->ww, &tar->ww);
        swap_int(&cur->wh, &tar->wh);
        
        if(tar2)
        {
            swap_int(&cur->nmaster, &tar->nmaster);
            swap_ulong(&cur->barwin, &tar2->barwin);
            swap_float(&cur->mfact, &tar->mfact);
            swap_int(&cur->rmaster, &tar->rmaster);
            swap_int(&cur->num, &tar2->num);
            swap_int(&cur->mx, &tar2->mx);
            swap_int(&cur->my, &tar2->my);
            swap_int(&cur->mw, &tar2->mw);
            swap_int(&cur->mh, &tar2->mh);
            swap_int(&cur->wx, &tar2->wx);
            swap_int(&cur->wy, &tar2->wy);
            swap_int(&cur->ww, &tar2->ww);
            swap_int(&cur->wh, &tar2->wh);
            updatebarpos(tar2);
            XMoveResizeWindow(gwm.dpy, tar2->barwin, tar2->wx, tar2->by, tar2->ww, gwm.bh);
            arrange(tar2);
        }
        
        updatebarpos(tar);
        updatebarpos(cur);

        XMoveResizeWindow(gwm.dpy, cur->barwin, cur->wx, cur->by, cur->ww, gwm.bh);
        XMoveResizeWindow(gwm.dpy, tar->barwin, tar->wx, tar->by, tar->ww, gwm.bh);

        arrange(tar);
        arrange(cur);
        
        XSync(gwm.dpy, False);
    }

    if (dp->selmon) {
        dp->selmon->devices--;
        for (tdp = &dp->selmon->devstack; *tdp && *tdp != dp; tdp = &(*tdp)->mnext);
        *tdp = dp->mnext;
        dp->mnext = NULL;
    }

    dp->lastselmon = dp->selmon;
    dp->selmon = m;

    if (dp->selmon) {
        for (ndp = dp->selmon->devstack; ndp && ndp->mnext; ndp = ndp->mnext);
        if (ndp)
            ndp->mnext = dp;
        else
            dp->selmon->devstack = dp;
        dp->selmon->devices++;
    }

    if(gwm.forcing_focus)
    {
        while (XCheckTypedEvent(gwm.dpy, GenericEvent, &ev)) {
            fire_event(ev.type, &ev);
        }
        gwm.forcing_focus = 0;
    }
    
    drawbars();
}

void focus(DevPair *dp, Client *c)
{
    DBG("+focus %lu -> %lu\n", dp->sel ? dp->sel->win : 0, c ? c->win : 0);
    DevPair *ndp;

    if(c && dp->sel == c)
        return;

    if ((!c || !ISVISIBLE(c)) && dp->selmon)
        for (c = dp->selmon->stack; c && !ISVISIBLE(c); c = c->snext);

    if (dp->sel && dp->sel != c)
        unfocus(dp, 0);

    if (c) {
        if (c->mon != dp->selmon)
            setselmon(dp, c->mon);
        if (c->isurgent)
            seturgent(c, 0);
        if (!c->devices) {
            detachstack(c);
            attachstack(c);
        }

        grabbuttons(dp->mptr, c, 1);
        setfocus(dp, c);
    } else {
        XISetFocus(gwm.dpy, dp->mkbd->info.deviceid, gwm.root, CurrentTime);
        XISetClientPointer(gwm.dpy, None, dp->mptr->info.deviceid);
        XDeleteProperty(gwm.dpy, gwm.root, gwm.netatom[NetActiveWindow]);
    }

    setsel(dp, c);
    /* refocus other devparis*/
    for(ndp = dp->selmon->devstack; ndp; ndp = ndp->mnext)
    {
        if(dp == ndp || !ndp->sel)
            continue;
        c = ndp->sel;
        unfocus(ndp, 1);
        grabbuttons(ndp->mptr, c, 1);
        setfocus(ndp, c);
    }
	drawbars();
    DBG("-focus\n");
}

/*
 * setsel is only necessary when not calling focus after this function 
 * 
*/
void unfocus(DevPair *dp, int setfocus)
{
    XWindowChanges wc;
    DBG("+unfocus %lu\n", dp->sel ? dp->sel->win : 0);
    
    if (!dp || !dp->sel)
        return;

    grabbuttons(dp->mptr, dp->sel, 0);
    
    if(dp->sel->isfloating)
    {
        wc.stack_mode = Below;
        wc.sibling = gwm.floating_stack_helper;
        XConfigureWindow(gwm.dpy, dp->sel->win, CWSibling|CWStackMode, &wc);   
    }

    if (setfocus) {
        XISetFocus(gwm.dpy, dp->mkbd->info.deviceid, gwm.root, CurrentTime);
        XISetClientPointer(gwm.dpy, None, dp->mptr->info.deviceid);
        XDeleteProperty(gwm.dpy, gwm.root, gwm.netatom[NetActiveWindow]);
    }
}

void setfocus(DevPair *dp, Client *c)
{
    XWindowChanges wc;

    if (!c->neverfocus) {
        XISetFocus(gwm.dpy, dp->mkbd->info.deviceid, c->win, CurrentTime);
        XISetClientPointer(gwm.dpy, c->win, dp->mptr->info.deviceid);
        XChangeProperty(gwm.dpy, gwm.root, gwm.netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &(c->win), 1);

        // make sure focused window goes over other floating windows
        if(c->isfloating)
        {
            wc.stack_mode = Above;
            wc.sibling = gwm.floating_stack_helper;
            XConfigureWindow(gwm.dpy, c->win, CWSibling|CWStackMode, &wc);
        }
    }
    sendevent(c, gwm.wmatom[WMTakeFocus]);
}

void sendmon(DevPair *dp, Client *c, Monitor *m, int refocus)
{
    Monitor *prev_m = c->mon;
    if (prev_m == m)
        return;

    if(refocus)
        unfocus(dp, 1);

    detach(c);
    detachstack(c);
    
    c->mon = m;
    c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */

    attach(c);
    attachstack(c);

    if(refocus)
        focus(dp, NULL);

    if(!c->isfloating && !c->isfullscreen)
    {
        arrange(prev_m);
        arrange(m);
    }
    else if(c->isfullscreen)
    {
        if(c->dirty_resize)
            arrange(prev_m);
        c->dirty_resize = False;
        resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    }
    else if(dp->move.c != c && dp->resize.c != c && c->isfloating)
        resizeclient(c, m->mx, m->my, c->w, c->h);
}
