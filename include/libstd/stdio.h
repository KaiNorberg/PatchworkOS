#ifndef _STDIO_H
#define _STDIO_H 1

#include <stdarg.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/MAX_PATH.h"
#include "_AUX/NULL.h"
#include "_AUX/SEEK.h"
#include "_AUX/config.h"
#include "_AUX/fd_t.h"
#include "_AUX/size_t.h"

#define _IOFBF (1u << 0)
#define _IOLBF (1u << 1)
#define _IONBF (1u << 2)

// TODO: Implement streams!
typedef struct
{
    uint8_t todo;
} fpos_t;

typedef struct
{
    uint8_t todo;
} FILE;

#define EOF (-1)
#define BUFSIZ 1024
#define FOPEN_MAX 8
#define FILENAME_MAX MAX_PATH

#define L_tmpnam 46
#define TMP_MAX 50

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

_PUBLIC int remove(const char* filename);

_PUBLIC int rename(const char* oldpath, const char* newpath);

_PUBLIC FILE* tmpfile(void);

_PUBLIC char* tmpnam(char* s);

_PUBLIC int fclose(FILE* stream);

_PUBLIC int fflush(FILE* stream);

_PUBLIC FILE* fopen(const char* _RESTRICT filename, const char* _RESTRICT mode);

_PUBLIC FILE* freopen(const char* _RESTRICT filename, const char* _RESTRICT mode, FILE* _RESTRICT stream);

_PUBLIC void setbuf(FILE* _RESTRICT stream, char* _RESTRICT buf);

_PUBLIC int setvbuf(FILE* _RESTRICT stream, char* _RESTRICT buf, int mode, size_t size);

_PUBLIC int fprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);

_PUBLIC int fscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);

_PUBLIC int printf(const char* _RESTRICT format, ...);

_PUBLIC int scanf(const char* _RESTRICT format, ...);

_PUBLIC int snprintf(char* _RESTRICT s, size_t n, const char* _RESTRICT format, ...);

_PUBLIC int sprintf(char* _RESTRICT s, const char* _RESTRICT format, ...);

_PUBLIC int sscanf(const char* _RESTRICT s, const char* _RESTRICT format, ...);

_PUBLIC int vfprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);

_PUBLIC int vfscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);

_PUBLIC int vprintf(const char* _RESTRICT format, va_list arg);

_PUBLIC int vscanf(const char* _RESTRICT format, va_list arg);

_PUBLIC int vsnprintf(char* _RESTRICT s, size_t n, const char* _RESTRICT format, va_list arg);

_PUBLIC int vsprintf(char* _RESTRICT s, const char* _RESTRICT format, va_list arg);

_PUBLIC int vsscanf(const char* _RESTRICT s, const char* _RESTRICT format, va_list arg);

_PUBLIC int fgetc(FILE* stream);

_PUBLIC char* fgets(char* _RESTRICT s, int n, FILE* _RESTRICT stream);

_PUBLIC int fputc(int c, FILE* stream);

_PUBLIC int fputs(const char* _RESTRICT s, FILE* _RESTRICT stream);

_PUBLIC int getc(FILE* stream);

_PUBLIC int getchar(void);

_PUBLIC int putc(int c, FILE* stream);

_PUBLIC int putchar(int c);

_PUBLIC int puts(const char* s);

_PUBLIC int ungetc(int c, FILE* stream);

_PUBLIC size_t fread(void* _RESTRICT ptr, size_t size, size_t nmemb, FILE* _RESTRICT stream);

_PUBLIC size_t fwrite(const void* _RESTRICT ptr, size_t size, size_t nmemb, FILE* _RESTRICT stream);

_PUBLIC int fgetpos(FILE* _RESTRICT stream, fpos_t* _RESTRICT pos);

_PUBLIC int fseek(FILE* stream, long int offset, int whence);

_PUBLIC int fsetpos(FILE* stream, const fpos_t* pos);

_PUBLIC long int ftell(FILE* stream);

_PUBLIC void rewind(FILE* stream);

_PUBLIC void clearerr(FILE* stream);

_PUBLIC int feof(FILE* stream);

_PUBLIC int ferror(FILE* stream);

_PUBLIC void perror(const char* s);

#if _USE_ANNEX_K == 1

#define L_tmpnam_s L_tmpnam
#define TMP_MAX_S TMP_MAX

#include "_AUX/errno_t.h"
#include "_AUX/rsize_t.h"

_PUBLIC errno_t tmpfile_s(FILE* _RESTRICT* _RESTRICT streamptr);

_PUBLIC errno_t fopen_s(FILE* _RESTRICT* _RESTRICT streamptr, const char* _RESTRICT filename,
    const char* _RESTRICT mode);

_PUBLIC errno_t freopen_s(FILE* _RESTRICT* _RESTRICT newstreamptr, const char* _RESTRICT filename,
    const char* _RESTRICT mode, FILE* _RESTRICT stream);

_PUBLIC errno_t tmpnam_s(char* s, rsize_t maxsize);
_PUBLIC int fprintf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);
_PUBLIC int fscanf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, ...);
_PUBLIC int printf_s(const char* _RESTRICT format, ...);
_PUBLIC int scanf_s(const char* _RESTRICT format, ...);
_PUBLIC int snprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, ...);
_PUBLIC int sprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, ...);
_PUBLIC int sscanf_s(const char* _RESTRICT s, const char* _RESTRICT format, ...);
_PUBLIC int vfprintf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);
_PUBLIC int vfscanf_s(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg);
_PUBLIC int vprintf_s(const char* _RESTRICT format, va_list arg);
_PUBLIC int vscanf_s(const char* _RESTRICT format, va_list arg);
_PUBLIC int vsnprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, va_list arg);
_PUBLIC int vsprintf_s(char* _RESTRICT s, rsize_t n, const char* _RESTRICT format, va_list arg);
_PUBLIC int vsscanf_s(const char* _RESTRICT s, const char* _RESTRICT format, va_list arg);
_PUBLIC char* gets_s(char* s, rsize_t n);

#endif

#if defined(__cplusplus)
}
#endif

#endif
