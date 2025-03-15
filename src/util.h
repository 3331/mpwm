/* See LICENSE file for copyright and license details. */
#pragma once

#include <stdint.h>
#include <sys/param.h>

#ifdef DEBUG
extern int indent;
#define DBG_IN(...) dprintf(log_fd, "%*s", indent*4, "");dprintf(log_fd, __VA_ARGS__);indent++;
#define DBG_OUT(...) indent--;dprintf(log_fd, "%*s", indent*4, "");dprintf(log_fd, __VA_ARGS__);
#define DBG(...) dprintf(log_fd, "%*s", indent*4, "");dprintf(log_fd, __VA_ARGS__);
#else
#define DBG_IN(...) while(0) {}
#define DBG_OUT(...) while(0) {}
#define DBG(...) while(0) {}
#endif

extern void die(const char *fmt, ...);
extern void *ecalloc(size_t nmemb, size_t size);

extern void swap_int(int *a, int *b);
extern void swap_uint32(uint32_t *a, uint32_t *b);
extern void swap_void(void **a, void **b);
extern void swap_float(float *a, float *b);
extern void swap_ulong(unsigned long *a, unsigned long *b);
