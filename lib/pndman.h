#ifndef PNDMAN_PNDMAN_H
#define PNDMAN_PNDMAN_H

#define DEBUG(x)        puts(x)
#define DEBUGP(x,...)   printf(x,##__VA_ARGS__)
#define DEBFAIL(x)      { DEBUG(x); return EXIT_FAILURE; }
#define DEBFAILP(x,...) { printf(x,##__VA_ARGS__); return EXIT_FAILURE; }

/* \brief appdata directory to store, temporary downland and database data */
#define PNDMAN_APPDATA "libpndman"

#ifdef __linux__
#  include <limits.h>
#endif

/* \brief currently only mingw */
#ifdef __WIN32__
#  include <windows.h>
#  include <limits.h>
#  define LINE_MAX 256
#  ifdef _UNICODE
#     error "Suck it unicde.."
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

int _strupstr(char *hay, char *needle);
int _strupcmp(char *hay, char *needle);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_PNDMAN_H */

/* vim: set ts=8 sw=3 tw=0 :*/
