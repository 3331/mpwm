#include "config.h"
#include "cmds.h"
#include "layouts.h"

const char *fonts[] = { "monospace:size=10" };

Config gcfg = {
    .fonts = fonts,
    .fonts_len = LENGTH(fonts),
    .lockfullscreen = 1,
    .gappx = 0,
    .bhgappx = 2,
    .borderpx = 2
};

const char dmenufont[] = "monospace:size=10";
char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
const char *termcmd[]  = { "rxvt-unicode", NULL };

const char *gtags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
unsigned int gtags_len = LENGTH(gtags);

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(gtags) > 31 ? -1 : 1]; };

const Key gkeys[] = {
    /* modifier                     key        function        argument */
    { MODKEY,                       XK_p,      spawn,             {.v = dmenucmd } },
    { MODKEY|ShiftMask,             XK_Return, spawn,             {.v = termcmd } },
    { MODKEY|ControlMask,           XK_b,      togglebar,         {0} },
    { MODKEY,                       XK_j,      focusstack,        {.i = +1 } },
    { MODKEY,                       XK_k,      focusstack,        {.i = -1 } },
    { MODKEY,                       XK_i,      incnmaster,        {.i = +1 } },
    { MODKEY,                       XK_d,      incnmaster,        {.i = -1 } },
    { MODKEY,                       XK_h,      setmfact,          {.f = -0.05} },
    { MODKEY,                       XK_l,      setmfact,          {.f = +0.05} },
    { MODKEY,                       XK_r,      togglermaster,     {0} },
    { MODKEY,                       XK_Return, zoom,              {0} },
    { MODKEY|ShiftMask,             XK_c,      killclient,        {0} },
    { MODKEY,                       XK_t,      setlayout,         {.v = &glayouts[0]} },
    { MODKEY,                       XK_f,      setlayout,         {.v = &glayouts[1]} },
    { MODKEY,                       XK_m,      setlayout,         {.v = &glayouts[2]} },
    { MODKEY,                       XK_o,      setlayout,         {.v = &glayouts[3]} },
    { MODKEY|ShiftMask,             XK_space,  togglefloating,    {0} },
    { MODKEY|ShiftMask,             XK_f,      togglefullscreen,  {0} },
    { MODKEY|ShiftMask,             XK_s,      toggleautoswapmon, {0} },
    { MODKEY,                       XK_0,      view,              {.ui = ~0 } },
    { MODKEY|ShiftMask,             XK_0,      tag,               {.ui = ~0 } },
    { MODKEY,                       XK_9,      view,              {.ui = 0 } },
    { MODKEY|ShiftMask,             XK_9,      tag,               {.ui = 0 } },
    { MODKEY,                       XK_Tab,    cyclestack,        {.i = +1 } },
    { MODKEY|ShiftMask,             XK_Tab,    cyclestack,        {.i = -1 } },
    { MODKEY,                       XK_comma,  focusmon,          {.i = +1 } },
    { MODKEY,                       XK_period, focusmon,          {.i = -1 } },
    { MODKEY|ShiftMask,             XK_comma,  tagmon,            {.i = +1 } },
    { MODKEY|ShiftMask,             XK_period, tagmon,            {.i = -1 } },
    { MODKEY|ShiftMask|ControlMask, XK_comma,  swapmon,           {.i = +1 } },
    { MODKEY|ShiftMask|ControlMask, XK_period, swapmon,           {.i = -1 } },
    { MODKEY|ShiftMask,             XK_m,      togglemouse,       {0} },
    TAGKEYS(                        XK_1,                         0)
    TAGKEYS(                        XK_2,                         1)
    TAGKEYS(                        XK_3,                         2)
    TAGKEYS(                        XK_4,                         3)
    TAGKEYS(                        XK_5,                         4)
    TAGKEYS(                        XK_6,                         5)
    TAGKEYS(                        XK_7,                         6)
    TAGKEYS(                        XK_8,                         7)
    TAGKEYS(                        XK_9,                         8)
    { MODKEY|ShiftMask,             XK_q,      quit,              {0} },
};
const unsigned int gkeys_len = LENGTH(gkeys);

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
const Button gbuttons[] = {
    /* click                event mask      button          function        argument */
    { ClkLtSymbol,          0,                        Button1,        setlayout,      {0} },
    { ClkLtSymbol,          0,                        Button3,        setlayout,      {.v = &glayouts[2]} },
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
const unsigned int gbuttons_len = LENGTH(gbuttons);
