#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pndman.h"
#include "device.h"
#include "package.h"
#include "repository.h"
#include "version.h"

/* \brief convert string to uppercase, returns number of characters converted */
static char* _upstr(char *src)
{
   int i;
   char *dst = malloc(strlen(src)+1);
   if (!dst) return NULL;
   for (i = 0; i != strlen(src); ++i)
      dst[i] = (char)toupper(src[i]);
   return dst;
}

/* \brief strstr strings in uppercase, NOTE: returns 1 on found else 0 */
int _strupstr(char *hay, char *needle)
{
   char *uphay, *upneedle; int ret = RETURN_FALSE;
   if (!(uphay    = _upstr(hay))) return RETURN_FALSE;
   if (!(upneedle = _upstr(needle))) {
      free(uphay); return RETURN_FALSE;
   }

   if (strstr(uphay, upneedle)) ret = RETURN_TRUE;
   free(uphay); free(upneedle);
   return ret;
}

/* \brief strcmp strings in uppercase, NOTE: returns 1 on found else 0 */
int _strupcmp(char *hay, char *needle)
{
   char *uphay, *upneedle; int ret = RETURN_FALSE;
   if (!(uphay    = _upstr(hay))) return RETURN_FALSE;
   if (!(upneedle = _upstr(needle))) {
      free(uphay); return RETURN_FALSE;
   }

   if (!strcmp(uphay, upneedle)) ret = RETURN_TRUE;
   free(uphay); free(upneedle);
   return ret;
}

/* \brief return temporary file */
FILE* _pndman_get_tmp_file()
{
   FILE *tmp;

#ifndef __WIN32__ /* why won't this work on windows 7 correctly :/ */
   if (!(tmp = tmpfile()))
      return NULL;
#else
   char* name;
   if (!(name = _tempnam( NULL, NULL )))
      return NULL;
   if (!(tmp = fopen(name, "wb+TD"))) {
      free(name);
      return NULL;
   }
   free(name);
#endif
   DEBUG("created temporary file");
   return tmp;
}

/* \brief strip trailing slash from string */
void _strip_slash(char *path)
{
   if (path[strlen(path)-1] == '/' ||
       path[strlen(path)-1] == '\\')
      path[strlen(path)-1] = 0;
}

/* API */

/* \brief Initializes the library and its resources. */
int pndman_init()
{
   DEBUG("pndman init");
   return RETURN_OK;
}

/* \brief Shutdowns the library cleanly, frees all the resources. */
int pndman_quit()
{
   DEBUG("pndman quit");
   return RETURN_OK;
}

/* \brief Get library version (git head) */
const char* pndman_git_head()
{
   return VERSION;
}

/* \brief Get library commit (git) */
const char* pndman_git_commit()
{
   return COMMIT;
}

/* vim: set ts=8 sw=3 tw=0 :*/
