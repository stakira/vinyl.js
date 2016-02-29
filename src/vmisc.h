
#ifndef VMISC_H
#define VMISC_H

#include "vinyl.h"

void vfatal(const char* format, ...);
void verror(const char* format, ...);
void vwarning(const char* format, ...);
void vfataloom(const char* doing);

unsigned int round_up_pow2(unsigned int v);

double get_t();

#endif // VMISC_H
