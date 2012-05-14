#ifndef PNDMAN_PNDMAN_H
#define PNDMAN_PNDMAN_H

/* error string length */
#define PNDMAN_ERR_LEN 256

/* some predefined warns, errors */
#define _PNDMAN_WRN_BAD_USE "Bad usage of function: %s"

/* if blocked free */
#define IFREE(x) if (x) free(x);

/* debug macros, takes verbose level and printf syntax */
#define DEBUG(l,x,...) _pndman_debug_hook(__func__,l,x,##__VA_ARGS__);

/* failure functions, pndman_get_error uses these in future */
#define DEBFAIL(x,...) _pndman_set_error(x, ##__VA_ARGS__);

/* \brief appdata directory to store, temporary downland and database data */
#define PNDMAN_APPDATA "libpndman"

#ifdef __linux__
#  include <limits.h>
#endif

/* \brief currently only mingw */
#ifdef __WIN32__
#  include <windows.h>
#  include <limits.h>
#  define LINE_MAX 2048
#  ifdef _UNICODE
#     error "Suck it unicode.."
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief
 * Default return values.
 * Expections will be test functions where true condition returns 1, and false condition 0 */
typedef enum
{
   RETURN_FAIL  = -1,
   RETURN_OK    =  0,

   RETURN_TRUE  =  1,
   RETURN_FALSE =  !RETURN_TRUE
} PNDMAN_RETURN;

FILE* _pndman_get_tmp_file();
void _strip_slash(char *path);

char* _strupstr(const char *hay, const char *needle);
int _strupcmp(const char *hay, const char *needle);
int _strnupcmp(const char *hay, const char *needle, size_t len);

int pndman_get_verbose();
void _pndman_set_error(const char *err, ...);
void _pndman_debug_hook(const char *function, int verbose_level, const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_PNDMAN_H */

/* vim: set ts=8 sw=3 tw=0 :*/
