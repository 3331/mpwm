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
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
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
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include <X11/Xft/Xft.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XInput2.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh+((m)->showbar ? ((m)->topbar ? 0 : bh) : 0)) - MAX((y),(m)->wy-((m)->showbar ? ((m)->topbar ? bh : 0) : 0))))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof(X) / sizeof(X[0]))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

#define MAXDEVICES 256 /* from xorg-server/include/misc.h */

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeSel2, SchemeSel3 }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetWMTooltip, NetWMPopupMenu,
       NetLast }; /* EWMH atoms */
enum { WMProtocols, WMIgnoreEnter, WMNormalEnter, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

typedef struct Monitor_t Monitor;
typedef struct Client_t Client;
typedef struct DevPair_t DevPair;
typedef struct Device_t Device;

typedef struct {
    unsigned int click;
    unsigned int mask;
    int button;
    void (*func)(DevPair*, const Arg*);
    const Arg arg;
} Button;

typedef struct Client_t {
    Client* next;
    Client* snext;
    Monitor* mon;
    Clr **prev_scheme;
    DevPair* devstack;
    char name[64];
    char prefix_name[256];
    float mina, maxa;
    int x, y, w, h;
    int oldx, oldy, oldw, oldh;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
    int bw, oldbw;
    unsigned int tags;
    int grabbed;
    int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
    int ismanaged;
    int devices;
    int dirty_resize;
    Window win;
} Client;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(DevPair*, const Arg*);
    const Arg arg;
} Key;

typedef struct {
    const char *symbol;
    void (*arrange)(Monitor*);
} Layout;

typedef struct Monitor_t {
    Monitor* next;
    Monitor* prev;
    Client* clients;
    Client* stack;
    DevPair* devstack;
    char ltsymbol[16];
    float mfact;
    int nmaster;
    int num;
    int by;               /* bar geometry */
    int mx, my, mw, mh;   /* screen size */
    int wx, wy, ww, wh;   /* window area  */
    unsigned int seltags;
    unsigned int sellt;
    unsigned int tagset[2];
    int showbar;
    int topbar;
    int rmaster;
    int devices;
    Window barwin;
    const Layout *lt[2];
} Monitor;

typedef struct {
    Client* c;
    Time time;
    int active;
    int detail;
    int x;
    int y;
    int ox;
    int oy;
} Motion;

typedef struct Device_t {
    DevPair* self;
    XIHierarchyInfo info;
    Device* snext; /* next slave */
    Device* sprev; /* prev slave (not implemented) */
} Device;

typedef struct DevPair_t {
    DevPair* next;  /* next devpair */
    DevPair* fnext; /* next devpair that has same focus */
    DevPair* mnext; /* next devpair that has same monitor */
    Monitor* selmon;
    Monitor* lastselmon;
    Device* slaves;
    Device* mptr;
    Device* mkbd;
    Client* sel;
    Motion resize;
    Motion move;
    Time lastevent;
    int lastdetail;
} DevPair;

typedef struct {
    const char *class;
    const char *instance;
    const char *title;
    unsigned int tags;
    int isfloating;
    int monitor;
} Rule;

/* function declarations (commandable) */
static void focusmon(DevPair* dp, const Arg *arg);
static void swapmon(DevPair* dp, const Arg *arg);
static void focusstack(DevPair* dp, const Arg *arg);
static void cyclestack(DevPair* dp, const Arg *arg);
static void incnmaster(DevPair* dp, const Arg *arg);
static void killclient(DevPair* dp, const Arg *arg);
static void movemouse(DevPair* dp, const Arg *arg);
static void quit(DevPair* dp, const Arg *arg);
static void resizemouse(DevPair* dp, const Arg *arg);
static void setlayout(DevPair* dp, const Arg *arg);
static void setmfact(DevPair* dp, const Arg *arg);
static void spawn(DevPair* dp, const Arg *arg);
static void tag(DevPair* dp, const Arg *arg);
static void tagmon(DevPair* dp, const Arg *arg);
static void togglebar(DevPair* dp, const Arg *arg);
static void togglefloating(DevPair* dp, const Arg *arg);
static void togglefullscreen(DevPair* dp, const Arg *arg);
static void toggleautoswapmon(DevPair* dp, const Arg *arg);
static void togglermaster(DevPair* dp, const Arg *arg);
static void togglemouse(DevPair* dp, const Arg *arg);
static void toggletag(DevPair* dp, const Arg *arg);
static void toggleview(DevPair* dp, const Arg *arg);
static void view(DevPair* dp, const Arg *arg);
static void zoom(DevPair* dp, const Arg *arg);

/* function declarations (client) */
static Client *wintoclient(Window w);
static Client *nexttiled(Client *c);
static int sendevent(Window w, Atom proto);
static void setclientstate(Client *c, long state);
static void seturgent(Client *c, int urg);
static void setfullscreen(Client *c, int fullscreen);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void applyrules(Client *c);
static void attach(Client *c);
static void append(Client *c);
static void attachstack(Client *c);
static void configure(Client *c);
static void detach(Client *c);
static void detachstack(Client* c);
static void focus(DevPair* dp, Client* c);
static void unfocus(DevPair* dp, int setfocus);
static void setfocus(DevPair* dp, Client *c);
static void manage(Window w, XWindowAttributes *wa);
static void unmanage(Client *c, int destroyed);
static void showhide(Client *c);
static void updatesizehints(Client *c);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client* c);
static void pop(DevPair* dp, Client* c);

/* layouts */
static void tile(Monitor *m);
static void monocle(Monitor *m);
static void centeredmaster(Monitor *m);

/* function declarations (monitor) */
static Monitor *createmon(void);
static Monitor *dirtomon(DevPair* dp, int dir);
static Monitor* anywintomon(Window w);
static Monitor *wintomon(DevPair* dp, Window w);
static Monitor *recttomon(DevPair* dp, int x, int y, int w, int h);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void drawbar(Monitor *m);
static void restack(Monitor* m);
static void updatebarpos(Monitor *m);
static void insertmon(Monitor* at, Monitor* m);
static void unlinkmon(Monitor* m);
static void cleanupmon(Monitor *m);
static void sendmon(DevPair* dp, Client* c, Monitor* m, int refocus);

/* function declarations (devices) */
static DevPair* createdevpair(void);
static void removedevpair(DevPair* dp);
static DevPair* getdevpair(int deviceid);
static void updatedevpair(DevPair* dp);
static void grabdevicekeys(Device* mkbd);
static void grabdevicebuttons(Device* mptr);
static void grabbuttons(Device* mptr, Client *c, int focused);
static void setsel(DevPair* dp, Client* c);
static void setselmon(DevPair* dp, Monitor* m);
static int getrootptr(DevPair* dp, int* x, int* y);
static int updategeom(DevPair* dp);
static void updatedebuginfo(void);

/* function declarations */
static void drawbars(void);
static void initdevices(void);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void checkotherwm(void);
static void run(void);
static void scan(void);
static void setup(void);
static void cleanup(void);
static void updatebars(void);
static void updateclientlist(void);
static void updatenumlockmask(void);
static void updatestatus(void);

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
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);

/* function declarations (xinput2 events) */
static void xi2keypress(void *ev);
static void xi2buttonpress(void *ev);
static void xi2buttonrelease(void *ev);
static void xi2motion(void *ev);
static void xi2enter(void *ev);
static void xi2focusin(void *ev);
static void xi2hierarchychanged(void *ev);

/* variables */
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
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

static int xi2opcode;
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme, **ff_scheme;
static Display *dpy;
static Drw *drw;
static Monitor* mons, *mons_end, *spawnmon, *forcedfocusmon;
static DevPair* devpairs,* spawndev;
static Window root, wmcheckwin, lowest_barwin = None, highest_barwin = None, floating_stack_helper = None;
static XIGrabModifiers anymodifier[] = { { XIAnyModifier, 0 } };
static unsigned char hcmask[XIMaskLen(XI_HierarchyChanged)] = {0};
static unsigned char ptrmask[XIMaskLen(XI_LASTEVENT)] = {0};
static unsigned char kbdmask[XIMaskLen(XI_LASTEVENT)] = {0};
static XIEventMask hcevm = { .deviceid = XIAllDevices, .mask_len = sizeof(hcmask), .mask = hcmask };
static XIEventMask ptrevm = { .deviceid = -1, .mask_len = sizeof(ptrmask), .mask = ptrmask };
static XIEventMask kbdevm = { .deviceid = -1, .mask_len = sizeof(kbdmask), .mask = kbdmask };
static Device deviceslots[MAXDEVICES] = {0};

static int forcing_focus = 0;
int log_fd = 2;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
    const char *class, *instance;
    unsigned int i;
    const Rule *r;
    Monitor *m;
    XClassHint ch = { NULL, NULL };

    /* rule matching */
    c->isfloating = 0;
    c->tags = 0;
    XGetClassHint(dpy, c->win, &ch);
    class    = ch.res_class ? ch.res_class : broken;
    instance = ch.res_name  ? ch.res_name  : broken;

    for (i = 0; i < LENGTH(rules); i++) {
        r = &rules[i];
        if ((!r->title || strstr(c->name, r->title))
        && (!r->class || strstr(class, r->class))
        && (!r->instance || strstr(instance, r->instance)))
        {
            c->isfloating = r->isfloating;
            c->tags |= r->tags;
            for (m = mons; m && m->num != r->monitor; m = m->next);
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

int
applysizehints(Client *c, int * __restrict x, int * __restrict y, int * __restrict w, int * __restrict h, int interact)
{
    int baseismin;
    Monitor *m = c->mon;

    /* set minimum possible */
    *w = MAX(1, *w);
    *h = MAX(1, *h);
    if (interact) {
        if (*x > sw)
            *x = sw - WIDTH(c);
        if (*y > sh)
            *y = sh - HEIGHT(c);
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
    if (*h < bh)
        *h = bh;
    if (*w < bh)
        *w = bh;
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

void
arrange(Monitor* m)
{
    DBG("+arrange\n");

    if (m)
        showhide(m->stack);
    else for (m = mons; m; m = m->next)
        showhide(m->stack);
    
    if (m)
    {
        arrangemon(m);
        restack(m);
    }
    else for (m = mons; m; m = m->next)
        arrangemon(m);
}

void
arrangemon(Monitor *m)
{
    DBG("+arrangemon\n");
    strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof(m->ltsymbol) - 1);
    if (m->lt[m->sellt]->arrange)
        m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
    c->next = c->mon->clients;
    c->mon->clients = c;
}

void
append(Client *c)
{
    Client* tc;
    if(!c->mon->clients)
    {
        c->mon->clients = c;
        return;
    }
    for (tc = c->mon->clients; tc->next; tc = tc->next);
    tc->next = c;
    c->next = NULL;
}

void
attachstack(Client *c)
{
    c->snext = c->mon->stack;
    c->mon->stack = c;
}

void
xi2buttonpress(void* ev)
{
    unsigned int i, x, click = ClkRootWin;
    XIDeviceEvent* e = ev;
    DevPair* dp = getdevpair(e->deviceid);
    Arg arg = {0};
    Monitor* m;
    Client* c;
    
    /* focus monitor if necessary */
    if ((m = wintomon(dp, e->event)) && m != dp->selmon) {
        unfocus(dp, 1);
        setselmon(dp, m);
        focus(dp, NULL);
    }
    if (e->event == dp->selmon->barwin) {
        i = x = 0;
        do
            x += TEXTW(tags[i]);
        while (e->event_x >= x && ++i < LENGTH(tags));
        if (i < LENGTH(tags)) {
            click = ClkTagBar;
            arg.ui = 1 << i;
        } else if (e->event_x < x + TEXTW(dp->selmon->ltsymbol))
            click = ClkLtSymbol;
        else if (e->event_x > dp->selmon->ww - (int)TEXTW(stext))
            click = ClkStatusText;
        else
            click = ClkWinTitle;
    } else if ((c = wintoclient(e->event))) {
        focus(dp, c);
        if (dp->sel == c) {
            XIAllowEvents(dpy, dp->mptr->info.deviceid, XIReplayDevice, CurrentTime);
            click = ClkClientWin;
        }
    }

    dp->lastevent = e->time;
    dp->lastdetail = e->detail;

    for (i = 0; i < LENGTH(buttons); i++)
        if (click == buttons[i].click && buttons[i].func && buttons[i].button == e->detail
        && CLEANMASK(buttons[i].mask) == CLEANMASK(e->mods.effective))
            buttons[i].func(dp, click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
postmoveselmon(DevPair* dp, Client* c)
{
    Monitor* m;
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

void
xi2buttonrelease(void* ev)
{
    XIDeviceEvent* e = ev;
    DevPair* dp = getdevpair(e->deviceid);
    Client** pc;
    Motion* mm;

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
                XIUngrabDevice(dpy, dp->mptr->info.deviceid, CurrentTime);
                break;
        }
        postmoveselmon(dp, *pc);
        *pc = NULL;
    }
}

void
checkotherwm(void)
{
    xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XSync(dpy, False);
}

void
cleanup(void)
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

    for (m = mons; m; m = m->next)
        while (m->stack)
            unmanage(m->stack, 0);
    XIUngrabKeycode(dpy, XIAllMasterDevices, XIAnyKeycode, root, LENGTH(anymodifier), anymodifier);
    while (mons)
        cleanupmon(mons);
    for (i = 0; i < CurLast; i++)
        drw_cur_free(drw, cursor[i]);
    for (i = 0; i < LENGTH(colors); i++)
        free(scheme[i]);
    for (i = 0; i < LENGTH(ff_colors); i++)
        free(ff_scheme[i]);
    free(ff_scheme);
    free(scheme);
    XDestroyWindow(dpy, wmcheckwin);
    drw_free(drw);
    XSync(dpy, False);
    XISetFocus(dpy, XIAllMasterDevices, None, CurrentTime);
    XISetClientPointer(dpy, None, XIAllMasterDevices);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
insertmon(Monitor* at, Monitor* m)
{
    if(!at && mons) /* infront of mons */
    {
        mons->prev = m;
        m->next = mons;
        mons = m;
    }
    else if (at) /* at */
    {
        m->next = at->next;
        m->prev = at;
        
        if (at->next)
            at->next->prev = m;
        at->next = m;
    }
    else /* fresh */
        mons = m;
    
    if(!m->next)
        mons_end = m;
}

void
unlinkmon(Monitor* m)
{
    if(m->next)
        m->next->prev = m->prev;
    else
        mons_end = m->prev;
    
    if(m->prev)
        m->prev->next = m->next;
    else
        mons = m->next;
    
    m->next = NULL;
    m->prev = NULL;
}

void
cleanupmon(Monitor *m)
{
    unlinkmon(m);

    XUnmapWindow(dpy, m->barwin);
    XDestroyWindow(dpy, m->barwin);
    free(m);
}

void
clientmessage(XEvent *e)
{
    XClientMessageEvent *cme = &e->xclient;
    Client *c = wintoclient(cme->window);

    if (!c)
        return;
    if (cme->message_type == netatom[NetWMState]) {
        if (cme->data.l[1] == (long)netatom[NetWMFullscreen]
        || cme->data.l[2] == (long)netatom[NetWMFullscreen])
            setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
    } else if (cme->message_type == netatom[NetActiveWindow]) {
        if (!c->isurgent && !c->devices)
            seturgent(c, 1);
    }
}

void
configure(Client *c)
{
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = dpy;
    ce.event = c->win;
    ce.window = c->win;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->w;
    ce.height = c->h;
    ce.border_width = c->bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
    DevPair* dp;
    Monitor *m;
    Client *c;
    XConfigureEvent *ev = &e->xconfigure;
    int dirty;

    /* TODO: updategeom handling sucks, needs to be simplified */
    if (ev->window == root) {
        dirty = (sw != ev->width || sh != ev->height);
        sw = ev->width;
        sh = ev->height;
        if (updategeom(NULL) || dirty) {
            drw_resize(drw, sw, bh);
            updatebars();
            for (m = mons; m; m = m->next) {
                for (c = m->clients; c; c = c->next)
                    if (c->isfullscreen)
                        resizeclient(c, m->mx, m->my, m->mw, m->mh);
                XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
            }
            for (dp = devpairs; dp; dp = dp->next)
                focus(dp, NULL);
            arrange(NULL);
        }
    }
}

void
configurerequest(XEvent *e)
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

            for(m = mons; m; m = m->next) {
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
                XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
            }
        } else {
            DBG("+configurerequest configure 2 %lu %lu\n", c->win, ev->value_mask);
            configure(c);
        }
    } else {
        DBG("+configurerequest unmanaged window configure %lu\n", ev->window)
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }

    XSync(dpy, False);
}

Monitor *
createmon(void)
{
    Monitor *m;

    m = ecalloc(1, sizeof(Monitor));
    m->tagset[0] = m->tagset[1] = 1;
    m->mfact = mfact;
    m->nmaster = nmaster;
    m->showbar = showbar;
    m->topbar = topbar;
    m->rmaster = rmaster;
    m->lt[0] = &layouts[0];
    m->lt[1] = &layouts[1 % LENGTH(layouts)];
    strncpy(m->ltsymbol, layouts[0].symbol, sizeof(m->ltsymbol));
    return m;
}

void
expose(XEvent *e)
{
    Monitor *m;
    XExposeEvent *ev = &e->xexpose;

    if (ev->count == 0 && (m = anywintomon(ev->window)))
        drawbar(m);
}

void
destroynotify(XEvent *e)
{
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    DBG("+destroynotify %lu\n", ev->window);

    if ((c = wintoclient(ev->window)))
        unmanage(c, 1);
}

void
detach(Client *c)
{
    Client **tc;

    for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
    *tc = c->next;
    c->next = NULL;
}

void
detachstack(Client* c)
{
    Client **tc;

    for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
    *tc = c->snext;
}

Monitor *
dirtomon(DevPair* dp, int dir)
{
    int best_neg = 0;
    int best_pos = 0;
    int middle = dp->selmon->mx + (dp->selmon->mw / 2);
    int delta;
    Monitor* m = NULL;
    Monitor* m_left = NULL;
    Monitor* m_right = NULL;

    // Left
    if (dir > 0)
    {
        // looking for the closest positive number
        for(m = mons; m; m = m->next)
        {
            if(m == dp->selmon)
                continue;
            delta = middle - (m->mx + (m->mw / 2));
            // look for closest left screen (closest to 0 positive number)
            if((!best_pos && delta > 0) || (best_pos && delta > 0 && best_pos > delta))
            {
                best_pos = delta;
                m_left = m;
            }
            // look for furthest right screen (furthest away from 0 negative number)
            if((!best_neg && delta < 0) || (best_neg && delta < 0 && best_neg > delta))
            {
                best_neg = delta;
                m_right = m;
            }
        }
        return m_left ? m_left : m_right;
    }

    // Right
    // looking for the closest positive number
    for(m = mons; m; m = m->next)
    {
        if(m == dp->selmon)
            continue;
        delta = middle - (m->mx + (m->mw / 2));
        // look for the left screen (closest to 0 negative number)
        if((!best_neg && delta < 0) || (best_neg && delta < 0 && best_neg < delta))
        {
            best_neg = delta;
            m_right = m;
        }
        // look for the right screen (furthest away from 0 positive number)
        if((!best_pos && delta > 0) || (best_pos && delta > 0 && best_pos < delta))
        {
            best_pos = delta;
            m_left = m;
        }
    }
    return m_right ? m_right : m_left;
}

void
drawbar(Monitor* m)
{
    char focus_text[512];
    int fti = 0, rfti = 0;
    int x, w, sw = 0;
    int boxs = drw->fonts->h / 9;
    int boxw = drw->fonts->h / 6 + 2;
    unsigned int i, occ = 0, urg = 0, selt = 0;
    Client *c;
    DevPair* dp;
    Clr ** cur_scheme;

    if(m == forcedfocusmon)
        cur_scheme = ff_scheme;
    else
        cur_scheme = scheme;

    if (!m->showbar)
        return;
    
    /* draw status first so it can be overdrawn by tags later */
    if (m->devices) {
        drw_setscheme(drw, cur_scheme[SchemeNorm]);
        sw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
        drw_text(drw, m->ww - sw, 0, sw, bh, 0, stext, 0);
    }

    for (c = m->clients; c; c = c->next) {
        occ |= c->tags;
        if (c->isurgent)
            urg |= c->tags;
    }
    x = 0;
    for (i = 0; i < LENGTH(tags); i++) {
        w = TEXTW(tags[i]);
        drw_setscheme(drw, cur_scheme[m->tagset[m->seltags] & 1 << i ? CLAMP(SchemeNorm + m->devices, SchemeNorm, SchemeSel3) : SchemeNorm]);
        drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
        if (occ & 1 << i) {
            for (dp = m->devstack; dp && !selt; dp = dp->mnext) {
                selt |= dp->sel ? dp->sel->tags : 0;
            }
            drw_rect(drw, x + boxs, boxs, boxw, boxw,
                m->devices && selt & 1 << i,
                urg & 1 << i);
        }
        x += w;
    }
    w = TEXTW(m->ltsymbol);
    drw_setscheme(drw, cur_scheme[SchemeNorm]);
    x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

    /*
     * 1  2  3  4  5  6  7  8  9  []=  [dev01,dev02] user@vm01: ~/Downloads | [dev03] user@vm01: ~
    */
    if ((w = m->ww - sw - x) > bh) {
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
            drw_setscheme(drw, cur_scheme[CLAMP(SchemeNorm + m->devices, SchemeNorm, SchemeSel3)]);
            drw_text(drw, x, 0, w, bh, lrpad / 2, focus_text, 0);
        }
        else {
            drw_setscheme(drw, cur_scheme[SchemeNorm]);
            drw_rect(drw, x, 0, w, bh, 1, 1);
            drw_text(drw, x, 0, w, bh, lrpad / 2, "", 0);
        }
    }
    
    drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void)
{
    Monitor *m;

    for (m = mons; m; m = m->next)
        drawbar(m);
}

void
xi2enter(void *ev)
{
    Client* c;
    Monitor* m;
    XIEnterEvent* e = ev;
    DevPair* dp = getdevpair(e->deviceid);

    DBG("+xi2enter %lu\n", e->event);
    if ((e->mode != XINotifyNormal || e->detail == XINotifyInferior) && e->event != root)
        return;

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

void
focus(DevPair* dp, Client *c)
{
    XWindowChanges wc;
    
    DBG("+focus %lu -> %lu\n", dp->sel ? dp->sel->win : 0, c ? c->win : 0);

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

        if(c->isfloating)
        {
            wc.stack_mode = Above;
            wc.sibling = floating_stack_helper;
            XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
        }
    } else {
        XISetFocus(dpy, dp->mkbd->info.deviceid, root, CurrentTime);
        XISetClientPointer(dpy, None, dp->mptr->info.deviceid);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }

    setsel(dp, c);
	drawbars();
}

void
xi2focusin(void *ev)
{
    XIFocusInEvent* e = ev;
    DevPair* dp = getdevpair(e->deviceid);
    
    if (dp->sel && e->event != dp->sel->win)
        setfocus(dp, dp->sel);
}

void
focusmon(DevPair* dp, const Arg *arg)
{
    Monitor* m;

    if (!mons->next)
        return;
    if ((m = dirtomon(dp, arg->i)) == dp->selmon)
        return;
    
    unfocus(dp, 0);
    setselmon(dp, m);
    focus(dp, NULL);
}

void
swapmon(DevPair* dp, const Arg *arg)
{
    Monitor* tar;

    if (!mons->next)
        return;
    if ((tar = dirtomon(dp, arg->i)) == dp->selmon)
        return;
    
    Monitor* cur = dp->selmon;

    unfocus(dp, 0);

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

    updatebarpos(cur);
    updatebarpos(tar);

    XMoveResizeWindow(dpy, cur->barwin, cur->wx, cur->by, cur->ww, bh);
    XMoveResizeWindow(dpy, tar->barwin, tar->wx, tar->by, tar->ww, bh);
    
    if(forcedfocusmon)
        forcedfocusmon = tar;
    setselmon(dp, tar);
    focus(dp, NULL);
    arrange(tar);
    arrange(cur);
}

void
focusstack(DevPair* dp, const Arg *arg)
{
    Client *c = NULL, *i;

    if (!dp || !dp->sel || !dp->selmon || (dp->sel->isfullscreen && lockfullscreen))
        return;

    if (arg->i > 0) {
        for (c = dp->sel->next; c && !ISVISIBLE(c); c = c->next);
        if (!c)
            for (c = dp->selmon->clients; c && !ISVISIBLE(c); c = c->next);
    } else {
        for (i = dp->selmon->clients; i != dp->sel; i = i->next)
            if (ISVISIBLE(i))
                c = i;
        if (!c)
            for (; i; i = i->next)
                if (ISVISIBLE(i))
                    c = i;
    }

    if (c) {
        focus(dp, c);
        restack(dp->selmon);
    }
}

void
cyclestack(DevPair* dp, const Arg * arg)
{
    Client* c;

    /* TODO: hide fullscreen apps instead and show something somewhere
     * so user knows if theres a fullscreen app in the background
    */

    if (!dp || !dp->sel || !dp->selmon || (dp->sel->isfullscreen && lockfullscreen))
        return;

    focusstack(dp, arg);

    if (arg->i > 0)
    {
        c = dp->selmon->clients;
        detach(c);
        append(c);
    }
    else
    {
        for (c = dp->selmon->clients; c->next; c = c->next);
        detach(c);
        attach(c);
    }

    if (dp->sel) {
        focus(dp, dp->sel);
        arrange(dp->selmon);
    }
}

Atom
getatomprop(Client *c, Atom prop)
{
    int di;
    unsigned long dl;
    unsigned char *p = NULL;
    Atom da, atom = None;

    if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof(atom), False, XA_ATOM,
        &da, &di, &dl, &dl, &p) == Success && p) {
        atom = *(Atom *)p;
        XFree(p);
    }
    return atom;
}

int
getrootptr(DevPair* dp, int * __restrict x, int * __restrict y)
{
    double dx, dy;
    int ret = XIQueryPointer(dpy, dp->mptr->info.deviceid, root, &(Window){0},
        &(Window){0}, &dx, &dy, &(double){0.0}, &(double){0.0}, &(XIButtonState){0},
        &(XIModifierState){0}, &(XIGroupState){0}
    );
    *x = dx;
    *y = dy;
    return ret;
}

long
getstate(Window w)
{
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;

    if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
        &real, &format, &n, &extra, (unsigned char **)&p) != Success)
        return -1;
    if (n != 0)
        result = *p;
    XFree(p);
    return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
    char **list = NULL;
    int n;
    XTextProperty name;

    if (!text || size == 0)
        return 0;
    text[0] = '\0';
    if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
        return 0;
    if (name.encoding == XA_STRING)
        strncpy(text, (char *)name.value, size - 1);
    else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
        strncpy(text, *list, size - 1);
        XFreeStringList(list);
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return 1;
}

void
grabbuttons(Device* mptr, Client *c, int focused)
{
    ptrevm.deviceid = XIAllDevices;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Enter);
    XISetMask(ptrmask, XI_FocusIn);
    XISelectEvents(dpy, c->win, &ptrevm, 1);

    ptrevm.deviceid = mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Motion);
    XISetMask(ptrmask, XI_ButtonPress);
    XISetMask(ptrmask, XI_ButtonRelease);

    updatenumlockmask();
    {
        unsigned int i;
        XIUngrabButton(dpy, ptrevm.deviceid, XIAnyButton, c->win, LENGTH(anymodifier), anymodifier);
        if(!c->grabbed)
            return;
        if (!focused)
            XIGrabButton(dpy, ptrevm.deviceid, XIAnyButton, c->win, None, XIGrabModeAsync,
                XIGrabModeAsync, False, &ptrevm, LENGTH(anymodifier), anymodifier);
        for (i = 0; i < LENGTH(buttons); i++) {
            if (buttons[i].click != ClkClientWin)
                continue;
            XIGrabModifiers modifiers[] = {
                { buttons[i].mask, 0 },
                { buttons[i].mask|LockMask, 0 },
                { buttons[i].mask|numlockmask, 0 },
                { buttons[i].mask|numlockmask|LockMask, 0 }
            };
            XIGrabButton(dpy, ptrevm.deviceid, buttons[i].button, c->win, None, XIGrabModeAsync,
                XIGrabModeAsync, False, &ptrevm, LENGTH(modifiers), modifiers);
        }
    }
}

void
grabdevicekeys(Device* mkbd)
{
    memset(kbdmask, 0, sizeof(kbdmask));
    XISetMask(kbdmask, XI_KeyPress);
    kbdevm.deviceid = mkbd->info.deviceid;

    updatenumlockmask();
    {
        unsigned int i;
        KeyCode code;

        XIUngrabKeycode(dpy, kbdevm.deviceid, XIAnyKeycode, root, LENGTH(anymodifier), anymodifier);
        for (i = 0; i < LENGTH(keys); i++) {
            if (!(code = XKeysymToKeycode(dpy, keys[i].keysym)))
                continue;
            XIGrabModifiers modifiers[] = {
                { keys[i].mod, 0 },
                { keys[i].mod|LockMask, 0 },
                { keys[i].mod|numlockmask, 0 },
                { keys[i].mod|numlockmask|LockMask, 0 }
            };
            XIGrabKeycode(dpy, kbdevm.deviceid, code, root,
                XIGrabModeAsync, XIGrabModeAsync, True, &kbdevm, LENGTH(modifiers), modifiers);
        }
    }
}

void
grabdevicebuttons(Device* mptr)
{
    ptrevm.deviceid = mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISelectEvents(dpy, root, &ptrevm, 1);
}

void
incnmaster(DevPair* dp, const Arg *arg)
{
    dp->selmon->nmaster = MAX(dp->selmon->nmaster + arg->i, 0);
    arrange(dp->selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
    while (n--)
        if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
        && unique[n].width == info->width && unique[n].height == info->height)
            return 0;
    return 1;
}
#endif /* XINERAMA */

void
xi2keypress(void *ev)
{
    XIDeviceEvent* e = ev;
    DevPair* dp = getdevpair(e->deviceid);
    unsigned int i;
    KeySym keysym;
    
    keysym = XkbKeycodeToKeysym(dpy, (KeyCode)e->detail, 0, 0);
    dp->lastevent = e->time;
    for (i = 0; i < LENGTH(keys); i++) {
        if (keysym == keys[i].keysym
        && CLEANMASK(keys[i].mod) == CLEANMASK(e->mods.effective)
        && keys[i].func)
            keys[i].func(dp, &(keys[i].arg));
    }
}

void
xi2hierarchychanged(void* ev)
{
    DBG("+ xi2hierarchychanged\n");
    int i, x, y, idx;
    DevPair *dp;
    Device **pd;
    Device *d;
    Monitor *m;
    XIHierarchyEvent* e = ev;

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
    for (dp = devpairs; dp; dp = dp->next) {
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

void
killclient(DevPair* dp, const Arg *arg __attribute__((unused)))
{
    if (!dp->sel)
        return;
    if (!sendevent(dp->sel->win, wmatom[WMDelete])) {
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, dp->sel->win);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
}

void
manage(Window w, XWindowAttributes *wa)
{
    Client *c, *t = NULL;
    Window trans = None;
    XWindowChanges wc;
    DevPair* dp;
    Clr** cur_scheme;

    DBG("+manage %lu\n", w);

    if(forcedfocusmon)
        cur_scheme = ff_scheme;
    else
        cur_scheme = scheme;
    
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
    if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
        c->mon = t->mon;
        c->tags = t->tags;
    } else if (spawndev && spawndev->selmon) {
        c->mon = spawnmon ? spawnmon : spawndev->selmon;
        spawnmon = NULL;
        applyrules(c);
    } else {
        die("could not find monitor for client\n");
    }

    if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
        c->x = c->mon->wx + c->mon->ww - WIDTH(c);
    if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
        c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
    c->x = MAX(c->x, c->mon->wx);
    c->y = MAX(c->y, c->mon->wy);
    c->bw = borderpx;

    wc.border_width = c->bw;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, cur_scheme[SchemeNorm][ColBorder].pixel);
    configure(c); /* propagates border_width, if size doesn't change */
    updatewindowtype(c);
    updatesizehints(c);
    updatewmhints(c);
    XSelectInput(dpy, w, PropertyChangeMask|StructureNotifyMask);

    for (dp = devpairs; dp; dp = dp->next)
        grabbuttons(dp->mptr, c, 0);

    if (!c->isfloating) {
        c->isfloating = c->oldstate = trans != None || c->isfixed;
    }

    attach(c);
    attachstack(c);

    XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *) &(c->win), 1);
    //XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this, should be fixed with dirty_resize */
    setclientstate(c, NormalState);
    XMapWindow(dpy, c->win);

    XSync(dpy, False);

    // check if window still attached
    c->ismanaged = 1;
    if(!wintoclient(w))
    {
        DBG("-manage closed %lu %d %d\n", w, c->isfloating, c->isfullscreen);
        free(c);
        return;
    }

    if(!c->isfloating && !c->isfullscreen)
        arrange(c->mon);
    else
        restack(c->mon);

    if(!c->isfloating)
        focus(spawndev, NULL);

    DBG("-manage %lu %d %d\n", w, c->isfloating, c->isfullscreen);
}

void
grabkeys(void)
{
    DevPair* dp;
    for (dp = devpairs; dp; dp = dp->next)
        grabdevicekeys(dp->mkbd);
}

void
mappingnotify(XEvent *e)
{
    XMappingEvent *ev = &e->xmapping;

    DBG("+mappingnotify %lu %d\n", ev->window, ev->request);

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard) 
        grabkeys();
}

void
maprequest(XEvent *e)
{
    static XWindowAttributes wa = {0};
    XMapRequestEvent *ev = &e->xmaprequest;

    DBG("+maprequest %lu->%lu (%d, %d)\n", ev->parent, ev->window, ev->type, ev->send_event);

    if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect) {
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

void
monocle(Monitor *m)
{
    unsigned int n = 0;
    Client *c;

    for (c = m->clients; c; c = c->next)
        if (ISVISIBLE(c))
            n++;
    if (n > 0) /* override layout symbol */
        snprintf(m->ltsymbol, sizeof(m->ltsymbol), "[%d]", n);
    for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
        resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
tile(Monitor *m)
{
    int h, r, mw, my, ty, i, n, oe = 1, ie = 1;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);

    if (n == 0)
        return;

    if (n > m->nmaster)
        mw = m->nmaster ? (m->ww + gappx * ie) * (m->rmaster ? 1.0 - m->mfact : m->mfact) : 0;
    else
        mw = m->ww - 2 * gappx * oe + gappx * ie;
    
    for (i = 0, my = ty = gappx * oe, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < m->nmaster) {
            r = MIN(n, m->nmaster) - i;
            h = (m->wh - my - gappx * oe - gappx * ie * (r - 1)) / r;
            resize(c, m->rmaster ? m->wx + m->ww - mw : m->wx + gappx * oe, m->wy + my, mw - (2 * c->bw) - gappx * ie, h - (2 * c->bw), 0);
            my += HEIGHT(c) + gappx * ie;
        } else {
            r = n - i;
            h = (m->wh - ty - gappx * oe - gappx * ie * (r - 1)) / r;
            resize(c, m->rmaster ? m->wx : m->wx + mw + gappx * oe, m->wy + ty, m->ww - mw - (2 * c->bw) - 2 * gappx * oe, h - (2 * c->bw), 0);
            ty += HEIGHT(c) + gappx * ie;
        }
    }
}

void
centeredmaster(Monitor *m)
{
    int h, mw, mx, my, oty, ety, tw;
    int i, n, nm;
    Client *c;

    /* count number of clients in the selected monitor */
    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
    if (n == 0)
        return;

    /* initialize areas */
    mw = m->ww;
    mx = 0;
    my = 0;
    tw = mw;

    /* keep things centered if possible */
    nm = MIN(m->nmaster, MAX(n - 2, 0));

    if (n > nm) {
        /* go mfact box in the center if more than nmaster clients */
        mw = nm ? m->ww * m->mfact : 0;
        tw = m->ww - mw;

        if (n - nm > 1) {
            /* only one client */
            mx = (m->ww - mw) / 2;
            tw = (m->ww - mw) / 2;
        }
    }

    oty = 0;
    ety = 0;
    for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if (i < nm) {
        /* nmaster clients are stacked vertically, in the center
         * of the screen */
        h = (m->wh - my) / (MIN(n, nm) - i);
        resize(c, m->wx + mx, m->wy + my, mw - (2*c->bw),
               h - (2*c->bw), 0);
        my += HEIGHT(c);
    } else {
        /* stack clients are stacked vertically */
        if ((i - nm) % 2 ) {
            h = (m->wh - ety) / ( (1 + n - i) / 2);
            resize(c, m->wx, m->wy + ety, tw - (2*c->bw),
                   h - (2*c->bw), 0);
            ety += HEIGHT(c);
        } else {
            h = (m->wh - oty) / ((1 + n - i) / 2);
            resize(c, m->wx + mx + mw, m->wy + oty,
                   tw - (2*c->bw), h - (2*c->bw), 0);
            oty += HEIGHT(c);
        }
    }
}

void
xi2motion(void *ev)
{
    XIDeviceEvent *e = ev;
    Monitor* m = NULL;
    Client* c;
    Window sw;
    DevPair* dp = getdevpair(e->deviceid);

    if(forcing_focus)
        return;
        
    if (!e->child) {
        sw = e->event;
        for(m = mons; m; m = m->next) {
            if (m->barwin == sw) {
                sw = root;
                break;
            }
        }
    } else {
        sw = e->child;
    }
    
    if (sw == root && !dp->move.c && !dp->resize.c) {
        if (!m)
            m = recttomon(dp, e->root_x, e->root_y, 1, 1);
        if (m && m != dp->selmon) {
            setselmon(dp, m);
            focus(dp, NULL);
        }
    }

    if ((c = dp->move.c) && dp->resize.time < dp->move.time) {
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
        if (!c->isfloating && dp->selmon->lt[dp->selmon->sellt]->arrange
            && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
            togglefloating(dp, NULL);
        if (!dp->selmon->lt[dp->selmon->sellt]->arrange || c->isfloating) {
            if(!c->isfullscreen)
                resize(c, nx, ny, c->w, c->h, 1);
            postmoveselmon(dp, c);
        }
    } else if ((c = dp->resize.c) && dp->resize.time > dp->move.time) {
        int nw, nh;
        if ((e->time - dp->resize.time) <= (1000 / 250))
            return;
        dp->resize.time = e->time;
        nw = MAX(e->root_x - dp->resize.ox - 2 * c->bw + 1, 1);
        nh = MAX(e->root_y - dp->resize.oy - 2 * c->bw + 1, 1);
        if (c->mon->wx + nw >= dp->selmon->wx && c->mon->wx + nw <= dp->selmon->wx + dp->selmon->ww
        && c->mon->wy + nh >= dp->selmon->wy && c->mon->wy + nh <= dp->selmon->wy + dp->selmon->wh)
        {
            if (!c->isfloating && dp->selmon->lt[dp->selmon->sellt]->arrange
            && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
                togglefloating(dp, NULL);
        }
        if (!dp->selmon->lt[dp->selmon->sellt]->arrange || c->isfloating) {
            if(!c->isfullscreen)
                resize(c, c->x, c->y, nw, nh, 1);
            postmoveselmon(dp, c);
        }
    }

}

void
movemouse(DevPair* dp, __attribute__((unused)) const Arg * arg)
{
    Client* c;
    if (!(c = dp->move.c) && !(c = dp->sel))
        return;
    
    if(dp->resize.c)
    {
        XIUngrabDevice(dpy, dp->mptr->info.deviceid, CurrentTime);
    }

    ptrevm.deviceid = dp->mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Motion);
    XISetMask(ptrmask, XI_ButtonPress);
    XISetMask(ptrmask, XI_ButtonRelease);

    if (XIGrabDevice(
        dpy,
        dp->mptr->info.deviceid,
        root,
        CurrentTime,
        cursor[CurMove]->cursor,
        XIGrabModeAsync,
        XIGrabModeAsync,
        False,
        &ptrevm) != GrabSuccess)
    {
        return;
    }
    
    if (!dp->move.c)
    {
        dp->move.c = dp->sel;
    }

    dp->move.time = dp->lastevent;
    dp->move.detail = dp->lastdetail;
    dp->move.ox = c->x;
    dp->move.oy = c->y;

    getrootptr(dp, &dp->move.x, &dp->move.y);
}

void
resizemouse(DevPair* dp, __attribute__((unused)) const Arg * arg)
{
    Client *c;
    if (!(c = dp->resize.c) && !(c = dp->sel))
        return;

    /* no support resizing fullscreen windows by mouse */
    if (c->isfullscreen) 
        return;
    
    if(dp->move.c)
    {
        XIUngrabDevice(dpy, dp->mptr->info.deviceid, CurrentTime);
    }

    ptrevm.deviceid = dp->mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Motion);
    XISetMask(ptrmask, XI_ButtonPress);
    XISetMask(ptrmask, XI_ButtonRelease);

    if (XIGrabDevice(
        dpy,
        dp->mptr->info.deviceid,
        root,
        CurrentTime,
        cursor[CurResize]->cursor,
        XIGrabModeAsync,
        XIGrabModeAsync,
        False,
        &ptrevm) != GrabSuccess)
    {
        return;
    }
    
    if (!dp->resize.c)
    {
        dp->resize.c = dp->sel;
    }

    dp->resize.time = dp->lastevent;
    dp->resize.detail = dp->lastdetail;
    dp->resize.ox = c->x;
    dp->resize.oy = c->y;
    dp->resize.x = c->w + c->bw - 1;
    dp->resize.x = c->h + c->bw - 1;
}

Client *
nexttiled(Client *c)
{
    for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
    return c;
}

void
pop(DevPair* dp, Client* c)
{
    detach(c);
    attach(c);
    focus(dp, c);
    arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if ((ev->window == root) && (ev->atom == XA_WM_NAME))
        updatestatus();
    else if (ev->state == PropertyDelete)
        return; /* ignore */
    else if ((c = wintoclient(ev->window))) {
        DBG("+propertynotify %lu %lu\n", ev->window, ev->atom);
        switch(ev->atom) {
        default: break;
        case XA_WM_TRANSIENT_FOR:
            if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
                (c->isfloating = (wintoclient(trans)) != NULL))
                arrange(c->mon);
            break;
        case XA_WM_NORMAL_HINTS:
            c->hintsvalid = 0;
            break;
        case XA_WM_HINTS:
            updatewmhints(c);
            drawbars();
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
            updatetitle(c);
            if (c->devices)
                drawbar(c->mon);
        }
        if (ev->atom == netatom[NetWMWindowType])
            updatewindowtype(c);
    }
}

void
quit(__attribute__((unused)) DevPair * dp, __attribute__((unused)) const Arg * arg)
{
    running = 0;
}

Monitor *
recttomon(DevPair* dp, int x, int y, int w, int h)
{
    Monitor *m, *r = dp->selmon;
    int a, area = 0;

    for (m = mons; m; m = m->next)
        if ((a = INTERSECT(x, y, w, h, m)) > area) {
            area = a;
            r = m;
        }
    return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
    DBG("+resize %lu %d, %d, %d, %d\n", c->win, x, y, w, h);
    if (c->dirty_resize || applysizehints(c, &x, &y, &w, &h, interact))
    {
        c->dirty_resize = False;
        resizeclient(c, x, y, w, h);
    }
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
    XWindowChanges wc;

    c->oldx = c->x; c->x = wc.x = x;
    c->oldy = c->y; c->y = wc.y = y;
    c->oldw = c->w; c->w = wc.width = w;
    c->oldh = c->h; c->h = wc.height = h;
    wc.border_width = c->bw;
    XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
    configure(c);
    XSync(dpy, False);
}

void
restack(Monitor* m)
{
    Client* c = NULL;
    XWindowChanges wc;
    XEvent ev;
    DevPair* dp;
    
    DBG("+restack\n");

    for (dp = devpairs; dp; dp = dp->next)
    {
        if (dp->selmon == m)
        {
            // make sure focused floating windows stay on top
            if(dp->sel && dp->sel->mon == m)
            {
                c = dp->sel;
                /* TODO: only need to do this once per unique client */
                if (c->isfloating || !m->lt[m->sellt]->arrange)
                {
                    wc.stack_mode = Above;
                    wc.sibling = floating_stack_helper;
                    XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
                    DBG("    restack %lu float Above\n", c->win);
                }
            }

            // make sure tiled or otherwise managed windows stay below
            if (m->lt[m->sellt]->arrange)
            {
                wc.stack_mode = Below;
                wc.sibling = lowest_barwin;
                for (c = m->stack; c; c = c->snext)
                {
                    if (!c->isfloating && ISVISIBLE(c))
                    {
                        DBG("    restack %lu lowest barwin Below\n", c->win);
                        XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
                        wc.sibling = c->win;
                    }
                }
            }
        }
    }

    XSync(dpy, False);
    xi2handler[XI_Enter] = NULL;
    while (XCheckTypedEvent(dpy, GenericEvent, &ev)) {
        if (legacyhandler[ev.type])
            legacyhandler[ev.type](&ev); /* call handler */
    }
    xi2handler[XI_Enter] = xi2enter;
}

#ifdef DEBUG
__attribute__((unused)) void
updatedebuginfo(void)
{
    DevPair* dp;
    Monitor* m;
    Client* c;
    int mn = 0, cn, i;
    for (m = mons; m; m = m->next) {
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

void
run(void)
{
    XEvent ev;
    /* main event loop */
    XSync(dpy, False);
    while (running && !XNextEvent(dpy, &ev)) {
        if (legacyhandler[ev.type])
            legacyhandler[ev.type](&ev); /* call handler */
    }
}

void
scan(void)
{
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;
    
    if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(dpy, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
                continue;
            if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState) {
                manage(wins[i], &wa);
            }
        }
        for (i = 0; i < num; i++) { /* now the transients */
            if (!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if (XGetTransientForHint(dpy, wins[i], &d1)
            && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)) {
                manage(wins[i], &wa);
            }
        }
        if (wins)
            XFree(wins);
    }
}

void
sendmon(DevPair* dp, Client* c, Monitor* m, int refocus)
{
    Monitor* prev_m = c->mon;
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
        resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    else if(dp->move.c != c && dp->resize.c != c && c->isfloating)
        resizeclient(c, m->mx, m->my, c->w, c->h);
}

void
setclientstate(Client *c, long state)
{
    long data[] = { state, None };
    XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
        PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto)
{
    int n;
    Atom *protocols;
    int exists = 0;
    XEvent ev;

    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
        while (!exists && n--)
            exists = protocols[n] == proto;
        XFree(protocols);
    }
    if (exists) {
        char* name = XGetAtomName(dpy, proto);
        XFree(name);
        ev.type = ClientMessage;
        ev.xclient.window = w;
        ev.xclient.message_type = wmatom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = proto;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, w, False, NoEventMask, &ev);
    }
    return exists;
}

void
setfocus(DevPair* dp, Client* c)
{
    if (!c->neverfocus) {
        XISetFocus(dpy, dp->mkbd->info.deviceid, c->win, CurrentTime);
        XISetClientPointer(dpy, c->win, dp->mptr->info.deviceid);
        XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &(c->win), 1);
    }
    sendevent(c->win, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
    XWindowChanges wc;

    if (fullscreen && !c->isfullscreen)
    {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
        c->isfullscreen = 1;
        c->oldstate = c->isfloating;
        c->oldbw = c->bw;
        c->bw = 0;
        c->isfloating = 1;
        resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
        wc.stack_mode = Above;
        wc.sibling = floating_stack_helper;
        XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
    }
    else if (!fullscreen && c->isfullscreen)
    {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char*)0, 0);
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
            arrange(c->mon);
        }
    }
}

void
setlayout(DevPair* dp, const Arg *arg)
{
    if (!dp->selmon)
        return;
    if (!arg || !arg->v || arg->v != dp->selmon->lt[dp->selmon->sellt])
        dp->selmon->sellt ^= 1;
    if (arg && arg->v)
        dp->selmon->lt[dp->selmon->sellt] = (Layout *)arg->v;

    strncpy(dp->selmon->ltsymbol, dp->selmon->lt[dp->selmon->sellt]->symbol, sizeof(dp->selmon->ltsymbol) - 1);
    if (dp->sel)
        arrange(dp->selmon);
    else
        drawbar(dp->selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(DevPair* dp, const Arg *arg)
{
    float f;

    if (!dp->selmon || !arg || !dp->selmon->lt[dp->selmon->sellt]->arrange)
        return;
    f = arg->f < 1.0 ? arg->f + dp->selmon->mfact : arg->f - 1.0;
    if (f < 0.1 || f > 0.9)
        return;
    dp->selmon->mfact = f;
    arrange(dp->selmon);
}

void
format_client_prefix_name(Client* c)
{
    DevPair* dp;
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

void
setsel(DevPair* dp, Client* c)
{
    DevPair **tdp;
    DevPair* ndp;
    Clr** cur_scheme;

    if (dp->sel == c)
        return;

    DBG("+setsel %lu -> %lu\n", dp->sel ? dp->sel->win : 0, c ? c->win : 0);

    if(forcedfocusmon)
        cur_scheme = ff_scheme;
    else
        cur_scheme = scheme;
    
    if (dp->sel) {
        dp->sel->devices--;
        XSetWindowBorder(dpy, dp->sel->win, cur_scheme[CLAMP(SchemeNorm + dp->sel->devices, SchemeNorm, SchemeSel3)][ColBorder].pixel);
        for (tdp = &dp->sel->devstack; *tdp && *tdp != dp; tdp = &(*tdp)->fnext);
        *tdp = dp->fnext;
        dp->fnext = NULL;
        format_client_prefix_name(dp->sel);
    }
    
    dp->sel = c;

    if (dp->sel) {
        dp->sel->devices++;
        XSetWindowBorder(dpy, dp->sel->win, cur_scheme[CLAMP(SchemeNorm + dp->sel->devices, SchemeNorm, SchemeSel3)][ColBorder].pixel);
        for (ndp = dp->sel->devstack; ndp && ndp->fnext; ndp = ndp->fnext);
        if (ndp)
            ndp->fnext = dp;
        else
            dp->sel->devstack = dp;
        format_client_prefix_name(dp->sel);
    }
}

void
setselmon(DevPair* dp, Monitor* m)
{
    DevPair **tdp;
    DevPair* ndp;
    int cur_bar_offset;
    int tar_bar_offset;

    if(dp->selmon == m)
        return;

    if(forcedfocusmon && m != forcedfocusmon)
    {
        forcing_focus = 1;
        Monitor* tar = m;
        Monitor* tar2 = NULL;
        int x, y;

        // only 1 screen, no need...
        if (!mons->next)
            return;

        Monitor* cur = dp->selmon;
        int dir = (cur->mx + (cur->mw / 2)) - (tar->mx + (tar->mw / 2));
        tar2 = dirtomon(dp, -dir);
        
        if(getrootptr(dp, &x, &y) && recttomon(dp, x, y, 1, 1) != cur)
        {
            if(cur->showbar)
                if(cur->topbar)
                    cur_bar_offset = -bh;
                else
                    cur_bar_offset = bh;
            else
                cur_bar_offset = 0;
            
            if(tar->showbar)
                if(tar->topbar)
                    tar_bar_offset = -bh;
                else
                    tar_bar_offset = bh;
            else
                tar_bar_offset = 0;

            XIWarpPointer(dpy, dp->mptr->info.deviceid, None, None, 0, 0, 0, 0, cur->wx - tar->wx, (cur->wy + cur_bar_offset) - (tar->wy + tar_bar_offset));
        }

        forcedfocusmon = tar;

        XSync(dpy, False);
        
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
            XMoveResizeWindow(dpy, tar2->barwin, tar2->wx, tar2->by, tar2->ww, bh);
            arrange(tar2);
        }
        
        updatebarpos(tar);
        updatebarpos(cur);

        XMoveResizeWindow(dpy, cur->barwin, cur->wx, cur->by, cur->ww, bh);
        XMoveResizeWindow(dpy, tar->barwin, tar->wx, tar->by, tar->ww, bh);

        arrange(tar);
        arrange(cur);
        
        XSync(dpy, False);
        forcing_focus = 0;
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

    drawbars();
}

DevPair*
createdevpair(void)
{
    DevPair *last_dp;
    DevPair *dp;

    dp = ecalloc(1, sizeof(DevPair));
    for (last_dp = devpairs; last_dp && last_dp->next; last_dp = last_dp->next);
    if (last_dp)
        last_dp->next = dp;
    else
        devpairs = dp;
    return dp;
}

void
removedevpair(DevPair* dp)
{
    DevPair **pdp;

    setsel(dp, NULL);
    setselmon(dp, NULL);

    /* pop devpair from devpairs */
    for (pdp = &devpairs; *pdp && *pdp != dp; pdp = &(*pdp)->next);
    *pdp = dp->next;

    free(dp);
}

DevPair*
getdevpair(int deviceid)
{
    return deviceid ? deviceslots[deviceid].self : NULL;
}

void
updatedevpair(DevPair* dp)
{
    Monitor* m;
    Client* c;

    grabdevicekeys(dp->mkbd);
    grabdevicebuttons(dp->mptr);

    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            grabbuttons(dp->mptr, c, 0);
}

void
initdevices(void)
{
    uint32_t i, ndevices = 0;
    int32_t idx;
    XIDeviceInfo *devs;
    DevPair* dp;
    Device* d;
    Monitor* m;
    int x, y;

    devs = XIQueryDevice(dpy, XIAllDevices, (int*)&ndevices);
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
    for (dp = devpairs; dp; dp = dp->next) {
        if(!getrootptr(dp, &x, &y))
            continue;
        if(!(m = recttomon(dp, x, y, 1, 1)))
            continue;
        setselmon(dp, m);
        updatedevpair(dp);
    }
    XIFreeDeviceInfo(devs);
}

void
seturgent(Client *c, int urg)
{
    XWMHints *wmh;

    c->isurgent = urg;
    if (!(wmh = XGetWMHints(dpy, c->win)))
        return;
    wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
    XSetWMHints(dpy, c->win, wmh);
    XFree(wmh);
}

void
showhide(Client *c)
{
    if (!c)
        return;

    if (ISVISIBLE(c))
    {
        /* show clients top down */
        DBG("+showhide %lu\n", c->win);
        XMoveWindow(dpy, c->win, c->x, c->y);
        if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating))
        {
            resize(c, c->x, c->y, c->w, c->h, 0);
            if (c->isfullscreen)
            {
                setfullscreen(c, 1);
            }
        }
        showhide(c->snext);
    }
    else
    {
        DBG("+showhide %lu\n", c->win);
        /* hide clients bottom up */
        showhide(c->snext);
        XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
    }
}

void
spawn(DevPair* dp, const Arg *arg)
{
    pid_t child;
    int status;
    struct sigaction sa;

    if(!dp->selmon)
        return;
    if (arg->v == dmenucmd)
        dmenumon[0] = '0' + dp->selmon->num;
    
    spawnmon = dp->selmon;
    spawndev = dp;

    if ((child = fork())) {
        waitpid(child, &status, 0);
    }
    else
    {
        if (dpy)
            close(ConnectionNumber(dpy));

        if (fork())
        {
            exit(0);
        }
        else
        {
            setsid();
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sa.sa_handler = SIG_DFL;
            sigaction(SIGCHLD, &sa, NULL);

            execvp(((char **)arg->v)[0], (char **)arg->v);
            die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
        }
    }
}

void
tag(DevPair* dp, const Arg *arg)
{
    if (!dp->selmon || !dp->sel)
        return;
    if (dp->sel && arg->ui & TAGMASK) {
        dp->sel->tags = arg->ui & TAGMASK;
        focus(dp, NULL);
        arrange(dp->selmon);
    }
}

void
tagmon(DevPair* dp, const Arg *arg)
{
    if (!dp->selmon || !dp->sel || !mons->next)
        return;

    sendmon(dp, dp->sel, dirtomon(dp, arg->i), 1);
}

void
togglebar(DevPair* dp, __attribute__((unused)) const Arg * arg)
{
    if(!dp->selmon)
        return;
    dp->selmon->showbar = !dp->selmon->showbar;
    updatebarpos(dp->selmon);
    XMoveResizeWindow(dpy, dp->selmon->barwin, dp->selmon->wx, dp->selmon->by, dp->selmon->ww, bh);
    arrange(dp->selmon);
    drawbar(dp->selmon);
}

void
togglefloating(DevPair* dp, const Arg * arg)
{
    if (!dp->selmon || !dp->sel)
        return;
    
    if (dp->sel->isfullscreen) /* no support for fullscreen windows */
        return;
    
    dp->sel->isfloating = !dp->sel->isfloating || dp->sel->isfixed;
    
    // invoked from key combination
    if(arg && (dp->move.c || dp->resize.c))
    {
        dp->move.c = NULL;
        dp->resize.c = NULL;
        XIUngrabDevice(dpy, dp->mptr->info.deviceid, CurrentTime);
    }
    
    if (dp->sel->isfloating)
    {
        resize(dp->sel, dp->sel->x, dp->sel->y,
            dp->sel->w, dp->sel->h, 0);
    }

    arrange(dp->selmon);
}

void
togglefullscreen(DevPair* dp, __attribute__((unused)) const Arg *arg)
{
    if (!dp->selmon || !dp->sel)
        return;
    setfullscreen(dp->sel, !dp->sel->isfullscreen);
}

void
toggleautoswapmon(DevPair* dp, __attribute__((unused)) const Arg * arg)
{
    Clr** cur_scheme;
    Monitor* fake_tar;
    int x, y;
    int cur_bar_offset, tar_bar_offset;

    if(!dp->selmon)
        return;
    
    if(forcedfocusmon)
    {
        cur_scheme = scheme;
        forcedfocusmon = NULL;
    }
    else
    {
        cur_scheme = ff_scheme;
        forcedfocusmon = dp->selmon;
    }

    if(dp->sel)
        XSetWindowBorder(dpy, dp->sel->win, cur_scheme[CLAMP(SchemeNorm + dp->sel->devices, SchemeNorm, SchemeSel3)][ColBorder].pixel);
    
    if(getrootptr(dp, &x, &y) && (fake_tar = recttomon(dp, x, y, 1, 1)) != dp->selmon)
    {
        if(dp->selmon->showbar)
            if(dp->selmon->topbar)
                cur_bar_offset = -bh;
            else
                cur_bar_offset = bh;
        else
            cur_bar_offset = 0;
        
        if(fake_tar->showbar)
            if(fake_tar->topbar)
                tar_bar_offset = -bh;
            else
                tar_bar_offset = bh;
        else
            tar_bar_offset = 0;

        XIWarpPointer(dpy, dp->mptr->info.deviceid, None, None, 0, 0, 0, 0, dp->selmon->wx - fake_tar->wx, (dp->selmon->wy + cur_bar_offset) - (fake_tar->wy + tar_bar_offset));
    }
    
    setselmon(dp, dp->selmon);
}

void
togglermaster(DevPair* dp, __attribute__((unused)) const Arg * arg)
{
    dp->selmon->rmaster = !dp->selmon->rmaster;
    dp->selmon->mfact = 1.0 - dp->selmon->mfact;
    if (dp->selmon->lt[dp->selmon->sellt]->arrange)
        arrange(dp->selmon);
}

void
togglemouse(DevPair* dp, __attribute__((unused)) const Arg * arg)
{
    if (!dp->selmon || !dp->sel)
        return;
    if (dp->sel->isfullscreen) /* no support for fullscreen windows */
        return;

    dp->sel->grabbed = !dp->sel->grabbed;
    grabbuttons(dp->mptr, dp->sel, 1);
}

void
toggletag(DevPair* dp, const Arg *arg)
{
    unsigned int newtags;

    if (!dp->sel || !dp->selmon)
        return;

    newtags = dp->sel->tags ^ (arg->ui & TAGMASK);
    if (newtags) {
        dp->sel->tags = newtags;
        focus(dp, NULL);
        arrange(dp->selmon);
    }
}

void
toggleview(DevPair* dp, const Arg *arg)
{
    if (!dp->selmon)
        return;

    unsigned int newtagset = dp->selmon->tagset[dp->selmon->seltags] ^ (arg->ui & TAGMASK);

    if (newtagset) {
        dp->selmon->tagset[dp->selmon->seltags] = newtagset;
        focus(dp, NULL);
        arrange(dp->selmon);
    }
}

void
view(DevPair* dp, const Arg *arg)
{
    if ((arg->ui & TAGMASK) == dp->selmon->tagset[dp->selmon->seltags])
        return;
    dp->selmon->seltags ^= 1; /* toggle sel tagset */
    if (arg->ui & TAGMASK)
        dp->selmon->tagset[dp->selmon->seltags] = arg->ui & TAGMASK;
    focus(dp, NULL);
    arrange(dp->selmon);
}

void
zoom(DevPair* dp, __attribute__((unused)) const Arg * arg)
{
    Client* c = dp->sel;

    if (!c || !dp->selmon ||
        !dp->selmon->lt[dp->selmon->sellt]->arrange ||
        (dp->sel && dp->sel->isfloating))
        return;
    if (c == nexttiled(dp->selmon->clients) && !(c = nexttiled(c->next)))
        return;
    pop(dp, c);
}

void
unfocus(DevPair* dp, int setfocus)
{
    XWindowChanges wc;
    DBG("+unfocus %lu <-> %lu\n", dp->sel ? dp->sel->win : 0, c ? c->win : 0);
    
    if (!dp || !dp->sel)
        return;

    grabbuttons(dp->mptr, dp->sel, 0);
    if(dp->sel->isfloating)
    {
        wc.stack_mode = Below;
        wc.sibling = floating_stack_helper;
        XConfigureWindow(dpy, dp->sel->win, CWSibling|CWStackMode, &wc);   
    }

    if (setfocus) {
        XISetFocus(dpy, dp->mkbd->info.deviceid, root, CurrentTime);
        XISetClientPointer(dpy, None, dp->mptr->info.deviceid);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
    
    setsel(dp, NULL);
}

void
unmanage(Client *c, int destroyed)
{
    Monitor *m = c->mon;
    XWindowChanges wc;
    DevPair* dp;

    DBG("+unmanage %lu %d %d\n", c->win, c->isfloating, c->isfullscreen);

    detach(c);
    detachstack(c);

    if (!destroyed) {
        wc.border_width = c->oldbw;
        XGrabServer(dpy); /* avoid race conditions */
        XSetErrorHandler(xerrordummy);
        XSelectInput(dpy, c->win, NoEventMask);
        XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
        XIUngrabButton(dpy, XIAllMasterDevices, XIAnyButton, c->win, LENGTH(anymodifier), anymodifier);
        setclientstate(c, WithdrawnState);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }

    for (dp = c->devstack; dp; dp = dp->next)
    {
        if(dp->move.c == c || dp->resize.c == c)
        {
            dp->move.c = NULL;
            dp->resize.c = NULL;
            XIUngrabDevice(dpy, dp->mptr->info.deviceid, CurrentTime);
        }

        focus(dp, NULL);
    }
    
    updateclientlist();

    // need to rearrange if application is fullscreen, but not if it was floating
    if(!c->isfloating || c->isfullscreen)
        arrange(m);

    free(c);
    DBG("-unmanage %lu %d %d\n", c->win, c->isfloating, c->isfullscreen);
}

void
unmapnotify(XEvent *e)
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

void
genericevent(XEvent *e)
{
    int cookie = 0;
    if(e->xcookie.extension != xi2opcode) {
        return;
    }
    
    if (xi2handler[e->xcookie.evtype] && (cookie = XGetEventData(dpy, &e->xcookie)))
        xi2handler[e->xcookie.evtype](e->xcookie.data);

    if (cookie) {
        XFreeEventData(dpy, &e->xcookie);
    }
}

void
updatebars(void)
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
    for (m = mons; m; m = m->next) {
        if (m->barwin)
            continue;
        m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
                CopyFromParent, DefaultVisual(dpy, screen),
                CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
        
        wc.stack_mode = Above;
        if(highest_barwin == None) {
            lowest_barwin = m->barwin;
            wc.sibling = lowest_barwin;
        }
        else
            wc.sibling = highest_barwin;
        
        XConfigureWindow(dpy, m->barwin, CWSibling|CWStackMode, &wc);
        highest_barwin = m->barwin;

        XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
        XMapRaised(dpy, m->barwin);
        XSetClassHint(dpy, m->barwin, &ch);
        XISelectEvents(dpy, m->barwin, &ptrevm, 1);
    }

    floating_stack_helper = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);

    wc.stack_mode = Above;
    wc.sibling = highest_barwin;
    XConfigureWindow(dpy, floating_stack_helper, CWSibling|CWStackMode, &wc);
}

void
updatebarpos(Monitor *m)
{
    m->wy = m->my;
    m->wh = m->mh;
    if (m->showbar) {
        m->wh -= bh;
        m->by = m->topbar ? m->wy : m->wy + m->wh;
        m->wy = m->topbar ? m->wy + bh : m->wy;
    } else
        m->by = -bh;
}

void
updateclientlist(void)
{
    Client *c;
    Monitor *m;
    
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            XChangeProperty(dpy, root, netatom[NetClientList],
                XA_WINDOW, 32, PropModeAppend,
                (unsigned char *) &(c->win), 1);
}

int
updategeom(DevPair* dp)
{
    int dirty = 0;

#ifdef XINERAMA
    if (XineramaIsActive(dpy)) {
        int i, j, n, nn;
        Client *c;
        Monitor *m;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
        XineramaScreenInfo *unique = NULL;

        for (n = 0, m = mons; m; m = m->next, n++);

        /* only consider unique geometries as separate screens */
        unique = ecalloc(nn, sizeof(XineramaScreenInfo));

        for (i = 0, j = 0; i < nn; i++)
            if (isuniquegeom(unique, j, &info[i]))
                memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
        
        XFree(info);
        nn = j;

        /* new monitors if nn > n */
        for (i = n; i < nn; i++) {
            insertmon(mons_end, createmon());
        }

        for (i = 0, m = mons; i < nn && m; m = m->next, i++) {
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
                updatebarpos(m);
            }
        }

        /* removed monitors if n > nn */
        for (i = nn; i < n; i++) {
            while ((c = m->clients)) {
                dirty = 1;
                mons_end->clients = c->next;
                detachstack(c);
                c->mon = mons;
                attach(c);
                attachstack(c);
            }
            if (dp && mons_end == dp->selmon)
                setselmon(dp, mons);
            cleanupmon(mons_end);
        }

        free(unique);
    } else
#endif /* XINERAMA */
    { /* default monitor setup */
        if (!mons) {
            insertmon(mons_end, createmon());
        }
        if (mons->mw != sw || mons->mh != sh) {
            dirty = 1;
            mons->mw = mons->ww = sw;
            mons->mh = mons->wh = sh;
            updatebarpos(mons);
        }
    }
    if (dirty && dp)
        setselmon(dp, mons);
    return dirty;
}

void
updatenumlockmask(void)
{
    int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++)
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
                == XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
    long msize;
    XSizeHints size = {0};

    if (!XGetWMNormalHints(dpy, c->win, &size, &msize)) {
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
        c->isfloating = !!(size.flags & (PSize | PWinGravity));
    
    c->hintsvalid = 1;
}

void
updatestatus(void)
{
    Monitor* m;
    if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
        strcpy(stext, "mpwm-"VERSION);
    for (m = mons; m; m = m->next)
        drawbar(m);
}

void
updatetitle(Client *c)
{
    if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof(c->name)))
        gettextprop(c->win, XA_WM_NAME, c->name, sizeof(c->name));
    if (c->name[0] == '\0') /* hack to mark broken clients */
        strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
    Atom state = getatomprop(c, netatom[NetWMState]);
    Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

    if (state == netatom[NetWMFullscreen])
        setfullscreen(c, 1);
    if (wtype == netatom[NetWMWindowTypeDialog])
        c->isfloating = 1;
}

void
updatewmhints(Client* c)
{
    XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, c->win))) {
        if (c->devices && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->win, wmh);
        } else
            c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
        if (wmh->flags & InputHint)
            c->neverfocus = !wmh->input;
        else
            c->neverfocus = 0;
        XFree(wmh);
    }
}

Client *
wintoclient(Window w)
{
    Client *c;
    Monitor *m;

    for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) {
            if (c->win == w)
                return c;
        }
    }
    return NULL;
}

Monitor*
anywintomon(Window w)
{
    Client *c;
    Monitor *m;

    for (m = mons; m; m = m->next) {
        if(m->barwin == w)
            return m;
        for (c = m->clients; c; c = c->next)
            if (c->win == w)
                return c->mon;
    }
    return NULL;
}

Monitor*
wintomon(DevPair* dp, Window w)
{
    int x, y;
    Client *c;
    Monitor *m;

    if (w == root && getrootptr(dp, &x, &y))
        return recttomon(dp, x, y, 1, 1);
    for (m = mons; m; m = m->next)
        if (w == m->barwin)
            return m;
    if ((c = wintoclient(w)))
        return c->mon;
    return dp->selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
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
    
    if ((ee->request_code == xi2opcode && ee->error_code == BadMatch)
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
    
    return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(__attribute__((unused)) Display * dpy, __attribute__((unused)) XErrorEvent * ee)
{
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(__attribute__((unused)) Display * dpy, __attribute__((unused)) XErrorEvent * ee)
{
    die("mpwm: another window manager is already running");
    return -1;
}

Atom Dbg_XInternAtom(Display *dpy, const char* name, Bool only_if_exists)
{
    Atom atom = XInternAtom(dpy, name, only_if_exists);
    DBG("[%s] %lu\n", name, atom);
    return atom;
}

void
setup(void)
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
    mons = NULL;
    mons_end = NULL;
    spawnmon = NULL;
    forcedfocusmon = NULL;
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    drw = drw_create(dpy, screen, root, sw, sh);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
        die("no fonts could be loaded.");
    lrpad = drw->fonts->h;
    bh = drw->fonts->h + 2;
    updategeom(NULL);
    /* init atoms */
    utf8string = Dbg_XInternAtom(dpy, "UTF8_STRING", False);
    wmatom[WMProtocols] = Dbg_XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmatom[WMIgnoreEnter] = Dbg_XInternAtom(dpy, "WM_IGNORE_ENTER", False);
    wmatom[WMNormalEnter] = Dbg_XInternAtom(dpy, "WM_NORMAL_ENTER", False);
    wmatom[WMDelete] = Dbg_XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wmatom[WMState] = Dbg_XInternAtom(dpy, "WM_STATE", False);
    wmatom[WMTakeFocus] = Dbg_XInternAtom(dpy, "WM_TAKE_FOCUS", False);
    netatom[NetActiveWindow] = Dbg_XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    netatom[NetSupported] = Dbg_XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetWMName] = Dbg_XInternAtom(dpy, "_NET_WM_NAME", False);
    netatom[NetWMState] = Dbg_XInternAtom(dpy, "_NET_WM_STATE", False);
    netatom[NetWMCheck] = Dbg_XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    netatom[NetWMFullscreen] = Dbg_XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    netatom[NetWMWindowType] = Dbg_XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    netatom[NetWMWindowTypeDialog] = Dbg_XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    netatom[NetWMTooltip] = Dbg_XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    netatom[NetWMPopupMenu] = Dbg_XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    netatom[NetClientList] = Dbg_XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    /* init cursors */
    cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
    cursor[CurResize] = drw_cur_create(drw, XC_sizing);
    cursor[CurMove] = drw_cur_create(drw, XC_fleur);

    /* init appearance */
    scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
    for (i = 0; i < LENGTH(colors); i++)
        scheme[i] = drw_scm_create(drw, colors[i], LENGTH(*colors));
    ff_scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
    for (i = 0; i < LENGTH(ff_colors); i++)
        ff_scheme[i] = drw_scm_create(drw, ff_colors[i], LENGTH(*ff_colors));
    /* init bars */
    updatebars();
    updatestatus();
    /* supporting window for NetWMCheck */
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *) "mpwm", 4);
    XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);
    /* EWMH support per view */
    XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *) netatom, NetLast);
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    /* set cursor on root window */
    XDefineCursor(dpy, root, cursor[CurNormal]->cursor);
    /* select events */
    memset(&wa, 0, sizeof(wa));
    wa.cursor = cursor[CurNormal]->cursor;
    wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask | PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
    XSetWMProtocols(dpy, root, &wmatom[WMIgnoreEnter], 2);
    XSelectInput(dpy, root, wa.event_mask);
    XISetMask(hcmask, XI_HierarchyChanged);
    XISelectEvents(dpy, root, &hcevm, 1);
    XISetMask(hcmask, XI_Motion);
    XISelectEvents(dpy, root, &ptrevm, 1);
    
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
    if (!(dpy = XOpenDisplay(NULL)))
        die("mpwm: cannot open display");
    if (!XQueryExtension(dpy, "XInputExtension", &xi2opcode, &(int){0}, &(int){0}))
        die("XInputExtension not available.\n");
    if (XIQueryVersion(dpy, &major, &minor) == BadRequest)
        die("XInput 2.0 not available. Server only supports %d.%d\n", major, minor);
    checkotherwm();
    setup();
    scan();
    run();
    cleanup();
    dprintf(log_fd, "closing gracefully\n");
    close(log_fd);
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}
