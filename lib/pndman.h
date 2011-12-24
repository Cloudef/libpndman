#ifndef PNDMAN_PNDMAN_H
#define PNDMAN_PNDMAN_H

#define DEBUG(x)      puts(x)
#define DEBUGP(x,...) printf(x,##__VA_ARGS__)

#ifdef __linux__
#  include <limits.h>
#endif

#ifdef __WIN32__
#  define WINVER 0x0500
#  include <windows.h>
#  define PATH_MAX MAX_PATH
#  define LINE_MAX 256
#  ifdef _UNICODE
#     error "Suck it unicde.."
#  endif
#endif

/*! \brief
 * Default return values.\n
 * Expections will be test functions where true condition returns 1, and false condition 0 */
typedef enum
{
   RETURN_FAIL  = -1,
   RETURN_OK    =  0,

   RETURN_TRUE  =  1,
   RETURN_FALSE =  !RETURN_TRUE
} PNDMAN_RETURN;

#endif /* PNDMAN_PNDMAN_H */
