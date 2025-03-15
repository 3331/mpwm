#include "events.h"
#include "config.h"
#include "util.h"
#include "barwin.h"
#include "cmds.h"
#include "devpair.h"
#include "monitor.h"
#include "client.h"
#include "resolvers.h"
#include "drw.h"

#include <stdlib.h>
#include <string.h>

/* function declarations (legacy events) */
static void expose(XEvent *e);
static void destroynotify(XEvent *e);
static void unmapnotify(XEvent *e);
static void maprequest(XEvent *e);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void clientmessage(XEvent *e);
static void mappingnotify(XEvent *e);
static void propertynotify(XEvent *e);
static void genericevent(XEvent *e);

/* function declarations (xinput2 events) */
static void xi2keypress(void *ev);
static void xi2buttonpress(void *ev);
static void xi2buttonrelease(void *ev);
static void xi2motion(void *ev);
static void xi2enter(void *ev);
static void xi2focusin(void *ev);
static void xi2hierarchychanged(void *ev);

static void (*legacyhandler[LASTEvent]) (XEvent *) = {
    [Expose] = expose,
    [DestroyNotify] = destroynotify,
    [UnmapNotify] = unmapnotify,
    [MapRequest] = maprequest,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest,
    [ClientMessage] = clientmessage,
    [MappingNotify] = mappingnotify,
    [PropertyNotify] = propertynotify,
    [GenericEvent] = genericevent
};

static void (*xi2handler[XI_LASTEVENT]) (void *) = {
    [XI_KeyPress] = xi2keypress,
    [XI_ButtonPress] = xi2buttonpress,
    [XI_ButtonRelease] = xi2buttonrelease,
    [XI_Motion] = xi2motion,
    [XI_Enter] = xi2enter,
    [XI_FocusIn] = xi2focusin,
    [XI_HierarchyChanged] = xi2hierarchychanged
};

void fire_event(int ev_type, void *ev)
{
    if (legacyhandler[ev_type])
        legacyhandler[ev_type](ev); /* call handler */
}

void expose(XEvent *e)
{
    Monitor *m;
    XExposeEvent *ev = &e->xexpose;

    if (ev->count == 0 && (m = anywintomon(ev->window)))
        drawbar(m);
}

void destroynotify(XEvent *e)
{
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    DBG("+destroynotify %lu\n", ev->window);

    if ((c = wintoclient(ev->window)))
        unmanage(c, 1);
}

void maprequest(XEvent *e)
{
    static XWindowAttributes wa = {0};
    XMapRequestEvent *ev = &e->xmaprequest;

    DBG("+maprequest %lu->%lu (%d, %d)\n", ev->parent, ev->window, ev->type, ev->send_event);

    if (!XGetWindowAttributes(gwm.dpy, ev->window, &wa) || wa.override_redirect) {
        DBG("+maprequest badwindow maybe\n");
        return;
    }

    /*
    int bit_gravity;		 // one of bit gravity values
    int win_gravity;		 // one of the window gravity values
    int backing_store;		 // NotUseful, WhenMapped, Always
    unsigned long backing_planes; // planes to be preserved if possible
    unsigned long backing_pixel; // value to be used when restoring planes
    Bool save_under;		 // boolean, should bits under be saved?
    Colormap colormap;		 // color map to be associated with window
    Bool map_installed;		 // boolean, is color map currently installed
    int map_state;		 // IsUnmapped, IsUnviewable, IsViewable
    long all_event_masks;	 // set of events all people have interest in
    long your_event_mask;	 // my event mask
    long do_not_propagate_mask;  // set of events that should not propagate
    Bool override_redirect;	 // boolean value for override-redirect
    */
    
    DBG("  +maprequest %d, %d, %d, %lu, %lu\n", wa.bit_gravity, wa.win_gravity, wa.backing_store, wa.backing_planes, wa.backing_pixel);
    DBG("  +maprequest %d, %d, %d, %ld, %ld, %ld %d\n", wa.save_under, wa.map_installed, wa.map_state, wa.all_event_masks, wa.your_event_mask, wa.do_not_propagate_mask, wa.override_redirect);
    
    if (!wintoclient(ev->window))
        manage(ev->window, &wa);
}

void unmapnotify(XEvent *e)
{
    Client *c;
    XUnmapEvent *ev = &e->xunmap;

    if ((c = wintoclient(ev->window))) {
        if (ev->send_event)
            setclientstate(c, WithdrawnState);
        else
            unmanage(c, 0);
    }
}

void configurenotify(XEvent *e)
{
    DevPair *dp;
    Monitor *m;
    Client *c;
    XConfigureEvent *ev = &e->xconfigure;
    int dirty;

    DBG("+configurenotify\n");

    /* TODO: updategeom handling sucks, needs to be simplified */
    if (ev->window == gwm.root) {
        dirty = (gwm.sw != ev->width || gwm.sh != ev->height);
        gwm.sw = ev->width;
        gwm.sh = ev->height;
        DBG("NEW sw: %d, sh: %d\n", gwm.sw, gwm.sh);
        if (updategeom(NULL) || dirty) {
            drw_resize(gdrw, gwm.sw, gwm.bh);
            updatebars();
            for (m = gwm.mons; m; m = m->next) {
                for (c = m->clients; c; c = c->next)
                    if (c->isfullscreen)
                        resizeclient(c, m->mx, m->my, m->mw, m->mh);
                XMoveResizeWindow(gwm.dpy, m->barwin, m->wx, m->by, m->ww, gwm.bh);
            }
            for (dp = gwm.devpairs; dp; dp = dp->next)
                focus(dp, NULL);
            arrange(NULL);
        }
    }
}

void configurerequest(XEvent *e)
{
    Client *c;
    Monitor *m;
    int use_old_m = 1;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    
    if ((c = wintoclient(ev->window))) {
        if (ev->value_mask & CWBorderWidth) {
            c->bw = ev->border_width;
        }
        else if (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
            m = c->mon;
            if (ev->value_mask & CWX) {
                c->oldx = c->x;
                c->x = ev->x;
            }
            if (ev->value_mask & CWY) {
                c->oldy = c->y;
                c->y = ev->y;
            }
            if (ev->value_mask & CWWidth) {
                c->oldw = c->w;
                c->w = ev->width;
            }
            if (ev->value_mask & CWHeight) {
                c->oldh = c->h;
                c->h = ev->height;
            }

            // this is supposed to protect again moving a floating window completely out of bounds

            for(m = gwm.mons; m; m = m->next) {
                // client is at least halfway into a monitor, that monitor
                // will be used for bounds checks
                if ((c->x + (c->w / 2)) > m->mx &&
                    (c->x + (c->w / 2)) < m->mx + m->mw &&
                    c->isfloating)
                {
                    use_old_m = 0;
                    break;
                }
            }
            
            if(use_old_m)
            {
                m = c->mon;
            }

            if ((c->x + c->bw) > m->mx + m->mw && c->isfloating)
                c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
            if ((c->y + c->bw) > m->my + m->mh && c->isfloating)
                c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */

            if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight))) {
                DBG("+configurerequest configure 1 %lu %lu\n", c->win, ev->value_mask);
                configure(c);
            }
            if (ISVISIBLE(c)) {
                DBG("+configurerequest Move resize window %lu\n", c->win);
                XMoveResizeWindow(gwm.dpy, c->win, c->x, c->y, c->w, c->h);
            }
        } else {
            DBG("+configurerequest configure 2 %lu %lu\n", c->win, ev->value_mask);
            configure(c);
        }
    } else {
        DBG("+configurerequest unmanaged window configure %lu\n", ev->window);
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(gwm.dpy, ev->window, ev->value_mask, &wc);
    }

    XSync(gwm.dpy, False);
}

void clientmessage(XEvent *e)
{
    XClientMessageEvent *cme = &e->xclient;
    Client *c = wintoclient(cme->window);

    if (!c)
        return;
    if (cme->message_type == gwm.netatom[NetWMState]) {
        if (cme->data.l[1] == (long)gwm.netatom[NetWMFullscreen]
        || cme->data.l[2] == (long)gwm.netatom[NetWMFullscreen])
            setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
    } else if (cme->message_type == gwm.netatom[NetActiveWindow]) {
        if (!c->isurgent && !c->devices)
            seturgent(c, 1);
    }
}

void mappingnotify(XEvent *e)
{
    XMappingEvent *ev = &e->xmapping;

    DBG("+mappingnotify %lu %d\n", ev->window, ev->request);

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard) 
        grabkeys();
}

void propertynotify(XEvent *e)
{
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if ((ev->window == gwm.root) && (ev->atom == XA_WM_NAME))
        updatestatus();
    else if (ev->state == PropertyDelete)
        return; /* ignore */
    else if ((c = wintoclient(ev->window))) {
        DBG("+propertynotify %lu %lu (root: %lu)\n", ev->window, ev->atom, gwm.root);
        switch(ev->atom) {
        default: break;
        case XA_WM_TRANSIENT_FOR:
            if (!c->isfloating && (XGetTransientForHint(gwm.dpy, c->win, &trans)))
                setfloating(c, (wintoclient(trans)) != NULL, 0, 1);
            break;
        case XA_WM_NORMAL_HINTS:
            c->hintsvalid = 0;
            break;
        case XA_WM_HINTS:
            updatewmhints(c);
            drawbars();
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == gwm.netatom[NetWMName]) {
            updatetitle(c);
            if (c->devices)
                drawbar(c->mon);
        }
        if (ev->atom == gwm.netatom[NetWMWindowType])
            updatewindowtype(c);
    }
}

void genericevent(XEvent *e)
{
    int cookie = 0;
    if(e->xcookie.extension != gwm.xi2opcode) {
        return;
    }
    
    if (xi2handler[e->xcookie.evtype] && (cookie = XGetEventData(gwm.dpy, &e->xcookie)))
        xi2handler[e->xcookie.evtype](e->xcookie.data);

    if (cookie) {
        XFreeEventData(gwm.dpy, &e->xcookie);
    }
}

void xi2keypress(void *ev)
{
    XIDeviceEvent *e = ev;
    DevPair *dp = getdevpair(e->deviceid);
    unsigned int i;
    KeySym keysym;
    
    keysym = XkbKeycodeToKeysym(gwm.dpy, (KeyCode)e->detail, 0, 0);
    dp->lastevent = e->time;
    for (i = 0; i < gkeys_len; i++) {
        if (keysym == gkeys[i].keysym
        && CLEANMASK(gkeys[i].mod) == CLEANMASK(e->mods.effective)
        && gkeys[i].func)
            gkeys[i].func(dp, &(gkeys[i].arg));
    }
}

void xi2buttonpress(void *ev)
{
    unsigned int i, x, click = ClkRootWin;
    XIDeviceEvent *e = ev;
    DevPair *dp = getdevpair(e->deviceid);
    Arg arg = {0};
    Monitor *m;
    Client *c;
    
    /* focus monitor if necessary */
    if ((m = wintomon(dp, e->event)) && m != dp->selmon) {
        unfocus(dp, 1);
        setselmon(dp, m);
        focus(dp, NULL);
    }
    if (e->event == dp->selmon->barwin) {
        i = x = 0;
        do
            x += TEXTW(gtags[i]);
        while (e->event_x >= x && ++i < gtags_len);
        if (i < gtags_len) {
            click = ClkTagBar;
            arg.ui = 1 << i;
        } else if (e->event_x < x + TEXTW(dp->selmon->ltsymbol))
            click = ClkLtSymbol;
        else if (e->event_x > dp->selmon->ww - (int)TEXTW(gwm.stext))
            click = ClkStatusText;
        else
            click = ClkWinTitle;
    } else if ((c = wintoclient(e->event))) {
        focus(dp, c);
        if (dp->sel == c) {
            XIAllowEvents(gwm.dpy, dp->mptr->info.deviceid, XIReplayDevice, CurrentTime);
            click = ClkClientWin;
        }
    }

    dp->lastevent = e->time;
    dp->lastdetail = e->detail;

    for (i = 0; i < gbuttons_len; i++)
        if (click == gbuttons[i].click && gbuttons[i].func && gbuttons[i].button == e->detail
        && CLEANMASK(gbuttons[i].mask) == CLEANMASK(e->mods.effective))
            gbuttons[i].func(dp, click == ClkTagBar && gbuttons[i].arg.i == 0 ? &arg : &gbuttons[i].arg);
}

void postmoveselmon(DevPair *dp, Client *c)
{
    Monitor *m;
    int x, y;

    if(!c)
        return;

    if(!c->isfloating)
        return;
    
    if(c->isfullscreen)
    {
        if(!getrootptr(dp, &x, &y))
            return;
        if(!(m = recttomon(dp, x, y, 1, 1)))
            return;
    }
    else
        if (!(m = recttomon(dp, c->x, c->y, c->w, c->h)))
            return;
    
    if(dp->selmon != m)
    {
        sendmon(dp, c, m, 0);
        setselmon(dp, m);
    }
}

void xi2buttonrelease(void *ev)
{
    XIDeviceEvent *e = ev;
    DevPair *dp = getdevpair(e->deviceid);
    Client **pc;
    Motion *mm;

    dp->lastevent = e->time;
    if ((*(pc = &dp->move.c) && (mm = &dp->resize) && dp->move.detail == e->detail) ||
        (*(pc = &dp->resize.c) && (mm = &dp->move) && dp->resize.detail == e->detail)) {
        int newcursor = (dp->move.c && dp->resize.c) ? ((mm == &dp->resize) ? CurResize : CurMove) : CurNormal;
        dp->lastdetail = mm->detail;
        switch(newcursor) {
            case CurMove:
                movemouse(dp, &(Arg){0});
                break;
            case CurResize:
                resizemouse(dp, &(Arg){0});
                break;
            case CurNormal:
                XIUngrabDevice(gwm.dpy, dp->mptr->info.deviceid, CurrentTime);
                break;
        }
        postmoveselmon(dp, *pc);
        *pc = NULL;
    }
}

void xi2motion(void *ev)
{
    XIDeviceEvent *e = ev;
    Monitor *m = NULL;
    Client *c;
    Window sw;
    DevPair *dp = getdevpair(e->deviceid);

    if (!e->child) {
        sw = e->event;
        for(m = gwm.mons; m; m = m->next) {
            if (m->barwin == sw) {
                sw = gwm.root;
                break;
            }
        }
    } else {
        sw = e->child;
    }
    
    if (sw == gwm.root && !dp->move.c && !dp->resize.c) {
        if (!m)
            m = recttomon(dp, e->root_x, e->root_y, 1, 1);
        if (m && m != dp->selmon) {
            DBG("+xi2motion - sel\n");
            setselmon(dp, m);
            focus(dp, NULL);
            DBG("-xi2motion - sel\n");
        }
    }

    if ((c = dp->move.c) && dp->resize.time < dp->move.time)
    {
        int nx, ny;

        if ((e->time - dp->move.time) <= (1000 / 250))
            return;

        dp->move.time = e->time;
        nx = dp->move.ox + ((int)e->root_x - dp->move.x);
        ny = dp->move.oy + ((int)e->root_y - dp->move.y);

        if (abs(dp->selmon->wx - nx) < snap)
            nx = dp->selmon->wx;
        else if (abs((dp->selmon->wx + dp->selmon->ww) - (nx + WIDTH(c))) < snap)
            nx = dp->selmon->wx + dp->selmon->ww - WIDTH(c);

        if (abs(dp->selmon->wy - ny) < snap)
            ny = dp->selmon->wy;
        else if (abs((dp->selmon->wy + dp->selmon->wh) - (ny + HEIGHT(c))) < snap)
            ny = dp->selmon->wy + dp->selmon->wh - HEIGHT(c);

        if (!c->isfloating && dp->selmon->lt[dp->selmon->sellt]->arrange && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
            setfloating(c, 1, 0, 1);

        if (!dp->selmon->lt[dp->selmon->sellt]->arrange || c->isfloating)
        {
            if(!c->isfullscreen)
                resize(c, nx, ny, c->w, c->h, 1);
            postmoveselmon(dp, c);
        }
    }
    else if ((c = dp->resize.c) && dp->resize.time > dp->move.time)
    {
        int nw, nh;

        if ((e->time - dp->resize.time) <= (1000 / 250))
            return;

        dp->resize.time = e->time;
        nw = MAX(e->root_x - dp->resize.ox - 2 * c->bw + 1, 1);
        nh = MAX(e->root_y - dp->resize.oy - 2 * c->bw + 1, 1);

        if (
            c->mon->wx + nw >= dp->selmon->wx &&
            c->mon->wx + nw <= dp->selmon->wx + dp->selmon->ww &&
            c->mon->wy + nh >= dp->selmon->wy &&
            c->mon->wy + nh <= dp->selmon->wy + dp->selmon->wh)
        {
            if (!c->isfloating && dp->selmon->lt[dp->selmon->sellt]->arrange && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
                setfloating(c, 1, 0, 1);
        }

        if (!dp->selmon->lt[dp->selmon->sellt]->arrange || c->isfloating)
        {
            if(!c->isfullscreen)
                resize(c, c->x, c->y, nw, nh, 1);
            postmoveselmon(dp, c);
        }
    }
}

void xi2enter(void *ev)
{
    Client *c;
    Monitor *m;
    XIEnterEvent *e = ev;
    DevPair *dp = getdevpair(e->deviceid);

    if ((e->mode != XINotifyNormal || e->detail == XINotifyInferior) && e->event != gwm.root)
        return;

    if(gwm.forcing_focus || dp->selmon->arranging_clients)
        return;

    DBG("+xi2enter %lu\n", e->event);

    c = wintoclient(e->event);
    m = c ? c->mon : anywintomon(e->event);

    if (m != dp->selmon)
    {
        unfocus(dp, 1);
        setselmon(dp, m);
    }
    else if (!c || c == dp->sel)
        return;
    
    focus(dp, c);
    DBG("-xi2enter %lu\n", e->event);
}

void xi2focusin(void *ev)
{
    XIFocusInEvent *e = ev;
    DevPair *dp = getdevpair(e->deviceid);
    
    if (dp->sel && e->event != dp->sel->win)
        setfocus(dp, dp->sel);
}

void xi2hierarchychanged(void *ev)
{
    DBG("+ xi2hierarchychanged\n");
    int i, x, y, idx;
    DevPair *dp;
    Device **pd;
    Device *d;
    Monitor *m;
    XIHierarchyEvent *e = ev;

    if (!e->num_info)
        return;

    /* cache added devices */
    for(i = 0; i < e->num_info; i++) {
        if (!(e->info[i].flags & (XIMasterAdded | XISlaveAdded)))
            continue;
        idx = e->info[i].deviceid;
        deviceslots[idx].info.deviceid = e->info[i].deviceid;
        deviceslots[idx].info.attachment = e->info[i].attachment;
        deviceslots[idx].info.use = e->info[i].use;
        deviceslots[idx].info.enabled = e->info[i].enabled;
    }

    /* create device pairs */
    for (i = 0; i < e->num_info; i++) {
        if (!e->info[i].flags)
            continue;

        idx = e->info[i].deviceid;

        if (deviceslots[idx].info.use != XIMasterKeyboard && deviceslots[idx].info.use != XIMasterPointer)
            continue;

        if (e->info[i].flags & XIMasterAdded) {
            if (!(dp = getdevpair(deviceslots[idx].info.attachment))) {
                dp = createdevpair();
            }

            *(deviceslots[idx].info.use == XIMasterPointer ? &dp->mptr : &dp->mkbd) = &deviceslots[idx];
            deviceslots[idx].self = dp;
            DBG("add master: %d\n", idx);
        }
    }

    /* deattach slaves before  potentially re-attaching */
    for (i = 0; i < e->num_info; i++) {
        if (!e->info[i].flags)
            continue;

        idx = e->info[i].deviceid;

        if (deviceslots[idx].info.use != XISlaveKeyboard && deviceslots[idx].info.use != XISlavePointer)
            continue;

        if (e->info[i].flags & XISlaveDetached) {
            if (!(dp = getdevpair(deviceslots[idx].info.attachment)))
                die("could not find device pair for slave device\n");
            for (pd = &dp->slaves; *pd && *pd != &deviceslots[idx]; pd = &(*pd)->snext);
            *pd = deviceslots[idx].snext;
            deviceslots[idx].self = NULL;
            deviceslots[idx].snext = NULL;
            deviceslots[idx].sprev = NULL; /* TODO: not used yet... */
            DBG("detach slave: %d\n", idx);
        }
    }

    /* attach slaves to device pairs */
    for (i = 0; i < e->num_info; i++) {
        if (!e->info[i].flags)
            continue;

        idx = e->info[i].deviceid;

        if (deviceslots[idx].info.use != XISlaveKeyboard && deviceslots[idx].info.use != XISlavePointer)
            continue;
        
        if (e->info[i].flags & (XISlaveAdded | XISlaveAttached)) {
            if (!(dp = getdevpair(deviceslots[idx].info.attachment)))
                die("could not find device pair for slave device\n");
            if (deviceslots[idx].self) {
                for (pd = &deviceslots[idx].self->slaves; *pd && *pd != &deviceslots[idx]; pd = &(*pd)->snext);
                *pd = deviceslots[idx].snext;
                deviceslots[idx].self = NULL;
                deviceslots[idx].snext = NULL;
                deviceslots[idx].sprev = NULL; /* TODO: not used yet... */
            }
            if (!dp->slaves) {
                d = &deviceslots[idx];
                dp->slaves = d;
            } else {
                for (d = dp->slaves; d && d->snext; d = d->snext);
                d->snext = &deviceslots[idx];
            }
            DBG("added slave: %d\n", idx);
            deviceslots[idx].self = dp;
        }
    }

    /* uncache removed devices */
    for(i = 0; i < e->num_info; i++) {
        if (!(e->info[i].flags & (XIMasterRemoved | XISlaveRemoved)))
            continue;
        idx = e->info[i].deviceid;
        
        /* unset master pointer if removed */
        if(deviceslots[idx].self && &deviceslots[idx] == deviceslots[idx].self->mptr)
        {
            DBG("remove mptr: %d\n", idx);
            deviceslots[idx].self->mptr = NULL;
        }
        /* unset master keyboard if removed */
        else if(deviceslots[idx].self && &deviceslots[idx] == deviceslots[idx].self->mkbd)
        {
            DBG("remove mkbd: %d\n", idx);
            deviceslots[idx].self->mkbd = NULL;
        }
        else if(deviceslots[idx].self) /* slave removal */
        {
            DBG("remove slave: %d\n", idx);
            for (pd = &deviceslots[idx].self->slaves; *pd && *pd != &deviceslots[idx]; pd = &(*pd)->snext);
            *pd = deviceslots[idx].snext;
            deviceslots[idx].self = NULL;
            deviceslots[idx].snext = NULL;
            deviceslots[idx].sprev = NULL; /* TODO: not used yet... */
        }
        
        /* detach devpair from everything if mptr and mkbd is NULL */
        if(deviceslots[idx].self && !deviceslots[idx].self->mptr && !deviceslots[idx].self->mkbd)
        {
            if(deviceslots[idx].self->slaves)
            {
                die("slaves still attached to devpair!\n");
            }

            removedevpair(deviceslots[idx].self);
            deviceslots[idx].self = NULL;
        }

        memset(&deviceslots[idx], 0, sizeof(Device));
    }

    /* initialize device pairs */
    for (dp = gwm.devpairs; dp; dp = dp->next) {
        if(dp->lastselmon)
            continue;
        if(!getrootptr(dp, &x, &y))
            continue;
        if(!(m = recttomon(dp, x, y, 1, 1)))
            continue;
        setselmon(dp, m);
        updatedevpair(dp);
    }

    DBG("- xi2hierarchychanged\n");
}
