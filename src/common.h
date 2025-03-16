#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/XKBlib.h>

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <X11/Xft/Xft.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XInput2.h>

#include <sys/param.h>
/* macros */

#define CLAMP(X, A, B)          (MIN(MAX(A, B), MAX(X, MIN(A, B))))
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))
#define LENGTH(X)               (sizeof(X) / sizeof(X)[0])

#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(gwm.numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh+((m)->showbar ? ((m)->topbar ? 0 : gwm.bh) : 0)) - MAX((y),(m)->wy-((m)->showbar ? ((m)->topbar ? gwm.bh : 0) : 0))))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << gtags_len) - 1)

#define MAXDEVICES 256 /* from xorg-server/include/misc.h */

typedef XftColor Clr;
typedef struct Monitor_t Monitor;
typedef struct Client_t Client;
typedef struct DevPair_t DevPair;
typedef struct Device_t Device;

/* enums */

/* cursor */
enum {
    CurNormal,
    CurResize,
    CurMove,
    CurLast
};

/* color schemes */
enum {
    SchemeNorm,
    SchemeSel,
    SchemeSel2,
    SchemeSel3
};

/* EWMH atoms */
enum {
    NetSupported,
    NetWMName,
    NetWMState,
    NetWMCheck,
    NetWMFullscreen,
    NetActiveWindow,
    NetWMWindowType,
    NetWMWindowTypeDialog,
    NetClientList,
    NetWMTooltip,
    NetWMPopupMenu,
    NetLast
};

/* default atoms */
enum {
    WMProtocols,
    WMIgnoreEnter,
    WMNormalEnter,
    WMDelete,
    WMState,
    WMTakeFocus,
    WMLast
};

/* clicks */
enum {
    ClkTagBar,
    ClkLtSymbol,
    ClkStatusText,
    ClkWinTitle,
    ClkClientWin,
    ClkRootWin,
    ClkLast
};

typedef struct {
    const char *symbol;
    void (*arrange)(Monitor*);
} Layout;

typedef struct {
    Client *c;
    Time time;
    int active;
    int detail;
    int x;
    int y;
    int ox;
    int oy;
} Motion;

typedef struct Client_t {
    Client *next;
    Client *snext;
    Monitor *mon;
    Clr **prev_scheme;
    DevPair *devstack;
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

typedef struct Monitor_t {
    Monitor *next;
    Monitor *prev;
    Client *clients;
    Client *stack;
    DevPair *devstack;
    char ltsymbol[16];
    float mfact;
    int nmaster;
    int num;
    int by;               /* bar geometry */
    int mx, my, mw, mh;   /* screen size */
    int wx, wy, ww, wh;   /* window area  */
    int arranging_clients;
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

typedef struct Device_t {
    DevPair *self;
    XIHierarchyInfo info;
    Device *snext; /* next slave */
    Device *sprev; /* prev slave (not implemented) */
} Device;

typedef struct DevPair_t {
    DevPair *next;  /* next devpair */
    DevPair *fnext; /* next devpair that has same focus */
    DevPair *mnext; /* next devpair that has same monitor */
    Monitor *selmon;
    Monitor *lastselmon;
    Device *slaves;
    Device *mptr;
    Device *mkbd;
    Client *sel;
    Motion resize;
    Motion move;
    Time lastevent;
    int lastdetail;
} DevPair;

typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

typedef struct {
    unsigned int click;
    unsigned int mask;
    int button;
    void (*func)(DevPair*, const Arg*);
    const Arg arg;
} Button;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(DevPair*, const Arg*);
    const Arg arg;
} Key;

typedef struct {
    Cursor cursor;
} Cur;

typedef struct Fnt {
    Display *dpy;
    unsigned int h;
    XftFont *xfont;
    FcPattern *pattern;
    struct Fnt *next;
} Fnt;

/* Clr scheme index */
enum {
    ColFg,
    ColBg,
    ColBorder,
    ColBorder2,
    ColBorder3
}; 

typedef struct {
    unsigned int w, h;
    Display *dpy;
    int screen;
    Window root;
    Drawable drawable;
    GC gc;
    Clr *scheme;
    Fnt *fonts;
} Drw;

typedef struct {
    char stext[256];

    int running;
    int forcing_focus;
    int numlockmask;

    Atom wmatom[WMLast];
    Atom netatom[NetLast];
    int xi2opcode;
    int (*xerrorxlib)(Display *, XErrorEvent *);

    Cur *cursor[CurLast];
    Clr **scheme;
    Clr **ff_scheme;
    Clr **cur_scheme;

    Display *dpy;
    int screen;
    int sw;               /* X display screen geometry width */
    int sh;               /* X display screen geometry height */

    DevPair *devpairs;
    DevPair *spawndev;

    Monitor *mons;
    Monitor *mons_end;
    Monitor *spawnmon;
    Monitor *forcedfocusmon;

    Window root;
    Window wmcheckwin;
    Window lowest_barwin;
    Window highest_barwin;
    Window floating_stack_helper;

    /* bar properties (move to a `struct Barwin` in barwin.h eventually)*/
    int bh;               /* bar height */
    int lrpad;            /* sum of left and right padding for text */
} Wm;

extern Wm gwm;

extern XIGrabModifiers ganymodifier[];
extern unsigned int ganymodifier_len;
extern unsigned char hcmask[XIMaskLen(XI_HierarchyChanged)];
extern unsigned char ptrmask[XIMaskLen(XI_LASTEVENT)];
extern unsigned char kbdmask[XIMaskLen(XI_LASTEVENT)];
extern XIEventMask hcevm;
extern XIEventMask ptrevm;
extern XIEventMask kbdevm;

extern int xerror(Display *display, XErrorEvent *ee);
extern int xerrordummy(Display *display, XErrorEvent *ee);
extern int xerrorstart(Display *display, XErrorEvent *ee);

extern int gettextprop(Window w, Atom atom, char *text, unsigned int size);
extern Atom getatomprop(Client *c, Atom prop);
extern int updategeom(DevPair *dp);
