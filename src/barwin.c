#include "barwin.h"
#include "config.h"
#include "drw.h"

void drawbar(Monitor *m)
{
    char focus_text[512];
    int fti = 0, rfti = 0;
    int x, w, sw = 0;
    int boxs = gdrw->fonts->h / 9;
    int boxw = gdrw->fonts->h / 6 + 2;
    unsigned int i, occ = 0, urg = 0, selt = 0;
    Client *c;
    DevPair *dp;
    Clr **cur_scheme;

    if(m == gwm.forcedfocusmon)
        cur_scheme = gwm.ff_scheme;
    else
        cur_scheme = gwm.scheme;

    if (!m->showbar)
        return;
    
    /* draw status first so it can be overdrawn by tags later */
    if (m->devices) {
        drw_setscheme(gdrw, cur_scheme[SchemeNorm]);
        sw = TEXTW(gwm.stext) - gwm.lrpad + 2; /* 2px right padding */
        drw_text(gdrw, m->ww - sw, 0, sw, gwm.bh-gcfg.bhgappx, 0, gwm.stext, 0);
    }

    for (c = m->clients; c; c = c->next) {
        occ |= c->tags;
        if (c->isurgent)
            urg |= c->tags;
    }
    x = 0;
    for (i = 0; i < gtags_len; i++) {
        w = TEXTW(gtags[i]);
        drw_setscheme(gdrw, cur_scheme[m->tagset[m->seltags] & 1 << i ? CLAMP(SchemeNorm + m->devices, SchemeNorm, SchemeSel3) : SchemeNorm]);
        drw_text(gdrw, x, 0, w, gwm.bh-gcfg.bhgappx, gwm.lrpad / 2, gtags[i], urg & 1 << i);
        if (occ & 1 << i) {
            for (dp = m->devstack; dp && !selt; dp = dp->mnext) {
                selt |= dp->sel ? dp->sel->tags : 0;
            }
            drw_rect(gdrw, x + boxs, boxs, boxw, boxw,
                m->devices && selt & 1 << i,
                urg & 1 << i);
        }
        x += w;
    }
    w = TEXTW(m->ltsymbol);
    drw_setscheme(gdrw, cur_scheme[SchemeNorm]);
    x = drw_text(gdrw, x, 0, w, gwm.bh-gcfg.bhgappx, gwm.lrpad / 2, m->ltsymbol, 0);

    /*
     * 1  2  3  4  5  6  7  8  9  []=  [dev01,dev02] user@vm01: ~/Downloads | [dev03] user@vm01: ~
    */
    if ((w = m->ww - sw - x) > (gwm.bh-gcfg.bhgappx)) {
        focus_text[0] = 0;
        for (c = m->clients, i = 0; c; c = c->next) {
            if (!c->devices)
                continue;
            if (!i) {
                if ((rfti = snprintf(&focus_text[fti], sizeof(focus_text) - fti, "%s - %d - %s", c->prefix_name, m->nmaster, c->name)) < 0)
                    break;
            }
            else {
                if ((rfti = snprintf(&focus_text[fti], sizeof(focus_text) - fti, " | %s - %d - %s", c->prefix_name, m->nmaster, c->name)) < 0)
                    break;
            }
            fti += rfti;
            i++;
        }
        if (m->devstack) {
            drw_setscheme(gdrw, cur_scheme[CLAMP(SchemeNorm + m->devices, SchemeNorm, SchemeSel3)]);
            drw_text(gdrw, x, 0, w, gwm.bh-gcfg.bhgappx, gwm.lrpad / 2, focus_text, 0);
        }
        else {
            drw_setscheme(gdrw, cur_scheme[SchemeNorm]);
            drw_rect(gdrw, x, 0, w, gwm.bh-gcfg.bhgappx, 1, 1);
            drw_text(gdrw, x, 0, w, gwm.bh-gcfg.bhgappx, gwm.lrpad / 2, "", 0);
        }
    }
    
    drw_map(gdrw, m->barwin, 0, 0, m->ww, gwm.bh-gcfg.bhgappx);
}

void drawbars(void)
{
    Monitor *m;

    for (m = gwm.mons; m; m = m->next)
        drawbar(m);
}

void updatebarpos(Monitor *m)
{
    m->wy = m->my;
    m->wh = m->mh;
    if (m->showbar) {
        m->wh -= gwm.bh;
        m->by = m->topbar ? m->wy : m->wy + m->wh;
        m->wy = m->topbar ? m->wy + gwm.bh : m->wy;
    } else
        m->by = -gwm.bh;
}

void updatebars(void)
{
    Monitor *m;
    XWindowChanges wc;
    XSetWindowAttributes wa = {
        .override_redirect = True,
        .background_pixmap = ParentRelative,
        .event_mask = ExposureMask
    };
    XClassHint ch = {"mpwm", "mpwm"};
    ptrevm.deviceid = XIAllMasterDevices;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Motion);
    XISetMask(ptrmask, XI_ButtonPress);

    for (m = gwm.mons; m; m = m->next)
    {
        if (m->barwin)
            continue;

        m->barwin = XCreateWindow(gwm.dpy, gwm.root, m->wx, m->by, m->ww, gwm.bh, 0, DefaultDepth(gwm.dpy, gwm.screen),
                CopyFromParent, DefaultVisual(gwm.dpy, gwm.screen),
                CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
        
        wc.stack_mode = Above;

        if(gwm.highest_barwin == None)
        {
            gwm.lowest_barwin = m->barwin;
            wc.sibling = gwm.lowest_barwin;
        }
        else
            wc.sibling = gwm.highest_barwin;
        
        XConfigureWindow(gwm.dpy, m->barwin, CWSibling|CWStackMode, &wc);

        // stack barwins right above each other
        gwm.highest_barwin = m->barwin;

        XDefineCursor(gwm.dpy, m->barwin, gwm.cursor[CurNormal]->cursor);
        XMapRaised(gwm.dpy, m->barwin);
        XSetClassHint(gwm.dpy, m->barwin, &ch);
        XISelectEvents(gwm.dpy, m->barwin, &ptrevm, 1);
    }

    // create invisible window that acts as a layer between tiled and floating windows
    gwm.floating_stack_helper = XCreateSimpleWindow(gwm.dpy, gwm.root, 0, 0, 1, 1, 0, 0, 0);

    wc.stack_mode = Above;
    wc.sibling = gwm.highest_barwin;
    XConfigureWindow(gwm.dpy, gwm.floating_stack_helper, CWSibling|CWStackMode, &wc);
}

void updatestatus(void)
{
    Monitor *m;
    if (!gettextprop(gwm.root, XA_WM_NAME, gwm.stext, sizeof(gwm.stext)))
        strcpy(gwm.stext, "mpwm-" VERSION);
    for (m = gwm.mons; m; m = m->next)
        drawbar(m);
}
