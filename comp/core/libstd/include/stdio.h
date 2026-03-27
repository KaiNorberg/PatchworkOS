#ifndef _STDIO_H
#define _STDIO_H 1

#include <stdarg.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/MAX_PATH.h"
#include "_libstd/NULL.h"
#include "_libstd/config.h"
#include "_libstd/size_t.h"

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF (1u << 0)
#define _IOLBF (1u << 1)
#define _IONBF (1u << 2)

typedef struct fpos fpos_t;
typedef struct FILE FILE;

#define EOF (-1)
#define BUFSIZ 1024
#define FOPEN_MAX 8
#define FILENAME_MAX MAX_PATH

#define L_tmpnam 46
#define TMP_MAX 50

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int remove(const char* filename);

int rename(const char* oldpath, const char* newpath);

FILE* tmpfile(void);

char* tmpnam(char* s);

int fclose(FILE* stream);

int fflush(FILE* stream);

FILE* fopen(const char* _RESTRICT filename, const char* _RESTRICT mode);

FILE* freopen(const char* _RESTRICT filename, const char* _RESTRICT mode, FILE* _RESTRICT stream);

void setbuf(FILE* _RESTRICT stream, char* _RESTRICT buf);

int setvbuf(FILE* _RESTRICT stream, char* _RESTRICT buf, int mode, size_t size);

int fprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);

int fscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);

int printf(const char* _RESTRICT format, ...);

int scanf(const char* _RESTRICT format, ...);

int snprintf(char* _RESTRICT s, size_t n, const char* _RESTRICT format, ...);

int sprintf(char* _RESTRICT s, const char* _RESTRICT format, ...);

int sscanf(const char* _RESTRICT s, const char* _RESTRICT format, ...);

int vfprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);

int vfscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);

int vprintf(const char* _RESTRICT format, va_list arg);

int vscanf(const char* _RESTRICT format, va_list arg);

int vsnprintf(char* _RESTRICT s, size_t n, const char* _RESTRICT format, va_list arg);

int vsprintf(char* _RESTRICT s, const char* _RESTRICT format, va_list arg);

int vsscanf(const char* _RESTRICT s, const char* _RESTRICT format, va_list arg);

int fgetc(FILE* stream);

char* fgets(char* _RESTRICT s, int n, FILE* _RESTRICT stream);

int fputc(int c, FILE* stream);

int fputs(const char* _RESTRICT s, FILE* _RESTRICT stream);

int getc(FILE* stream);

int getchar(void);

int putc(int c, FILE* stream);

int putchar(int c);

int puts(const char* s);

int ungetc(int c, FILE* stream);

size_t fread(void* _RESTRICT ptr, size_t size, size_t nmemb, FILE* _RESTRICT stream);

size_t fwrite(const void* _RESTRICT ptr, size_t size, size_t nmemb, FILE* _RESTRICT stream);

int fgetpos(FILE* _RESTRICT stream, fpos_t* _RESTRICT pos);

int fseek(FILE* stream, long int offset, int whence);

int fsetpos(FILE* stream, const fpos_t* pos);

long int ftell(FILE* stream);

void rewind(FILE* stream);

void clearerr(FILE* stream);

int feof(FILE* stream);

int ferror(FILE* stream);

void perror(const char* s);

#if _USE_ANNEX_K == 1

#define L_tmpnam_s L_tmpnam
#define TMP_MAX_S TMP_MAX

#include "_libstd/errno_t.h"
#include "_libstd/rsize_t.h"

errno_t tmpfile_s(FILE * _RESTRICT * _RESTRICT streamptr);

errno_t fopen_s(FILE * _RESTRICT * _RESTRICT streamptr, const char* _RESTRICT filename, const char* _RESTRICT mode);

errno_t freopen_s(FILE * _RESTRICT * _RESTRICT newstreamptr, const char* _RESTRICT filename, const char* _RESTRICT mode,
    FILE* _RESTRICT stream);

errno_t tmpnam_s(char* s, rsize_t maxsize);
int fprintf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);
int fscanf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);
int printf_s(const char* _RESTRICT format, ...);
int scanf_s(const char* _RESTRICT format, ...);
int snprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, ...);
int sprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, ...);
int sscanf_s(const char* _RESTRICT s, const char* _RESTRICT format, ...);
int vfprintf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);
int vfscanf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);
int vprintf_s(const char* _RESTRICT format, va_list arg);
int vscanf_s(const char* _RESTRICT format, va_list arg);
int vsnprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, va_list arg);
int vsprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, va_list arg);
int vsscanf_s(const char* _RESTRICT s, const char* _RESTRICT format, va_list arg);
char* gets_s(char* s, rsize_t n);

#endif

#if defined(__cplusplus)
}
#endif

#endif
