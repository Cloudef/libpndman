#ifndef PNDMAN_PNDMAN_H
#define PNDMAN_PNDMAN_H

#define DEBUG(x)      puts(x)
#define DEBUGP(x,...) printf(x,##__VA_ARGS__)

#ifdef __linux__
#  include <limits.h>
#endif

/* Currently only mingw */
#ifdef __WIN32__
#  include <windows.h>
#  include <limits.h>
#  define LINE_MAX 256
#  ifdef _UNICODE
#     error "Suck it unicde.."
#  endif
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

#endif /* PNDMAN_PNDMAN_H */

/* vim: set ts=8 sw=3 tw=0 :*/
