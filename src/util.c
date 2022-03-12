/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

extern int log_fd;

void *
ecalloc(size_t nmemb, size_t size)
{
    void *p;

    if (!(p = calloc(nmemb, size)))
        die("calloc:");
    return p;
}

void
die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vdprintf(log_fd, fmt, ap);
    va_end(ap);

    if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
        fputc(' ', stderr);
        perror(NULL);
    } else {
        fputc('\n', stderr);
    }

    close(log_fd);
    exit(1);
}

void inline
swap_int(int* a, int* b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void inline
swap_ulong(unsigned long* a, unsigned long* b)
{
    unsigned long tmp = *a;
    *a = *b;
    *b = tmp;
}
