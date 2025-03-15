#include "layouts.h"
#include "config.h"
#include "client.h"

const Layout glayouts[] = {
    /* symbol     arrange function */
    { "[]=",      tile },    /* first entry is default */
    { "><>",      NULL },    /* no layout function means floating behavior */
    { "[M]",      monocle },
    { ">M<",      centeredmaster },
};
const unsigned int glayouts_len = LENGTH(glayouts);

void tile(Monitor *m)
{
    int h, r, mw, my, ty, i, n, oe = 1, ie = 1;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);

    if (n == 0)
        return;

    if (n > m->nmaster)
        mw = m->nmaster ? (m->ww + gcfg.gappx * ie) * (m->rmaster ? 1.0 - m->mfact : m->mfact) : 0;
    else
        mw = m->ww - 2 * gcfg.gappx * oe + gcfg.gappx * ie;
    
    for (i = 0, my = ty = gcfg.gappx * oe, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < m->nmaster) {
            r = MIN(n, m->nmaster) - i;
            h = (m->wh - my - gcfg.gappx * oe - gcfg.gappx * ie * (r - 1)) / r;
            resize(c, m->rmaster ? m->wx + m->ww - mw : m->wx + gcfg.gappx * oe, m->wy + my, mw - (2 * c->bw) - gcfg.gappx * ie, h - (2 * c->bw), 0);
            my += HEIGHT(c) + gcfg.gappx * ie;
        } else {
            r = n - i;
            h = (m->wh - ty - gcfg.gappx * oe - gcfg.gappx * ie * (r - 1)) / r;
            resize(c, m->rmaster ? m->wx : m->wx + mw + gcfg.gappx * oe, m->wy + ty, m->ww - mw - (2 * c->bw) - 2 * gcfg.gappx * oe, h - (2 * c->bw), 0);
            ty += HEIGHT(c) + gcfg.gappx * ie;
        }
    }
}

void monocle(Monitor *m)
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

void centeredmaster(Monitor *m)
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
