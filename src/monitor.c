#include "monitor.h"
#include "util.h"
#include "client.h"
#include "events.h"

void showhide(Client *c)
{
    if (!c)
        return;

    if (ISVISIBLE(c))
    {
        /* show clients top down */
        DBG("+showhide %lu\n", c->win);
        XMoveWindow(gwm.dpy, c->win, c->x, c->y);
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
        XMoveWindow(gwm.dpy, c->win, WIDTH(c) * -2, c->y);
    }
}

void arrange(Monitor *m)
{
    XEvent ev;

    if(m->arranging_clients)
        return;

    m->arranging_clients = 1;

    DBG("+arrange\n");

    if (m)
        showhide(m->stack);
    else
        for (m = gwm.mons; m; m = m->next)
            showhide(m->stack);
    
    if (m)
        arrangemon(m);
    else
        for (m = gwm.mons; m; m = m->next)
            arrangemon(m);

    if(m->arranging_clients)
    {
        while (XCheckTypedEvent(gwm.dpy, GenericEvent, &ev)) {
            fire_event(ev.type, &ev);
        }
        m->arranging_clients = 0;
    }
}

void arrangemon(Monitor *m)
{
    DBG("+arrangemon\n");
    strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof(m->ltsymbol) - 1);
    if (m->lt[m->sellt]->arrange)
        m->lt[m->sellt]->arrange(m);
}

void insertmon(Monitor *at, Monitor *m)
{
    if(!at && gwm.mons) /* infront of mons */
    {
        gwm.mons->prev = m;
        m->next = gwm.mons;
        gwm.mons = m;
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
    {
        gwm.mons = m;
    }
    
    if(!m->next)
    {
        gwm.mons_end = m;
    }
}

void unlinkmon(Monitor *m)
{
    if(m->next)
        m->next->prev = m->prev;
    else
        gwm.mons_end = m->prev;
    
    if(m->prev)
        m->prev->next = m->next;
    else
        gwm.mons = m->next;
    
    m->next = NULL;
    m->prev = NULL;
}

void cleanupmon(Monitor *m)
{
    unlinkmon(m);

    XUnmapWindow(gwm.dpy, m->barwin);
    XDestroyWindow(gwm.dpy, m->barwin);
    free(m);
}
