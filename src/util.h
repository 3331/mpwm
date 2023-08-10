/* See LICENSE file for copyright and license details. */

#include <stdint.h>

#define CLAMP(X, A, B)          (MIN(MAX(A, B), MAX(X, MIN(A, B))))
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))

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

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);

void swap_int(int* a, int* b);
void swap_uint32(uint32_t* a, uint32_t* b);
void swap_void(void** a, void** b);
void swap_float(float* a, float* b);
void swap_ulong(unsigned long* a, unsigned long* b);
