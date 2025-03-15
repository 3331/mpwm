#include "cmds.h"
#include "config.h"
#include "util.h"
#include "barwin.h"
#include "devpair.h"
#include "monitor.h"
#include "client.h"
#include "resolvers.h"

#include <unistd.h>

#include <sys/wait.h>

void quit(__attribute__((unused)) DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    gwm.running = 0;
}

void focusmon(DevPair *dp, const Arg *arg)
{
    Monitor *m;

    if (!gwm.mons->next)
        return;
    if ((m = dirtomon(dp, arg->i)) == dp->selmon)
        return;
    
    unfocus(dp, 0);
    setselmon(dp, m);
    focus(dp, NULL);
}

void swapmon(DevPair *dp, const Arg *arg)
{
    Monitor *tarm;

    if (!gwm.mons->next)
        return;
    if ((tarm = dirtomon(dp, arg->i)) == dp->selmon)
        return;
    
    Monitor *curm = dp->selmon;

    unfocus(dp, 0);

    swap_int(&curm->nmaster, &tarm->nmaster);
    swap_ulong(&curm->barwin, &tarm->barwin);
    swap_float(&curm->mfact, &tarm->mfact);
    swap_int(&curm->rmaster, &tarm->rmaster);
    swap_int(&curm->num, &tarm->num);
    swap_int(&curm->mx, &tarm->mx);
    swap_int(&curm->my, &tarm->my);
    swap_int(&curm->mw, &tarm->mw);
    swap_int(&curm->mh, &tarm->mh);
    swap_int(&curm->wx, &tarm->wx);
    swap_int(&curm->wy, &tarm->wy);
    swap_int(&curm->ww, &tarm->ww);
    swap_int(&curm->wh, &tarm->wh);

    updatebarpos(curm);
    updatebarpos(tarm);

    XMoveResizeWindow(gwm.dpy, curm->barwin, curm->wx, curm->by, curm->ww, gwm.bh);
    XMoveResizeWindow(gwm.dpy, tarm->barwin, tarm->wx, tarm->by, tarm->ww, gwm.bh);
    
    if(gwm.forcedfocusmon)
        gwm.forcedfocusmon = tarm;
    
    setselmon(dp, tarm);
    focus(dp, NULL);
    arrange(tarm);
    arrange(curm);
}

void focusstack(DevPair *dp, const Arg *arg)
{
    Client *c = NULL, *i;

    if (!dp || !dp->sel || !dp->selmon || (dp->sel->isfullscreen && gcfg.lockfullscreen))
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
    }
}

void cyclestack(DevPair *dp, const Arg *arg)
{
    Client *c;

    /* TODO: hide fullscreen apps instead and show something somewhere
     * so user knows if theres a fullscreen app in the background
     *
     * its kinda broken right now, need to figure out what to do with
     * floating windows
    */

    if (!dp || !dp->sel || !dp->selmon || (dp->sel->isfullscreen && gcfg.lockfullscreen))
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

void incnmaster(DevPair *dp, const Arg *arg)
{
    dp->selmon->nmaster = MAX(dp->selmon->nmaster + arg->i, 0);
    arrange(dp->selmon);
}

void killclient(DevPair *dp, const Arg *arg __attribute__((unused)))
{
    if (!sendevent(dp->sel, gwm.wmatom[WMDelete])) {
        XGrabServer(gwm.dpy);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(gwm.dpy, DestroyAll);
        XKillClient(gwm.dpy, dp->sel->win);
        XSync(gwm.dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(gwm.dpy);
    }
}

void movemouse(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    Client *c;
    if (!(c = dp->move.c) && !(c = dp->sel))
    {
        return;
    }
    
    if(dp->resize.c)
    {
        XIUngrabDevice(gwm.dpy, dp->mptr->info.deviceid, CurrentTime);
    }

    ptrevm.deviceid = dp->mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Motion);
    XISetMask(ptrmask, XI_ButtonPress);
    XISetMask(ptrmask, XI_ButtonRelease);

    if (XIGrabDevice(
        gwm.dpy,
        dp->mptr->info.deviceid,
        gwm.root,
        CurrentTime,
        gwm.cursor[CurMove]->cursor,
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

void resizemouse(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    Client *c;
    if (!(c = dp->resize.c) && !(c = dp->sel))
        return;

    /* no support resizing fullscreen windows by mouse */
    if (c->isfullscreen) 
        return;
    
    if(dp->move.c)
    {
        XIUngrabDevice(gwm.dpy, dp->mptr->info.deviceid, CurrentTime);
    }

    ptrevm.deviceid = dp->mptr->info.deviceid;
    memset(ptrmask, 0, sizeof(ptrmask));
    XISetMask(ptrmask, XI_Motion);
    XISetMask(ptrmask, XI_ButtonPress);
    XISetMask(ptrmask, XI_ButtonRelease);

    if (XIGrabDevice(
        gwm.dpy,
        dp->mptr->info.deviceid,
        gwm.root,
        CurrentTime,
        gwm.cursor[CurResize]->cursor,
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

void setlayout(DevPair *dp, const Arg *arg)
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
    drawbar(dp->selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(DevPair *dp, const Arg *arg)
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

void spawn(DevPair *dp, const Arg *arg)
{
    pid_t child;
    int status;
    struct sigaction sa;

    if(!dp->selmon)
        return;
    if (arg->v == dmenucmd)
        dmenumon[0] = '0' + dp->selmon->num;
    
    gwm.spawnmon = dp->selmon;
    gwm.spawndev = dp;

    if ((child = fork())) {
        waitpid(child, &status, 0);
    }
    else
    {
        if (gwm.dpy)
            close(ConnectionNumber(gwm.dpy));

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

void tag(DevPair *dp, const Arg *arg)
{
    if (!dp->selmon || !dp->sel)
        return;
    if (dp->sel && arg->ui & TAGMASK) {
        dp->sel->tags = arg->ui & TAGMASK;
        focus(dp, NULL);
        arrange(dp->selmon);
    }
}

void tagmon(DevPair *dp, const Arg *arg)
{
    if (!dp->selmon || !dp->sel || !gwm.mons->next)
        return;

    sendmon(dp, dp->sel, dirtomon(dp, arg->i), 1);
}

void togglebar(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    if(!dp->selmon)
        return;
    dp->selmon->showbar = !dp->selmon->showbar;
    updatebarpos(dp->selmon);
    XMoveResizeWindow(gwm.dpy, dp->selmon->barwin, dp->selmon->wx, dp->selmon->by, dp->selmon->ww, gwm.bh);
    arrange(dp->selmon);
    drawbar(dp->selmon);
}

void togglefloating(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    if (!dp->selmon || !dp->sel)
        return;
    
    // can't toggle floating on a fullscreen window
    if (dp->sel->isfullscreen) 
        return;
    
    if(dp->move.c || dp->resize.c)
    {
        dp->move.c = NULL;
        dp->resize.c = NULL;
        XIUngrabDevice(gwm.dpy, dp->mptr->info.deviceid, CurrentTime);
    }
    
    setfloating(dp->sel, !dp->sel->isfloating, 0, 1);
}

void togglefullscreen(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    if (!dp->selmon || !dp->sel)
        return;

    if(dp->move.c || dp->resize.c)
    {
        dp->move.c = NULL;
        dp->resize.c = NULL;
        XIUngrabDevice(gwm.dpy, dp->mptr->info.deviceid, CurrentTime);
    }

    setfullscreen(dp->sel, !dp->sel->isfullscreen);
}

void toggleautoswapmon(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    Clr **cur_scheme;
    Monitor *fake_tar;
    int x, y;
    int cur_bar_offset, tar_bar_offset;

    if(!dp->selmon)
        return;
    
    if(gwm.forcedfocusmon)
    {
        cur_scheme = gwm.scheme;
        gwm.forcedfocusmon = NULL;
    }
    else
    {
        cur_scheme = gwm.ff_scheme;
        gwm.forcedfocusmon = dp->selmon;
    }

    if(dp->sel && updatewindowtype(dp->sel) < 2)
        XSetWindowBorder(gwm.dpy, dp->sel->win, cur_scheme[CLAMP(SchemeNorm + dp->sel->devices, SchemeNorm, SchemeSel3)][ColBorder].pixel);
    
    if(getrootptr(dp, &x, &y) && (fake_tar = recttomon(dp, x, y, 1, 1)) != dp->selmon)
    {
        if(dp->selmon->showbar)
            if(dp->selmon->topbar)
                cur_bar_offset = -gwm.bh;
            else
                cur_bar_offset = gwm.bh;
        else
            cur_bar_offset = 0;
        
        if(fake_tar->showbar)
            if(fake_tar->topbar)
                tar_bar_offset = -gwm.bh;
            else
                tar_bar_offset = gwm.bh;
        else
            tar_bar_offset = 0;

        XIWarpPointer(gwm.dpy, dp->mptr->info.deviceid, None, None, 0, 0, 0, 0, dp->selmon->wx - fake_tar->wx, (dp->selmon->wy + cur_bar_offset) - (fake_tar->wy + tar_bar_offset));
    }
    
    drawbar(dp->selmon);
}

void togglermaster(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    dp->selmon->rmaster = !dp->selmon->rmaster;
    dp->selmon->mfact = 1.0 - dp->selmon->mfact;
    if (dp->selmon->lt[dp->selmon->sellt]->arrange)
        arrange(dp->selmon);
}

void togglemouse(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    if (!dp->selmon || !dp->sel)
        return;
    if (dp->sel->isfullscreen) /* no support for fullscreen windows */
        return;

    dp->sel->grabbed = !dp->sel->grabbed;
    grabbuttons(dp->mptr, dp->sel, 1);
}

void toggletag(DevPair *dp, const Arg *arg)
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

void toggleview(DevPair *dp, const Arg *arg)
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

void view(DevPair *dp, const Arg *arg)
{
    if ((arg->ui & TAGMASK) == dp->selmon->tagset[dp->selmon->seltags])
        return;
    dp->selmon->seltags ^= 1; /* toggle sel tagset */
    if (arg->ui & TAGMASK)
        dp->selmon->tagset[dp->selmon->seltags] = arg->ui & TAGMASK;
    focus(dp, NULL);
    arrange(dp->selmon);
}

void pop(DevPair *dp, Client *c)
{
    detach(c);
    attach(c);
    focus(dp, c);
    arrange(c->mon);
}

void zoom(DevPair *dp, __attribute__((unused)) const Arg *arg)
{
    Client *c = dp->sel;

    if (!c || !dp->selmon ||
        !dp->selmon->lt[dp->selmon->sellt]->arrange ||
        (dp->sel && dp->sel->isfloating))
        return;
    if (c == nexttiled(dp->selmon->clients) && !(c = nexttiled(c->next)))
        return;
    pop(dp, c);
}
