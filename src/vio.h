
#ifndef VIO_H
#define VIO_H

#include <stdio.h>

typedef struct VFile VFile;

VFile* v_open(const char* filename, const char* mode);
VFile* v_filefromcstr(const char* s);
int v_close(VFile * file);
int v_getc(VFile* file);
int v_ungetc(int c, VFile * file);

#endif // VIO_H
