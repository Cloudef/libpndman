#include <stdio.h>
#include <stdlib.h>
#include "pndman.h"
#include "device.h"
#include "package.h"
#include "repository.h"
#include "version.h"

/* \brief return temporary file */
FILE* _pndman_get_tmp_file()
{
   DEBUG("creating temporary file");
   FILE *tmp;
   if ((tmp = tmpfile()))
      return tmp;
   return NULL;
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
