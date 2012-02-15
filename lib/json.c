#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <jansson.h>

#include "pndman.h"
#include "package.h"
#include "repository.h"

/* \brief helpfer string setter */
static int _json_set_string(char *string, json_t *object, size_t max)
{
   if (!object) return RETURN_FAIL;
   strncpy(string, json_string_value(object), max-1);
   return RETURN_OK;
}

/* \brief json parse repository header */
static int _pndman_json_repo_header(json_t *repo_header, pndman_repository *repo)
{
   json_t *element;
   assert(repo_header);
   assert(repo);

   _json_set_string(repo->name,    json_object_get(repo_header, "name"), REPO_NAME-1);
   _json_set_string(repo->updates, json_object_get(repo_header, "updates"), REPO_URL-1);
   if ((element = json_object_get(repo_header, "version")))
      if (json_is_string(element))
         strncpy(repo->version, json_string_value(element), REPO_VERSION-1);
      else
         snprintf(repo->version, REPO_VERSION-1, "%.2f", json_number_value(element));

   return RETURN_OK;
}

/* \brief json parse repository packages */
static int _pndman_json_process_packages(json_t *packages, pndman_repository *repo)
{
   json_t         *package;
   pndman_package *pnd;
   unsigned int p;

   p = 0;
   for (; p != json_array_size(packages); ++p) {
      package = json_array_get(packages, p);
      if (!json_is_object(package)) return RETURN_FAIL;

      /* INFO READ HERE */
      pnd = _pndman_repository_new_pnd(repo);
      if (!pnd) return RETURN_FAIL;
      _json_set_string(pnd->id,     json_object_get(package,"id"),     PND_ID);
      _json_set_string(pnd->icon,   json_object_get(package,"icon"),   PND_PATH);
      _json_set_string(pnd->md5,    json_object_get(package,"md5"),    PND_MD5);
      _json_set_string(pnd->vendor, json_object_get(package,"vendor"), PND_NAME);
   }
   return RETURN_OK;
}

/* INTERNAL */

/* \brief process retivied json data */
int _pndman_json_process(pndman_repository *repo, FILE *data)
{
   json_t *root, *repo_header, *packages;
   json_error_t error;

   /* flush and reset to beginning */
   fflush(data);
   fseek(data, 0L, SEEK_SET);
   root = json_loadf(data, 0, &error);
   if (!root) {
      printf("WARN: Bad json data, won't process sync for: %s\n", repo->url);
      return RETURN_FAIL;
   }

   repo_header = json_object_get(root, "repository");
   if (json_is_object(repo_header)) {
      if (_pndman_json_repo_header(repo_header, repo) == RETURN_OK) {
         packages = json_object_get(root, "packages");
         if (json_is_array(packages))
            _pndman_json_process_packages(packages, repo);
         else printf("WARN: No packages array for: %s\n", repo->url);
      }
   } else printf("WARN: Bad repo header for: %s\n", repo->url);

   json_decref(root);
   return RETURN_OK;
}

/* \brief outputs json for repository */
int _pndman_json_commit(pndman_repository *r, FILE *f)
{
   pndman_package *p;

   fprintf(f, "{"); /* start */
   fprintf(f, "\"repository\":{\"name\":\"%s\",\"version\":\"%s\",\"updates\":\"%s\"},",
         r->name, r->version, r->updates);
   fprintf(f, "\"packages\":["); /* packages */
   for (p = r->pnd; p; p = p->next) {
      fprintf(f, "{");
      fprintf(f, "\"id\":\"%s\"", p->id);
      fprintf(f, "}");
      if (p->next) fprintf(f, ",");
   }
   fprintf(f, "]}\n"); /* end */

   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
