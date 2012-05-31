#include "internal.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* \brief internal verbose level */
static int _PNDMAN_VERBOSE = 0;

/* \brief internal color output */
static int _PNDMAN_COLOR = 1;

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
void* _pndman_get_tmp_file()
{
   FILE *tmp;

#ifndef _WIN32
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
#ifdef _WIN32
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

 /* \brief output in red */
inline static void _pndman_red(void)
{
#ifdef __unix__
   printf("\33[31m");
#endif

#ifdef _WIN32
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_INTENSITY);
#endif
}

/* \brief output in green */
inline static void _pndman_green(void)
{
#ifdef __unix__
   printf("\33[32m");
#endif

#ifdef _WIN32
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN
   |FOREGROUND_INTENSITY);
#endif
}

/* \brief output in blue */
inline static void _pndman_blue(void)
{
#ifdef __unix__
   printf("\33[34m");
#endif

#ifdef _WIN32
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_BLUE
   |FOREGROUND_GREEN|FOREGROUND_INTENSITY);
#endif
}

/* \brief output in yellow */
inline static void _pndman_yellow(void)
{
#ifdef __unix__
   printf("\33[33m");
#endif

#ifdef _WIN32
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN
   |FOREGROUND_RED|FOREGROUND_INTENSITY);
#endif
}

/* \brief output in white */
inline static void _pndman_white(void)
{
#ifdef __unix__
   printf("\33[37m");
#endif

#ifdef _WIN32
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_GREEN|FOREGROUND_BLUE);
#endif
}

/* \brief reset output color */
inline static void _pndman_normal(void)
{
#ifdef __unix__
   printf("\33[0m");
#endif

#ifdef _WIN32
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_GREEN|FOREGROUND_BLUE);
#endif
}

/* \brief handle debug hook for client
 * (printf syntax) */
void _pndman_debug_hook(const char *file, int line,
      const char *function, int verbose_level,
      const char *fmt, ...)
{
   va_list args;
   char buffer[LINE_MAX];
   char buffer2[LINE_MAX];

   memset(buffer, 0, LINE_MAX);
   va_start(args, fmt);
   vsnprintf(buffer, LINE_MAX-1, fmt, args);
   va_end(args);

   /* no hook, handle it internally */
   if (!_PNDMAN_DEBUG_HOOK) {
      memset(buffer2, 0, LINE_MAX);
      snprintf(buffer2, LINE_MAX-1, DBG_FMT,
            file, line, function, buffer);
      pndman_puts(buffer2);
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

/* \brief colored put function
 * this is manily provided public to milkyhelper,
 * to avoid some code duplication.
 *
 * Plus pndman uses it internally, if no debug hooks set :)
 * Feel free to use it as well if you want. */
PNDMANAPI void pndman_puts(const char *buffer)
{
   int i;
   size_t len;

   /* no color output */
   if (!_PNDMAN_COLOR) {
      puts(buffer);
      return;
   }

   len = strlen(buffer);
   for (i = 0; i != len; ++i) {
           if (buffer[i] == '\1') _pndman_red();
      else if (buffer[i] == '\2') _pndman_green();
      else if (buffer[i] == '\3') _pndman_blue();
      else if (buffer[i] == '\4') _pndman_yellow();
      else if (buffer[i] == '\5') _pndman_white();
      else printf("%c", buffer[i]);
   }
   _pndman_normal();
   printf("\n");
   fflush(stdout);
}

/* \brief use this to disable internal color output */
PNDMANAPI void pndman_set_color(int use_color)
{
   if (!(_PNDMAN_COLOR = use_color))
      _pndman_normal();
}

/* vim: set ts=8 sw=3 tw=0 :*/
