/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "util.h"

extern int log_fd;
#ifdef DEBUG
int indent;
#endif

void *ecalloc(size_t nmemb, size_t size)
{
    void *p;

    if (!(p = calloc(nmemb, size)))
        die("calloc:");
    return p;
}

void die(const char *fmt, ...) {
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

char* read_file_to_buffer(const char *filename, size_t *size)
{
    FILE *file = fopen(filename, "rb");  // Open file in binary mode
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);  // Move to the end of the file
    *size = ftell(file);       // Get the current position (file size)
    fseek(file, 0, SEEK_SET);  // Move back to the beginning of the file

    // We dont care about empty files
    if(!(*size)) {
        fclose(file);
        return NULL;
    }

    // Allocate memory for the file content
    char *buffer = (char*)malloc(*size + 1);  // +1 for null-terminator
    if (!buffer) {
        perror("Error allocating memory");
        fclose(file);
        return NULL;
    }

    // Read the file into the buffer
    size_t bytesRead = fread(buffer, 1, *size, file);
    if (bytesRead != *size) {
        perror("Error reading file");
        free(buffer);
        fclose(file);
        return NULL;
    }

    // Null-terminate the string (optional if you're dealing with binary data)
    buffer[*size] = '\0';

    fclose(file);
    return buffer;
}

void swap_float(float *a, float *b)
{
    float tmp = *a;
    *a = *b;
    *b = tmp;
}

void swap_int(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void swap_uint32(uint32_t *a, uint32_t *b)
{
    uint32_t tmp = *a;
    *a = *b;
    *b = tmp;
}

void swap_void(void **a, void **b)
{
    void *tmp = *a;
    *a = *b;
    *b = tmp;
}

void swap_ulong(unsigned long *a, unsigned long *b)
{
    unsigned long tmp = *a;
    *a = *b;
    *b = tmp;
}
