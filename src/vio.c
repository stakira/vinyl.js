
#include <stdio.h>
#include "vinyl.h"
#include "vio.h"

struct VFile {
	int mode;
	FILE* cfile;
	char* cstr;
	int cstr_pos;
};

VFile* v_open(const char* filename, const char* mode) {
	VFile* file = (VFile*)malloc(sizeof(VFile));
	file->mode = 0;
	file->cfile = fopen(filename, mode);
	return file;
}

VFile * v_filefromcstr(const char * s)
{
	VFile* file = (VFile*)malloc(sizeof(VFile));
	file->mode = 1;
	file->cstr = strdup(s);
	file->cstr_pos = 0;
	return file;
}

int v_close(VFile* file) {
	if (file->mode == 0) {
		int ret = fclose(file->cfile);
		free(file);
		return ret;
	}
	if (file->mode == 1) {
		free(file->cstr);
		free(file);
		return 0;
	}
	else {
		return 0;
	}
}

int v_getc(VFile* file) {
	if (file->mode == 0) {
		return fgetc(file->cfile);
	}
	if (file->mode == 1) {
		int ret = file->cstr[file->cstr_pos];
		if (ret == 0) {
			return EOF;
		}
		else {
			file->cstr_pos++;
			return ret;
		}
	}
	else return 0;
}

int v_ungetc(int c, VFile* file) {
	if (file->mode == 0) {
		return ungetc(c, file->cfile);
	}
	if (file->mode == 1) {
		if (file->cstr_pos == 0) return EOF;
		else return file->cstr[file->cstr_pos--];
	}
	else return 0;
}
