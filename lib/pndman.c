#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pndman.h"
#include "device.h"
#include "package.h"
#include "repository.h"
#include "version.h"

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

/*! \brief Read repositories from configuration which lies on each device */
int pndman_read_repositories_from(pndman_device *device, pndman_repository *repo)
{
   DEBUG("pndman read repositories from");
   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
