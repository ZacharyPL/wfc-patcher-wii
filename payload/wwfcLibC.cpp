#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <wwfcCommon.h>

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + 'a' - 'A';
    }

    return c;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c + 'A' - 'a';
    }

    return c;
}

__attribute__((__optimize__("-fno-tree-loop-distribute-patterns"))) //
void* memcpy(void* __restrict dest, const void* __restrict src, size_t n)
{
    u8* d = (u8*) dest;
    const u8* s = (const u8*) src;

    while (n-- > 0) {
        *d++ = *s++;
    }

    return dest;
}

__attribute__((__optimize__("-fno-tree-loop-distribute-patterns"))) //
void* memset(void* s, int c, size_t n)
{
    u8* p = (u8*) s;
    for (size_t i = 0; i < n; i++) {
        p[i] = c;
    }
    return s;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
    const u8* su1 = (const u8*) s1;
    const u8* su2 = (const u8*) s2;

    size_t i = 0;
    for (; i < n && su1[i] == su2[i]; i++) {
    }

    return i < n ? su1[i] - su2[i] : 0;
}

void* memchr(const void* s, int c, size_t n)
{
    const u8* su = (const u8*) s;

    for (size_t i = 0; i < n; i++) {
        if (su[i] == c) {
            // This function is also a const-cast
            return (void*) &su[i];
        }
    }

    return NULL;
}

size_t strlen(const char* s)
{
    const char* f = s;
    while (*s != '\0') {
        s++;
    }
    return s - f;
}

int strcmp(const char* s1, const char* s2)
{
    size_t i = 0;
    for (; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') {
            break;
        }
    }

    return s1[i] - s2[i];
}

int strncmp(const char* s1, const char* s2, size_t n)
{
    size_t i = 0;
    for (; i < n && s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') {
            break;
        }
    }

    return i < n ? s1[i] - s2[i] : 0;
}

int strcasecmp(const char* s1, const char* s2)
{
    size_t i = 0;
    for (; tolower(s1[i]) == tolower(s2[i]); i++) {
        if (s1[i] == '\0') {
            break;
        }
    }

    return tolower(s1[i]) - tolower(s2[i]);
}

char* strchr(const char* s, int c)
{
    while (*s != '\0') {
        if (*s == c) {
            // This function is also a const-cast
            return (char*) s;
        }

        s++;
    }

    return NULL;
}

char* strcpy(char* __restrict dst, const char* __restrict src)
{
    return (char*) memcpy(dst, src, strlen(src) + 1);
}

char* strncpy(char* __restrict dst, const char* __restrict src, size_t n)
{
    if (n == 0) {
        return dst;
    }

    return (char*) memcpy(dst, src, std::min<size_t>(strlen(src) + 1, n));
}
