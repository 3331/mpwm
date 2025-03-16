#include "config.h"
#include "client.h"
#include "util.h"
#include "cmds.h"
#include "layouts.h"

#include <json-c/json.h>
#include <json-c/json_object.h>
#include <json-c/json_object_iterator.h>
#include <json-c/json_types.h>

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
    { MODKEY|ShiftMask|ControlMask, XK_r,      reloadconfig,      {0} },
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

void unload_rule(Rule* r)
{
    if(!r)
        return;

    if(r->class)
        free(r->class);
    if(r->instance)
        free(r->instance);
    if(r->title)
        free(r->title);
}

void unload_rules(Rule** head)
{
    Rule* r = *head;
    Rule* next_rule = NULL;

    for(; r; r = next_rule)
    {
        next_rule = r->next;
        unload_rule(r);
        free(r);
    }

    *head = NULL;
}

int append_rule(Rule **head, Rule *r)
{
    Rule *r_end = *head;
    for (; r_end && r_end->next; r_end = r_end->next);

    if(!r_end)
    {
        *head = r;
        return 0;
    }

    if(r_end->next)
    {
        DBG("r_end has a next pointer, this shouldn't be possible\n");
        return 1;
    }

    r_end->next = r;
    return 0;
}

void read_json_string(const char* outer_key, const char *inner_key, char **out, char *default_value, struct json_object *outer_jobj)
{
    if(out)
        *out = default_value;

    struct json_object *inner_jobj = NULL;
    if (!outer_jobj)
        return;

    inner_jobj = json_object_object_get(outer_jobj, inner_key);
    if (!inner_jobj)
        return;

    if(json_object_get_type(inner_jobj) != json_type_string)
    {
        fprintf(stderr, "[%s.%s] expected string type\n", outer_key, inner_key);
        return;
    }

    char *str = (char *)json_object_get_string(inner_jobj);
    if(str && strlen(str) && out)
        *out = strdup(str);
}

void read_json_int(const char* outer_key, const char *inner_key, int *out, int default_value, struct json_object *outer_jobj)
{
    if(out)
        *out = default_value;

    struct json_object *inner_jobj = NULL;
    if (!outer_jobj)
        return;

    inner_jobj = json_object_object_get(outer_jobj, inner_key);
    if (!inner_jobj)
        return;

    if(json_object_get_type(inner_jobj) != json_type_int)
    {
        fprintf(stderr, "[%s.%s] expected integer type\n", outer_key, inner_key);
        return;
    }

    if(out)
        *out = json_object_get_int(inner_jobj);
}

void read_json_boolean(const char* outer_key, const char *inner_key, int *out, int default_value, struct json_object *outer_jobj)
{
    if(out)
        *out = default_value;

    struct json_object *inner_jobj = NULL;
    if (!outer_jobj)
        return;

    inner_jobj = json_object_object_get(outer_jobj, inner_key);
    if (!inner_jobj)
        return;

    if(json_object_get_type(inner_jobj) != json_type_boolean)
    {
        fprintf(stderr, "[%s.%s] expected boolean type\n", outer_key, inner_key);
        return;
    }

    if(out)
        *out = json_object_get_boolean(inner_jobj);
}

Rule *parse_rule(const char *key, struct json_object *jrule)
{
    Rule *r = ecalloc(1, sizeof(Rule));
    if(!r)
        return NULL;

    read_json_string(key, "class", &r->class, NULL, jrule);
    read_json_string(key, "instance", &r->instance, NULL, jrule);
    read_json_string(key, "title", &r->title, NULL, jrule);
    read_json_int(key, "tags", (int*)&r->tags, 0, jrule);
    read_json_boolean(key, "isfloating", &r->isfloating, 0, jrule);
    read_json_boolean(key, "isfullscreen", &r->isfullscreen, 0, jrule);
    read_json_int(key, "monitor", &r->monitor, -1, jrule);

    return r;
}

int parse_rules(struct json_object *jcfg, Rule **out_rules)
{
	struct json_object *rules_obj;
    Rule *r = NULL;
    int ret = 0;

    rules_obj = json_object_object_get(jcfg, "rules");
    if(!rules_obj)
    {
        fprintf(stderr, "empty rules configuration\n");
        goto failed;
    }

    json_object_object_foreach(rules_obj, key, val) {
        switch (json_object_get_type(val)) {
            case json_type_object:
                r = parse_rule(key, val);
                if(!r)
                    break;
                append_rule(out_rules, r);
                break;
            default:
                fprintf(stderr, "Unknown field found in json blob for rules: %s\n", key);
                break;
        }
    }

    goto end;

failed:
    ret = 1;
    unload_rules(out_rules);
end:
    return ret;
}

void load_rules_config(struct json_object *jcfg)
{
    Rule *new_rule_head = NULL;
    if(parse_rules(jcfg, &new_rule_head))
    {
        // failed to parse rules, just abort and dont overwrite old rules
        return;
    }

    unload_rules(&gcfg.rules);
    gcfg.rules = new_rule_head;
}

void load_config(void)
{
    DBG("loading config!\n");
    struct json_object *jcfg;
    size_t json_cfg_size = 0;
    char* json_cfg = read_file_to_buffer(gcfg.config_file, &json_cfg_size);

    if(!json_cfg || !json_cfg_size)
    {
        DBG("config is empty\n");
        fprintf(stderr, "config file is empty\n");
        return;
    }

	jcfg = json_tokener_parse(json_cfg);
    free(json_cfg);
    if(!jcfg)
    {
        fprintf(stderr, "failed to parse json\n");
        return;
    }

    load_rules_config(jcfg);

    json_object_put(jcfg);
}

void unload_config(void)
{
    unload_rules(&gcfg.rules);
}