
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "vmisc.h"

void vfatal(const char* format, ...) {
	va_list args;
	va_start(args, format);
	printf("fatal: ");
	vprintf(format, args);
	printf("\n");
	va_end(args);
	exit(1);
}

void verror(const char* format, ...) {
	va_list args;
	va_start(args, format);
	printf("error: ");
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

void vwarning(const char* format, ...) {
	va_list args;
	va_start(args, format);
	printf("warning: ");
	vprintf(format, args);
	va_end(args);
}

void vfataloom(const char* doing) {
	vfatal("out of memory while %s", doing);
}

unsigned int round_up_pow2(unsigned int v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v <= 0 ? 1 : v;
}

#ifdef WIN32

#include <windows.h>

double get_t()
{
	LARGE_INTEGER t, f;
	QueryPerformanceCounter(&t);
	QueryPerformanceFrequency(&f);
	return (double)t.QuadPart / (double)f.QuadPart;
}

#else

#include <sys/time.h>
#include <sys/resource.h>

double get_t()
{
	struct timeval t;
	struct timezone tzp;
	gettimeofday(&t, &tzp);
	return t.tv_sec + t.tv_usec*1e-6;
}

#endif
