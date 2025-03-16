#pragma once

#include "common.h"

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

typedef struct Rule_t Rule;

typedef struct Rule_t {
    Rule *next;  /* next rule */
	char *class;
	char *instance;
	char *title;
	unsigned int tags;
	int isfloating;
    int isfullscreen;
	int monitor;
} Rule;

typedef struct {
    char *config_file;
    const char **fonts;
    unsigned int fonts_len;

    int lockfullscreen;        /* 1 means focus can change by a focusstack call */
    int gappx;                 /* gap amount in pixels between clients */
    int bhgappx;               /* gap between top bar and clients */
    unsigned int borderpx;     /* border pixel of windows */

    Rule *rules;
} Config;

extern Config gcfg;

/* appearance */
static const int snap               = 5;        /* snap pixel */
static const int rmaster            = 0;        /* 1 means master-area is initially on the right */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#7d4848";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#0066AA";
static const char col_green[]       = "#88AA00";
static const char col_red[]         = "#AA4444";
static const char col_ff_cyan[]     = "#003380";
static const char col_ff_green[]    = "#33AA22";
static const char col_ff_red[]      = "#AA1111";

/* tagging */
extern const char *gtags[];
extern unsigned int gtags_len;

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */

/* commands */
extern char dmenumon[]; /* component of dmenucmd, manipulated in spawn() */
extern const char *dmenucmd[];

extern const Button gbuttons[];
extern const unsigned int gbuttons_len;
extern const Key gkeys[];
extern const unsigned int gkeys_len;

extern void load_config(void);
extern void unload_config(void);
