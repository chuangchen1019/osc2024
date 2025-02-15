#ifndef _UTILS_H_
#define _UTILS_H_

#define VSPRINT_MAX_BUF_SIZE 0x100
#include <stddef.h>

unsigned int sprintf(char *dst, char* fmt, ...);
unsigned int vsprintf(char *dst,char* fmt, __builtin_va_list args);

unsigned long long strlen(const char *str);
int                strcmp(const char*, const char*);
int                strncmp(const char*, const char*, unsigned long long);
char*              memcpy(void *dest, const void *src, unsigned long long len);
char*              strcpy(char *dest, const char *src);
char*              strcat (char *dest, const char *src);
void*              memset(void *s, int c, size_t n);

char* str_SepbySpace(char* head);
int   atoi(char* str);
#endif /* _UTILS_H_ */
