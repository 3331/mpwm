/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of mpwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <X11/X.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "common.h"
#include "config.h"
#include "cmds.h"
#include "events.h"
#include "barwin.h"
#include "devpair.h"
#include "monitor.h"
#include "client.h"
#include "layouts.h"

#include "drw.h"
#include "util.h"

#ifdef DEBUG
static void updatedebuginfo(void);
#endif

/* function declarations */
static long getstate(Window w);
static void checkotherwm(void);
static void run(void);
static void scan(void);
static void setup(void);
static void cleanup(void);

static const char *colors[][5]      = {
    /*               fg         bg         border     border+1    border+2 */
    [SchemeNorm]  = { col_gray3, col_gray1, col_gray2 },
    [SchemeSel]   = { col_gray4, col_cyan,  col_cyan },  /* one device */
    [SchemeSel2]  = { col_gray4, col_green, col_green }, /* two devices */
    [SchemeSel3]  = { col_gray4, col_red,   col_red },   /* three devices */
};

static const char *ff_colors[][5]      = {
    /*               fg         bg         border     border+1    border+2 */
    [SchemeNorm]  = { col_gray3, col_gray1, col_gray2 },
    [SchemeSel]   = { col_gray4, col_ff_cyan,  col_ff_cyan },  /* one device */
    [SchemeSel2]  = { col_gray4, col_ff_green, col_ff_green }, /* two devices */
    [SchemeSel3]  = { col_gray4, col_ff_red,   col_ff_red },   /* three devices */
};

int log_fd = 2;

Wm gwm = {0};

void checkotherwm(void)
{
    gwm.xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(gwm.dpy, DefaultRootWindow(gwm.dpy), SubstructureRedirectMask);
    XSync(gwm.dpy, False);
    XSetErrorHandler(xerror);
    XSync(gwm.dpy, False);
}

void cleanup(void)
{
    Arg a = {.ui = ~0};
    Layout foo = { "", NULL };
    Monitor *m;
    size_t i;

    /* TODO: turbo tune this, no need to loop through everything... */
    for(i = 0; i < LENGTH(deviceslots); i++) {
        if(deviceslots[i].info.use != XIMasterPointer)
            break;
        view(deviceslots[i].self, &a);

        if(deviceslots[i].self->selmon)
            deviceslots[i].self->selmon->lt[deviceslots[i].self->selmon->sellt] = &foo;
    }

    for (m = gwm.mons; m; m = m->next)
        while (m->stack)
            unmanage(m->stack, 0);
    XIUngrabKeycode(gwm.dpy, XIAllMasterDevices, XIAnyKeycode, gwm.root, ganymodifier_len, ganymodifier);
    while (gwm.mons)
        cleanupmon(gwm.mons);
    for (i = 0; i < CurLast; i++)
        drw_cur_free(gdrw, gwm.cursor[i]);
    for (i = 0; i < LENGTH(colors); i++)
        free(gwm.scheme[i]);
    for (i = 0; i < LENGTH(ff_colors); i++)
        free(gwm.ff_scheme[i]);
    free(gwm.ff_scheme);
    free(gwm.scheme);
    XDestroyWindow(gwm.dpy, gwm.wmcheckwin);
    drw_free(gdrw);
    XSync(gwm.dpy, False);
    XISetFocus(gwm.dpy, XIAllMasterDevices, None, CurrentTime);
    XISetClientPointer(gwm.dpy, None, XIAllMasterDevices);
    XDeleteProperty(gwm.dpy, gwm.root, gwm.netatom[NetActiveWindow]);
}

Monitor *createmon(void)
{
    Monitor *m;

    m = ecalloc(1, sizeof(Monitor));
    m->tagset[0] = m->tagset[1] = 1;
    m->mfact = mfact;
    m->nmaster = nmaster;
    m->showbar = showbar;
    m->topbar = topbar;
    m->rmaster = rmaster;
    m->lt[0] = &glayouts[0];
    m->lt[1] = &glayouts[1 % glayouts_len];
    strncpy(m->ltsymbol, glayouts[0].symbol, sizeof(m->ltsymbol)-1);
    return m;
}

long getstate(Window w)
{
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;

    if (XGetWindowProperty(gwm.dpy, w, gwm.wmatom[WMState], 0L, 2L, False, gwm.wmatom[WMState],
        &real, &format, &n, &extra, (unsigned char **)&p) != Success)
        return -1;
    if (n != 0)
        result = *p;
    XFree(p);
    return result;
}

#ifdef DEBUG
void __attribute__((unused)) updatedebuginfo(void)
{
    DevPair *dp;
    Monitor *m;
    Client *c;
    int mn = 0, cn, i;
    for (m = gwm.mons; m; m = m->next) {
        DBG("[%d] Monitor (%d devices)\n", mn, m->devices);
        cn = 0;
        for (c = m->stack; c; c = c->snext) {
            DBG("    [%d%s] Client %ld (title: %s, devices: %d)\n", cn, m->stack == c ? " - STACK" : "", c->win, c->name, c->devices);
            i = 0;
            for (dp = devpairs; dp; dp = dp->next, i++) {
                if (dp->sel == c) {
                    DBG("        [%d] Device %d (use: %d)\n", i, i, dp->mkbd->info.use);
                }
            }
            cn++;
        }
        mn++;
    }
}
#endif

void run(void)
{
    gwm.running = 1;

    XEvent ev;
    /* main event loop */
    XSync(gwm.dpy, False);
    while (gwm.running && !XNextEvent(gwm.dpy, &ev)) {
        fire_event(ev.type, &ev);
    }
}

void scan(void)
{
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;
    
    if (XQueryTree(gwm.dpy, gwm.root, &d1, &d2, &wins, &num)) {
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(gwm.dpy, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(gwm.dpy, wins[i], &d1))
                continue;
            if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState) {
                manage(wins[i], &wa);
            }
        }
        for (i = 0; i < num; i++) { /* now the transients */
            if (!XGetWindowAttributes(gwm.dpy, wins[i], &wa))
                continue;
            if (XGetTransientForHint(gwm.dpy, wins[i], &d1)
            && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)) {
                manage(wins[i], &wa);
            }
        }
        if (wins)
            XFree(wins);
    }
}

Atom Dbg_XInternAtom(const char *name, Bool only_if_exists)
{
    Atom atom = XInternAtom(gwm.dpy, name, only_if_exists);
    DBG("[%s] %lu\n", name, atom);
    return atom;
}

void setup(void)
{
    uint32_t i;
    XSetWindowAttributes wa;
    Atom utf8string;
    struct sigaction sa;

#ifdef DEBUG
    unlink("/home/user/.mpwm.log");
    log_fd = open("/home/user/.mpwm.log", O_RDWR | O_CREAT, 0644);
    if(log_fd == -1)
        die("could not open log_fd, why not..\n");
#endif

    /* do not transform children into zombies when they terminate */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, NULL);

    /* clean up any zombies (inherited from .xinitrc etc) immediately */
    while (waitpid(-1, NULL, WNOHANG) > 0);

    /* init screen */
    gwm.mons = NULL;
    gwm.mons_end = NULL;
    gwm.spawnmon = NULL;
    gwm.forcedfocusmon = NULL;
    gwm.screen = DefaultScreen(gwm.dpy);
    gwm.sw = DisplayWidth(gwm.dpy, gwm.screen);
    gwm.sh = DisplayHeight(gwm.dpy, gwm.screen);
    gwm.root = RootWindow(gwm.dpy, gwm.screen);

    gdrw = drw_create(gwm.dpy, gwm.screen, gwm.root, gwm.sw, gwm.sh);
    if (!drw_fontset_create(gdrw, gcfg.fonts, gcfg.fonts_len))
        die("no fonts could be loaded.");

    gwm.lrpad = gdrw->fonts->h;
    gwm.bh = gdrw->fonts->h + 2 + gcfg.bhgappx;
    updategeom(NULL);

    /* init atoms */
    utf8string = Dbg_XInternAtom("UTF8_STRING", False);
    gwm.wmatom[WMProtocols] = Dbg_XInternAtom("WM_PROTOCOLS", False);
    gwm.wmatom[WMIgnoreEnter] = Dbg_XInternAtom("WM_IGNORE_ENTER", False);
    gwm.wmatom[WMNormalEnter] = Dbg_XInternAtom("WM_NORMAL_ENTER", False);
    gwm.wmatom[WMDelete] = Dbg_XInternAtom("WM_DELETE_WINDOW", False);
    gwm.wmatom[WMState] = Dbg_XInternAtom("WM_STATE", False);
    gwm.wmatom[WMTakeFocus] = Dbg_XInternAtom("WM_TAKE_FOCUS", False);
    gwm.netatom[NetActiveWindow] = Dbg_XInternAtom("_NET_ACTIVE_WINDOW", False);
    gwm.netatom[NetSupported] = Dbg_XInternAtom("_NET_SUPPORTED", False);
    gwm.netatom[NetWMName] = Dbg_XInternAtom("_NET_WM_NAME", False);
    gwm.netatom[NetWMState] = Dbg_XInternAtom("_NET_WM_STATE", False);
    gwm.netatom[NetWMCheck] = Dbg_XInternAtom("_NET_SUPPORTING_WM_CHECK", False);
    gwm.netatom[NetWMFullscreen] = Dbg_XInternAtom("_NET_WM_STATE_FULLSCREEN", False);
    gwm.netatom[NetWMWindowType] = Dbg_XInternAtom("_NET_WM_WINDOW_TYPE", False);
    gwm.netatom[NetWMWindowTypeDialog] = Dbg_XInternAtom("_NET_WM_WINDOW_TYPE_DIALOG", False);
    gwm.netatom[NetWMTooltip] = Dbg_XInternAtom("_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    gwm.netatom[NetWMPopupMenu] = Dbg_XInternAtom("_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    gwm.netatom[NetClientList] = Dbg_XInternAtom("_NET_CLIENT_LIST", False);

    /* init cursors */
    gwm.cursor[CurNormal] = drw_cur_create(gdrw, XC_left_ptr);
    gwm.cursor[CurResize] = drw_cur_create(gdrw, XC_sizing);
    gwm.cursor[CurMove] = drw_cur_create(gdrw, XC_fleur);

    /* init appearance */
    gwm.scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
    for (i = 0; i < LENGTH(colors); i++)
        gwm.scheme[i] = drw_scm_create(gdrw, colors[i], LENGTH(*colors));

    gwm.ff_scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
    for (i = 0; i < LENGTH(ff_colors); i++)
        gwm.ff_scheme[i] = drw_scm_create(gdrw, ff_colors[i], LENGTH(*ff_colors));

    /* init bars */
    updatebars();
    updatestatus();
    /* supporting window for NetWMCheck */
    gwm.wmcheckwin = XCreateSimpleWindow(gwm.dpy, gwm.root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(gwm.dpy, gwm.wmcheckwin, gwm.netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &gwm.wmcheckwin, 1);
    XChangeProperty(gwm.dpy, gwm.wmcheckwin, gwm.netatom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *) "mpwm", 4);
    XChangeProperty(gwm.dpy, gwm.root, gwm.netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &gwm.wmcheckwin, 1);
    /* EWMH support per view */
    XChangeProperty(gwm.dpy, gwm.root, gwm.netatom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *) gwm.netatom, NetLast);
    XDeleteProperty(gwm.dpy, gwm.root, gwm.netatom[NetClientList]);
    /* set cursor on root window */
    XDefineCursor(gwm.dpy, gwm.root, gwm.cursor[CurNormal]->cursor);
    /* select events */
    memset(&wa, 0, sizeof(wa));
    wa.cursor = gwm.cursor[CurNormal]->cursor;
    wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask | PropertyChangeMask;
    XChangeWindowAttributes(gwm.dpy, gwm.root, CWEventMask|CWCursor, &wa);
    XSetWMProtocols(gwm.dpy, gwm.root, &gwm.wmatom[WMIgnoreEnter], 2);
    XSelectInput(gwm.dpy, gwm.root, wa.event_mask);
    XISetMask(hcmask, XI_HierarchyChanged);
    XISelectEvents(gwm.dpy, gwm.root, &hcevm, 1);
    XISetMask(hcmask, XI_Motion);
    XISelectEvents(gwm.dpy, gwm.root, &ptrevm, 1);
    
    /* get device map */
    initdevices();
}

int
main(int argc, char *argv[])
{
    int major = 2, minor = 1;
    if (argc == 2 && !strcmp("-v", argv[1]))
        die("mpwm-" VERSION);
    else if (argc != 1)
        die("usage: mpwm [-v]");
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
        fputs("warning: no locale support\n", stderr);
    if (!(gwm.dpy = XOpenDisplay(NULL)))
        die("mpwm: cannot open display");
    if (!XQueryExtension(gwm.dpy, "XInputExtension", &gwm.xi2opcode, &(int){0}, &(int){0}))
        die("XInputExtension not available.\n");
    if (XIQueryVersion(gwm.dpy, &major, &minor) == BadRequest)
        die("XInput 2.0 not available. Server only supports %d.%d\n", major, minor);
    checkotherwm();
    setup();
    scan();
    run();
    cleanup();
    dprintf(log_fd, "closing gracefully\n");
    close(log_fd);
    XCloseDisplay(gwm.dpy);
    return EXIT_SUCCESS;
}
