#include "client.h"
#include "config.h"
#include "util.h"
#include "devpair.h"
#include "monitor.h"
#include "resolvers.h"

/* function implementations */
void applyrules(Client *c)
{
	const char *class, *instance;
	Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(gwm.dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : "broken";
	instance = ch.res_name  ? ch.res_name  : "broken";

	for (r = gcfg.rules; r; r = r->next) {
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
            DBG("found rule for %s:%s:%s, %d, %d, %d, %d\n", r->title, r->class, r->instance, r->tags, r->isfloating, r->isfullscreen, r->monitor);
			c->isfloating = r->isfloating;
			c->isfullscreen = r->isfullscreen;
			c->tags |= r->tags;
			for (m = gwm.mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

void manage(Window w, XWindowAttributes *wa)
{
    Client *c, *t = NULL;
    Window trans = None;
    XWindowChanges wc;
    DevPair *dp;
    Clr **cur_scheme;
    int window_type;
    int apply_rules = 0;

    DBG("+manage %lu\n", w);

    if(gwm.forcedfocusmon)
        cur_scheme = gwm.ff_scheme;
    else
        cur_scheme = gwm.scheme;
    
    c = ecalloc(1, sizeof(Client));
    c->dirty_resize = True;
    c->grabbed = True;
    c->win = w;
    /* geometry */
    c->x = c->oldx = wa->x;
    c->y = c->oldy = wa->y;
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;

    updatetitle(c);
    if (XGetTransientForHint(gwm.dpy, w, &trans) && (t = wintoclient(trans))) {
        c->mon = t->mon;
        c->tags = t->tags;
    } else if (gwm.spawndev && gwm.spawndev->selmon) {
        c->mon = gwm.spawnmon ? gwm.spawnmon : gwm.spawndev->selmon;
        c->tags = c->mon->tagset[c->mon->seltags];
        gwm.spawnmon = NULL;
        apply_rules = 1;
    } else {
        die("could not find monitor for client\n");
    }

    if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
        c->x = c->mon->wx + c->mon->ww - WIDTH(c);
    if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
        c->y = c->mon->wy + c->mon->wh - HEIGHT(c);

    c->x = MAX(c->x, c->mon->wx);
    c->y = MAX(c->y, c->mon->wy);
    c->bw = gcfg.borderpx;

    if((window_type = updatewindowtype(c)) < 2)
    {
        wc.border_width = c->bw;
        XConfigureWindow(gwm.dpy, w, CWBorderWidth, &wc);
        XSetWindowBorder(gwm.dpy, w, cur_scheme[SchemeNorm][ColBorder].pixel);
        if(!window_type)
            configure(c); /* propagates border_width, if size doesn't change */
    }

    updatesizehints(c);
    updatewmhints(c);
    if(apply_rules)
        applyrules(c);

    for (dp = gwm.devpairs; dp; dp = dp->next)
        grabbuttons(dp->mptr, c, 0);

    if (!c->isfloating) {
        c->isfloating = c->oldstate = trans != None || c->isfixed;
    }

    attach(c);
    attachstack(c);

    XChangeProperty(gwm.dpy, gwm.root, gwm.netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *) &(c->win), 1);
    setclientstate(c, NormalState);

    XSync(gwm.dpy, False);

    // check if window still attached
    c->ismanaged = 1;
    if(!wintoclient(w))
    {
        DBG("-manage closed %lu %d %d\n", w, c->isfloating, c->isfullscreen);
        free(c);
        return;
    }

    setfloating(c, c->isfloating, 1, 0);
    XSelectInput(gwm.dpy, w, PropertyChangeMask|StructureNotifyMask);
    XMapWindow(gwm.dpy, c->win);

    if(!c->isfloating && !c->isfullscreen)
        arrange(c->mon);
    else
        resize(c, c->x, c->y, c->w, c->h, 0);

    if(!c->isfloating)
        focus(gwm.spawndev, NULL);

    DBG("-manage %lu %d %d\n", w, c->isfloating, c->isfullscreen);
}

void updateclientlist(void)
{
    Client *c;
    Monitor *m;
    
    XDeleteProperty(gwm.dpy, gwm.root, gwm.netatom[NetClientList]);
    for (m = gwm.mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            XChangeProperty(gwm.dpy, gwm.root, gwm.netatom[NetClientList],
                XA_WINDOW, 32, PropModeAppend,
                (unsigned char *) &(c->win), 1);
}

void unmanage(Client *c, int destroyed)
{
    Monitor *m = c->mon;
    XWindowChanges wc;
    DevPair *dp;

    DBG("+unmanage %lu %d %d\n", c->win, c->isfloating, c->isfullscreen);

    detach(c);
    detachstack(c);

    if (!destroyed) {
        wc.border_width = c->oldbw;
        XGrabServer(gwm.dpy); /* avoid race conditions */
        XSetErrorHandler(xerrordummy);
        XSelectInput(gwm.dpy, c->win, NoEventMask);
        XConfigureWindow(gwm.dpy, c->win, CWBorderWidth, &wc); /* restore border */
        XIUngrabButton(gwm.dpy, XIAllMasterDevices, XIAnyButton, c->win, ganymodifier_len, ganymodifier);
        setclientstate(c, WithdrawnState);
        XSetErrorHandler(xerror);
        XUngrabServer(gwm.dpy);
    }

    for (dp = c->devstack; dp; dp = dp->next)
    {
        if(dp->move.c == c || dp->resize.c == c)
        {
            dp->move.c = NULL;
            dp->resize.c = NULL;
            XIUngrabDevice(gwm.dpy, dp->mptr->info.deviceid, CurrentTime);
        }

        focus(dp, NULL);
    }
    
    updateclientlist();

    // fullscreen clients dont rearrange until they are unfullscreened, or unmanaged
    if(!c->isfloating || c->isfullscreen)
        arrange(m);

    DBG("-unmanage %lu %d %d\n", c->win, c->isfloating, c->isfullscreen);
    free(c);
}

void attach(Client *c)
{
    c->next = c->mon->clients;
    c->mon->clients = c;
}

void append(Client *c)
{
    Client *tc;
    if(!c->mon->clients)
    {
        c->mon->clients = c;
        return;
    }
    for (tc = c->mon->clients; tc->next; tc = tc->next);
    tc->next = c;
    c->next = NULL;
}

void detach(Client *c)
{
    Client **tc;

    for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
    *tc = c->next;
    c->next = NULL;
}

void attachstack(Client *c)
{
    c->snext = c->mon->stack;
    c->mon->stack = c;
}

void detachstack(Client *c)
{
    Client **tc;

    for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
    *tc = c->snext;
}

Client *nexttiled(Client *c)
{
    for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
    return c;
}

void setfullscreen(Client *c, int fullscreen)
{
    DBG("+setfullscreen %lu %d\n", c->win, fullscreen);
    XWindowChanges wc;

    if (fullscreen && !c->isfullscreen)
    {
        XChangeProperty(gwm.dpy, c->win, gwm.netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char*)&gwm.netatom[NetWMFullscreen], 1);
        c->isfullscreen = 1;
        c->dirty_resize = True;
        c->oldstate = c->isfloating;
        c->oldbw = c->bw;
        c->bw = 0;
        c->isfloating = 1;
        resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
        wc.stack_mode = Above;
        wc.border_width = c->bw;
        wc.sibling = gwm.floating_stack_helper;
        XConfigureWindow(gwm.dpy, c->win, CWSibling|CWStackMode|CWBorderWidth, &wc);
        // no need to arrange, fullscreen window is above everything anyway
    }
    else if (!fullscreen && c->isfullscreen)
    {
        XChangeProperty(gwm.dpy, c->win, gwm.netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char*)0, 0);
        c->isfullscreen = 0;
        c->isfloating = c->oldstate;
        c->bw = c->oldbw;
        c->x = c->oldx;
        c->y = c->oldy;
        c->w = c->oldw;
        c->h = c->oldh;
        if(c->isfloating)
            resizeclient(c, c->x, c->y, c->w, c->h);
        else
        {
            c->dirty_resize = True;
            setfloating(c, 0, 1, 1);
        }
    }
}

void setfloating(Client *c, int floating, int force, int should_arrange)
{
    DBG("+setfloating %lu %d, %d, %d\n", c->win, floating, force, should_arrange);

    XWindowChanges wc;

    // force floating on fixed windows
    floating = c->isfixed ? 1 : floating;

    if (floating && (!c->isfloating || force))
    {
        c->isfloating = 1;
        wc.stack_mode = Above;
        wc.sibling = gwm.floating_stack_helper;
        XConfigureWindow(gwm.dpy, c->win, CWSibling|CWStackMode, &wc);

        if(should_arrange)
        {
            resize(c, c->x, c->y, c->w, c->h, 0);
            arrange(c->mon);
        }
    }
    else if (!floating && (c->isfloating || force))
    {
        c->isfloating = 0;
        wc.stack_mode = Below;
        wc.sibling = gwm.lowest_barwin;
        XConfigureWindow(gwm.dpy, c->win, CWSibling|CWStackMode, &wc);

        if(should_arrange)
            arrange(c->mon);
    }
}

void seturgent(Client *c, int urg)
{
    XWMHints *wmh;

    c->isurgent = urg;
    if (!(wmh = XGetWMHints(gwm.dpy, c->win)))
        return;
    wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
    XSetWMHints(gwm.dpy, c->win, wmh);
    XFree(wmh);
}

void setclientstate(Client *c, long state)
{
    long data[] = { state, None };
    XChangeProperty(gwm.dpy, c->win, gwm.wmatom[WMState], gwm.wmatom[WMState], 32, PropModeReplace, (unsigned char *)data, 2);
}

void updatetitle(Client *c)
{
    if (!gettextprop(c->win, gwm.netatom[NetWMName], c->name, sizeof(c->name)))
        gettextprop(c->win, XA_WM_NAME, c->name, sizeof(c->name));
    if (c->name[0] == '\0') /* hack to mark broken clients */
        strcpy(c->name, "broken");
}

/*
 * return 1 when window is a special type
*/
int updatewindowtype(Client *c)
{
    int ret = 0;
    Atom state = getatomprop(c, gwm.netatom[NetWMState]);
    Atom wtype = getatomprop(c, gwm.netatom[NetWMWindowType]);

    if (state == gwm.netatom[NetWMFullscreen])
    {
        setfullscreen(c, 1);
        ret = 2;
    }

    if (wtype == gwm.netatom[NetWMWindowTypeDialog])
    {
        setfloating(c, 1, 0, 1);
        ret = 1;
    }

    return ret;
}

void updatesizehints(Client *c)
{
    long msize;
    XSizeHints size = {0};

    if (!XGetWMNormalHints(gwm.dpy, c->win, &size, &msize)) {
        /* size is uninitialized, ensure that size.flags aren't used */
        size.flags = PSize;
    }

    if (size.flags & PBaseSize) {
        c->basew = size.base_width;
        c->baseh = size.base_height;
    } else if (size.flags & PMinSize) {
        c->basew = size.min_width;
        c->baseh = size.min_height;
    } else
        c->basew = c->baseh = 0;
    
    if (size.flags & PResizeInc) {
        c->incw = size.width_inc;
        c->inch = size.height_inc;
    } else
        c->incw = c->inch = 0;
    
    if (size.flags & PMaxSize) {
        c->maxw = size.max_width;
        c->maxh = size.max_height;
    } else
        c->maxw = c->maxh = 0;

    if (size.flags & PMinSize) {
        c->minw = size.min_width;
        c->minh = size.min_height;
    } else if (size.flags & PBaseSize) {
        c->minw = size.base_width;
        c->minh = size.base_height;
    } else
        c->minw = c->minh = 0;

    if (size.flags & PAspect) {
        c->mina = (float)size.min_aspect.y / size.min_aspect.x;
        c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
    } else
        c->maxa = c->mina = 0.0;

    /*if(size.flags & (USPosition | PPosition)) {
        c->x = size.x;
        c->y = size.y;
    }*/

    if(size.flags & PWinGravity) {
        DBG("+updatesizehints WinGravity %lu %ld, %d\n", c->win, size.flags, size.win_gravity);
        DBG("+updatesizehints mon wsizes: %d, %d, %d, %d\n", c->mon->wx, c->mon->wy, c->mon->ww, c->mon->wh);
        DBG("+updatesizehints mon msizes: %d, %d, %d, %d\n", c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
        switch(size.win_gravity)
        {
            // lets just assume it needs to be tiled if the default value is supplied
            default:
            case NorthWestGravity:
                size.flags &= ~PWinGravity;
                break;
            case NorthGravity:
                c->x = c->mon->wx + (/* c->w + */ c->mon->ww / 2);
                c->y = c->mon->wy;
                break;
            case NorthEastGravity:
                c->x = c->mon->wx + (c->mon->ww /* - c->w*/);
                c->y = c->mon->wy;
                break;
            case WestGravity:
                c->x = c->mon->wx;
                c->y = c->mon->wy + (/* c->h + */ c->mon->wh / 2);
                break;
            case CenterGravity:
                c->x = c->mon->wx + (/* c->w + */ c->mon->ww / 2);
                c->y = c->mon->wy + (/* c->h + */ c->mon->wh / 2);
                break;
            case EastGravity:
                c->x = c->mon->wx + (c->mon->ww /* - c->w*/);
                c->y = c->mon->wy + (/* c->h + */ c->mon->wh / 2);
                break;
            case SouthWestGravity:
                c->x = c->mon->wx;
                c->y = c->mon->wy + (c->mon->wh /* - c->h*/);
                break;
            case SouthGravity:
                c->x = c->mon->wx + (/* c->w + */ c->mon->ww / 2);
                c->y = c->mon->wy + (c->mon->wh /* - c->h*/);
                break;
            case SouthEastGravity:
                c->x = c->mon->wx + (c->mon->ww /* - c->w*/);
                c->y = c->mon->wy + (c->mon->wh /* - c->h*/);
                break;
            case StaticGravity:
                break;
        }
    }

    c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);

    // only allow turning a window in to a floating window
    if(!c->isfloating)
        setfloating(c, !!(size.flags & (PSize | PWinGravity)), 0, c->ismanaged);
    
    c->hintsvalid = 1;
}

void updatewmhints(Client *c)
{
    XWMHints *wmh;

    if ((wmh = XGetWMHints(gwm.dpy, c->win))) {
        if (c->devices && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(gwm.dpy, c->win, wmh);
        } else
            c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
        if (wmh->flags & InputHint)
            c->neverfocus = !wmh->input;
        else
            c->neverfocus = 0;
        XFree(wmh);
    }
}

void resize(Client *c, int x, int y, int w, int h, int interact)
{
    DBG("+resize %lu %d, %d, %d, %d", c->win, x, y, w, h);
    if (applysizehints(c, &x, &y, &w, &h, interact) || c->dirty_resize)
    {
        c->dirty_resize = False;
        resizeclient(c, x, y, w, h);
        DBG(" -> resizeclient");
    }
    DBG("\n");
}

void resizeclient(Client *c, int x, int y, int w, int h)
{
    XWindowChanges wc;

    c->oldx = c->x; c->x = wc.x = x;
    c->oldy = c->y; c->y = wc.y = y;
    c->oldw = c->w; c->w = wc.width = w;
    c->oldh = c->h; c->h = wc.height = h;
    wc.border_width = c->bw;
    XConfigureWindow(gwm.dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
    configure(c);
    XSync(gwm.dpy, False);
}

int applysizehints(Client *c, int * __restrict x, int * __restrict y, int * __restrict w, int * __restrict h, int interact)
{
    int baseismin;
    Monitor *m = c->mon;

    /* set minimum possible */
    *w = MAX(1, *w);
    *h = MAX(1, *h);
    if (interact) {
        if (*x > gwm.sw)
            *x = gwm.sw - WIDTH(c);
        if (*y > gwm.sh)
            *y = gwm.sh - HEIGHT(c);
        if (*x + *w + 2 * c->bw < 0)
            *x = 0;
        if (*y + *h + 2 * c->bw < 0)
            *y = 0;
    } else {
        if (*x >= m->wx + m->ww)
            *x = m->wx + m->ww - WIDTH(c);
        if (*y >= m->wy + m->wh)
            *y = m->wy + m->wh - HEIGHT(c);
        if (*x + *w + 2 * c->bw <= m->wx)
            *x = m->wx;
        if (*y + *h + 2 * c->bw <= m->wy)
            *y = m->wy;
    }
    if (*h < gwm.bh)
        *h = gwm.bh;
    if (*w < gwm.bh)
        *w = gwm.bh;
    if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
        if (!c->hintsvalid)
            updatesizehints(c);
        /* see last two sentences in ICCCM 4.1.2.3 */
        baseismin = c->basew == c->minw && c->baseh == c->minh;
        if (!baseismin) { /* temporarily remove base dimensions */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for aspect limits */
        if (c->mina > 0 && c->maxa > 0) {
            if (c->maxa < (float)*w / *h)
                *w = *h * c->maxa + 0.5;
            else if (c->mina < (float)*h / *w)
                *h = *w * c->mina + 0.5;
        }
        if (baseismin) { /* increment calculation requires this */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for increment value */
        if (c->incw)
            *w -= *w % c->incw;
        if (c->inch)
            *h -= *h % c->inch;
        /* restore base dimensions */
        *w = MAX(*w + c->basew, c->minw);
        *h = MAX(*h + c->baseh, c->minh);
        if (c->maxw)
            *w = MIN(*w, c->maxw);
        if (c->maxh)
            *h = MIN(*h, c->maxh);
    }
    return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

int sendevent(Client *c, Atom proto)
{
    int n;
    Atom *protocols;
    int exists = 0;
    XEvent ev;

    if(!c)
        return exists;

    if (XGetWMProtocols(gwm.dpy, c->win, &protocols, &n)) {
        while (!exists && n--)
            exists = protocols[n] == proto;
        XFree(protocols);
    }
    if (exists) {
        char *name = XGetAtomName(gwm.dpy, proto);
        XFree(name);
        ev.type = ClientMessage;
        ev.xclient.window = c->win;
        ev.xclient.message_type = gwm.wmatom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = proto;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(gwm.dpy, c->win, False, NoEventMask, &ev);
    }
    return exists;
}

void configure(Client *c)
{
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = gwm.dpy;
    ce.event = c->win;
    ce.window = c->win;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->w;
    ce.height = c->h;
    ce.border_width = c->bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(gwm.dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}
