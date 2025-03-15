#include "common.h"
#include "util.h"
#include "client.h"
#include "barwin.h"
#include "devpair.h"
#include "monitor.h"
#include "resolvers.h"

XIGrabModifiers ganymodifier[] = { { XIAnyModifier, 0 } };
unsigned int ganymodifier_len = LENGTH(ganymodifier);
unsigned char hcmask[XIMaskLen(XI_HierarchyChanged)] = {0};
unsigned char ptrmask[XIMaskLen(XI_LASTEVENT)] = {0};
unsigned char kbdmask[XIMaskLen(XI_LASTEVENT)] = {0};
XIEventMask hcevm = { .deviceid = XIAllDevices, .mask_len = sizeof(hcmask), .mask = hcmask };
XIEventMask ptrevm = { .deviceid = -1, .mask_len = sizeof(ptrmask), .mask = ptrmask };
XIEventMask kbdevm = { .deviceid = -1, .mask_len = sizeof(kbdmask), .mask = kbdmask };

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *display, XErrorEvent *ee)
{
    Client *c;
    // 12, 3, 8
    DBG("xerror: request code=%d, error code=%d, minor code=%d, serial=%lu, resourceid=%lu\n", ee->request_code, ee->error_code, ee->minor_code, ee->serial, ee->resourceid);
    
    if(ee->error_code == BadWindow)
    {
        if((c = wintoclient(ee->resourceid)) && !c->ismanaged)
        {
            detach(c);
            detachstack(c);
        }
        return 0;
    }
    
    if ((ee->request_code == gwm.xi2opcode && ee->error_code == BadMatch)
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    
    fprintf(stderr, "mpwm: fatal error: request code=%d, error code=%d\n",
        ee->request_code, ee->error_code);
    
    if(gwm.xerrorxlib)
        return gwm.xerrorxlib(display, ee); /* may call exit */
    else
        return 0;
}

int xerrordummy(__attribute__((unused)) Display * display, __attribute__((unused)) XErrorEvent * ee)
{
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(__attribute__((unused)) Display * display, __attribute__((unused)) XErrorEvent * ee)
{
    die("mpwm: another window manager is already running");
    return -1;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
    char **list = NULL;
    int n;
    XTextProperty name;

    if (!text || size == 0)
        return 0;
    text[0] = '\0';
    if (!XGetTextProperty(gwm.dpy, w, &name, atom) || !name.nitems)
        return 0;
    if (name.encoding == XA_STRING)
        strncpy(text, (char *)name.value, size - 1);
    else if (XmbTextPropertyToTextList(gwm.dpy, &name, &list, &n) >= Success && n > 0 && *list) {
        strncpy(text, *list, size - 1);
        XFreeStringList(list);
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return 1;
}

Atom getatomprop(Client *c, Atom prop)
{
    int di;
    unsigned long dl;
    unsigned char *p = NULL;
    Atom da, atom = None;

    if (XGetWindowProperty(gwm.dpy, c->win, prop, 0L, sizeof(atom), False, XA_ATOM,
        &da, &di, &dl, &dl, &p) == Success && p) {
        atom = *(Atom *)p;
        XFree(p);
    }
    return atom;
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
    while (n--)
        if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
        && unique[n].width == info->width && unique[n].height == info->height)
            return 0;
    return 1;
}
#endif /* XINERAMA */

int updategeom(DevPair *dp)
{
    int dirty = 0;
    static int found_active_xinerama = 0;

#ifdef XINERAMA
    // hack: after being idle for too long screens power off and XineramaIsActive never returns true again lol ?
    if(!found_active_xinerama)
    {
        found_active_xinerama = XineramaIsActive(gwm.dpy);
    }
    else
    {
        return 0;
    }
    
    if (found_active_xinerama) {
        DBG("XineramaIsActive sw: %d, sh: %d\n", gwm.sw, gwm.sh);
        int i, j, n, nn;
        Client *c;
        Monitor *m;
        XineramaScreenInfo *info = XineramaQueryScreens(gwm.dpy, &nn);
        XineramaScreenInfo *unique = NULL;

        for (n = 0, m = gwm.mons; m; m = m->next, n++);

        /* only consider unique geometries as separate screens */
        unique = ecalloc(nn, sizeof(XineramaScreenInfo));

        for (i = 0, j = 0; i < nn; i++)
            if (isuniquegeom(unique, j, &info[i]))
                memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
        
        XFree(info);
        nn = j;

        /* new monitors if nn > n */
        for (i = n; i < nn; i++) {
            DBG("added %d\n", i);
            insertmon(gwm.mons_end, createmon());
        }

        for (i = 0, m = gwm.mons; i < nn && m; m = m->next, i++) {
            if (i >= n
            || unique[i].x_org != m->mx || unique[i].y_org != m->my
            || unique[i].width != m->mw || unique[i].height != m->mh)
            {
                dirty = 1;
                m->num = i;
                m->mx = m->wx = unique[i].x_org;
                m->my = m->wy = unique[i].y_org;
                m->mw = m->ww = unique[i].width;
                m->mh = m->wh = unique[i].height;
                DBG("updated mon(%d) sizes: %d, %d, %d, %d\n", i, m->mx, m->my, m->mw, m->mh);
                updatebarpos(m);
            }
        }

        /* removed monitors if n > nn */
        for (i = nn; i < n; i++) {
            DBG("removed %d\n", i);
            while ((c = m->clients)) {
                dirty = 1;
                gwm.mons_end->clients = c->next;
                detachstack(c);
                c->mon = gwm.mons;
                attach(c);
                attachstack(c);
            }
            if (dp && gwm.mons_end == dp->selmon)
                setselmon(dp, gwm.mons);
            cleanupmon(gwm.mons_end);
        }

        free(unique);
    } else
#endif /* XINERAMA */
    { /* default monitor setup */
        DBG("NOT XineramaIsActive sw: %d, sh: %d\n", gwm.sw, gwm.sh);
        if (!gwm.mons) {
            insertmon(gwm.mons_end, createmon());
        }
        if (gwm.mons->mw != gwm.sw || gwm.mons->mh != gwm.sh) {
            dirty = 1;
            gwm.mons->mw = gwm.mons->ww = gwm.sw;
            gwm.mons->mh = gwm.mons->wh = gwm.sh;
            updatebarpos(gwm.mons);
        }
    }
    if (dirty && dp)
        setselmon(dp, gwm.mons);
    return dirty;
}
