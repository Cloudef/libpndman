#include "internal.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* \brief internal verbose level */
static int  _PNDMAN_VERBOSE = 0;

/* \brief internal debug hook function */
static PNDMAN_DEBUG_HOOK_FUNC _PNDMAN_DEBUG_HOOK = NULL;

/* \brief strstr strings in uppercase */
char* _strupstr(const char *hay, const char *needle)
{
   size_t i, r, p, len, len2;
   p = 0; r = 0;
   if (!_strupcmp(hay, needle)) return (char*)hay;
   if ((len = strlen(hay)) < (len2 = strlen(needle))) return NULL;
   for (i = 0; i != len; ++i) {
      if (p == len2) return (char*)&hay[r]; /* THIS IS IT! */
      if (toupper(hay[i]) == toupper(needle[p++])) {
         if (!r) r = i; /* could this be.. */
      } else { if (r) i = r; r = 0; p = 0; } /* ..nope, damn it! */
   }
   if (p == len2) return (char*)&hay[r]; /* THIS IS IT! */
   return NULL;
}

/* \brief strcmp strings in uppercase, NOTE: returns 0 on found else 1 (so you don't mess up with strcmp) */
int _strupcmp(const char *hay, const char *needle)
{
   size_t i, len;
   if ((len = strlen(hay)) != strlen(needle)) return RETURN_TRUE;
   for (i = 0; i != len; ++i)
      if (toupper(hay[i]) != toupper(needle[i])) return RETURN_TRUE;
   return RETURN_FALSE;
}

/* \brief strncmp strings in uppercase, NOTE: returns 0 on found else 1 (so you don't mess up with strcmp) */
int _strnupcmp(const char *hay, const char *needle, size_t len)
{
   size_t i;
   for (i = 0; i != len; ++i)
      if (toupper(hay[i]) != toupper(needle[i])) return RETURN_TRUE;
   return RETURN_FALSE;
}

/* \brief return temporary file */
FILE* _pndman_get_tmp_file()
{
   FILE *tmp;

#ifndef __WIN32__
   /* why won't this work on windows 7 correctly :/ */
   if (!(tmp = tmpfile()))
      goto fail;
#else
   char* name;
   if (!(name = _tempnam( NULL, NULL )))
      goto fail;
   if (!(tmp = fopen(name, "wb+TD")))
      goto fail;
   free(name);
#endif
   DEBUG(PNDMAN_LEVEL_CRAP, "Created temporary file.");
   return tmp;

fail:
   DEBFAIL(PNDMAN_TMP_FILE_FAIL);
#ifdef __WIN32__
   IFREE(name);
#endif
   return NULL;
}

/* \brief strip trailing slash from string */
void _strip_slash(char *path)
{
   if (path[strlen(path)-1] == '/' ||
       path[strlen(path)-1] == '\\')
      path[strlen(path)-1] = 0;
}

/* \brief handle debug hook for client
 * (printf syntax) */
void _pndman_debug_hook(const char *file, int line,
      const char *function, int verbose_level,
      const char *fmt, ...)
{
   va_list args;
   char buffer[LINE_MAX];

   memset(buffer, 0, LINE_MAX);
   va_start(args, fmt);
   vsnprintf(buffer, LINE_MAX, fmt, args);
   va_end(args);

   /* no hook, handle it internally */
   if (!_PNDMAN_DEBUG_HOOK) {
      printf(DBG_FMT, file, line, function, buffer);
      return;
   }

   /* pass to client */
   _PNDMAN_DEBUG_HOOK(file, line, function,
         verbose_level, buffer);
}

/* API */

/* \brief set verbose level of library
 * (prints debug output to stdout) */
PNDMANAPI void pndman_set_verbose(int verbose)
{
   _PNDMAN_VERBOSE = verbose;
}

/* \brief set debug hook function */
PNDMANAPI void pndman_set_debug_hook(
      PNDMAN_DEBUG_HOOK_FUNC func)
{
   _PNDMAN_DEBUG_HOOK = func;
}

/* \brief return current verbose level,
 * used internally by library as well. */
PNDMANAPI int pndman_get_verbose(void)
{
   return _PNDMAN_VERBOSE;
}

/* \brief get library version (git head) */
PNDMANAPI const char* pndman_git_head(void)
{
   return VERSION;
}

/* \brief get library commit (git) */
PNDMANAPI const char* pndman_git_commit(void)
{
   return COMMIT;
}

/* vim: set ts=8 sw=3 tw=0 :*/
