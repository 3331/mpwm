/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int gappx     = 2;       /* gap amount in pixels between clients */
static const int snap               = 10;       /* snap pixel */
static const int rmaster            = 0;        /* 1 means master-area is initially on the right */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int lockfullscreen     = 1;        /* 1 means focus can change by a focusstack call */
static const char *fonts[]          = { "monospace:size=10" };
static const char dmenufont[]       = "monospace:size=10";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#7d4848";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#0259a6";
static const char col_green[]       = "#228800";
static const char col_red[]         = "#AA2200";
static const char *colors[][5]      = {
    /*               fg         bg         border     border+1    border+2 */
    [SchemeNorm]  = { col_gray3, col_gray1, col_gray2 },
    [SchemeSel]   = { col_gray4, col_cyan,  col_cyan },  /* one device */
    [SchemeSel2]  = { col_gray4, col_green, col_green }, /* two devices */
    [SchemeSel3]  = { col_gray4, col_red,   col_red },   /* three devices */
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
    /* xprop(1):
     *    WM_CLASS(STRING) = instance, class
     *    WM_NAME(STRING) = title
     */
    /* class      instance    title       tags mask     isfloating   monitor */
    
    { "Gimp",     NULL,       NULL,       0,            1,           -1 },

};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
    /* symbol     arrange function */
    { "[]=",      tile },    /* first entry is default */
    { "><>",      NULL },    /* no layout function means floating behavior */
    { "[M]",      monocle },
    { ">M<",      centeredmaster },
};

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "rxvt-unicode", NULL };

static const Key keys[] = {
    /* modifier                     key        function        argument */
    { MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
    { MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
    { MODKEY|ControlMask,           XK_b,      togglebar,      {0} },
    { MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
    { MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
    { MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
    { MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
    { MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
    { MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
    { MODKEY,                       XK_r,      togglermaster,  {0} },
    { MODKEY,                       XK_Return, zoom,           {0} },
    { MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
    { MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
    { MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
    { MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
    { MODKEY,                       XK_o,      setlayout,      {.v = &layouts[3]} },
    { MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
    { MODKEY,                       XK_0,      view,           {.ui = ~0 } },
    { MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
    { MODKEY,                       XK_9,      view,           {.ui = 0 } },
    { MODKEY|ShiftMask,             XK_9,      tag,            {.ui = 0 } },
    { MODKEY,                       XK_Tab,    cyclestack,     {.i = +1 } },
    { MODKEY|ShiftMask,             XK_Tab,    cyclestack,     {.i = -1 } },
    { MODKEY,                       XK_comma,  focusmon,       {.i = +1 } },
    { MODKEY,                       XK_period, focusmon,       {.i = -1 } },
    { MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = +1 } },
    { MODKEY|ShiftMask,             XK_period, tagmon,         {.i = -1 } },
    { MODKEY|ShiftMask|ControlMask, XK_comma,  swapmon,        {.i = +1 } },
    { MODKEY|ShiftMask|ControlMask, XK_period, swapmon,        {.i = -1 } },
    { MODKEY|ShiftMask,             XK_m,      togglemouse,    {0} },
    TAGKEYS(                        XK_1,                      0)
    TAGKEYS(                        XK_2,                      1)
    TAGKEYS(                        XK_3,                      2)
    TAGKEYS(                        XK_4,                      3)
    TAGKEYS(                        XK_5,                      4)
    TAGKEYS(                        XK_6,                      5)
    TAGKEYS(                        XK_7,                      6)
    TAGKEYS(                        XK_8,                      7)
    TAGKEYS(                        XK_9,                      8)
    { MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
    /* click                event mask      button          function        argument */
    { ClkLtSymbol,          0,                        Button1,        setlayout,      {0} },
    { ClkLtSymbol,          0,                        Button3,        setlayout,      {.v = &layouts[2]} },
    { ClkWinTitle,          0,                        Button2,        zoom,           {0} },
    { ClkStatusText,        0,                        Button2,        spawn,          {.v = termcmd } },
    { ClkClientWin,         MODKEY|ShiftMask,         Button1,        movemouse,      {0} },
    { ClkClientWin,         MODKEY,                   Button2,        togglefloating, {0} },
    { ClkClientWin,         MODKEY|ShiftMask,         Button3,        resizemouse,    {0} },
    { ClkTagBar,            0,                        Button1,        view,           {0} },
    { ClkTagBar,            0,                        Button3,        toggleview,     {0} },
    { ClkTagBar,            MODKEY,                   Button1,        tag,            {0} },
    { ClkTagBar,            MODKEY,                   Button3,        toggletag,      {0} },
};
