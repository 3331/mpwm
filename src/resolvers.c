#include "resolvers.h"
#include "devpair.h"

Monitor *dirtomon(DevPair *dp, int dir)
{
    int best_neg = 0;
    int best_pos = 0;
    int middle = dp->selmon->mx + (dp->selmon->mw / 2);
    int delta;
    Monitor *m = NULL;
    Monitor *m_left = NULL;
    Monitor *m_right = NULL;

    // Left
    if (dir > 0)
    {
        // looking for the closest positive number
        for(m = gwm.mons; m; m = m->next)
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
    for(m = gwm.mons; m; m = m->next)
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

Monitor *anywintomon(Window w)
{
    Client *c;
    Monitor *m;

    for (m = gwm.mons; m; m = m->next) {
        if(m->barwin == w)
            return m;
        for (c = m->clients; c; c = c->next)
            if (c->win == w)
                return c->mon;
    }
    return NULL;
}

Monitor *wintomon(DevPair *dp, Window w)
{
    int x, y;
    Client *c;
    Monitor *m;

    if (w == gwm.root && getrootptr(dp, &x, &y))
        return recttomon(dp, x, y, 1, 1);
    for (m = gwm.mons; m; m = m->next)
        if (w == m->barwin)
            return m;
    if ((c = wintoclient(w)))
        return c->mon;
    return dp->selmon;
}

Monitor *recttomon(DevPair *dp, int x, int y, int w, int h)
{
    Monitor *m, *r = dp->selmon;
    int a, area = 0;

    for (m = gwm.mons; m; m = m->next)
        if ((a = INTERSECT(x, y, w, h, m)) > area) {
            area = a;
            r = m;
        }
    return r;
}

Client *wintoclient(Window w)
{
    Client *c;
    Monitor *m;

    for (m = gwm.mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) {
            if (c->win == w)
                return c;
        }
    }
    return NULL;
}
