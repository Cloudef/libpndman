#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
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

/* \brief helper int setter */
static int _json_set_number(double *i, json_t *object)
{
   if (!object) return RETURN_FAIL;
   *i = json_number_value(object);
   return RETURN_OK;
}

/* \brief helper version setter */
static int _json_set_version(pndman_version *ver, json_t *object)
{
   char type[PND_VER];
   if (!object) return RETURN_FAIL;
   _json_set_string(ver->major,     json_object_get(object,"major"),    PND_VER);
   _json_set_string(ver->minor,     json_object_get(object,"minor"),    PND_VER);
   _json_set_string(ver->release,   json_object_get(object,"release"),  PND_VER);
   _json_set_string(ver->build,     json_object_get(object,"build"),    PND_VER);
   _json_set_string(type,           json_object_get(object,"type"),     PND_VER);
   ver->type = !strcmp(type,PND_TYPE_BETA)  ? PND_VERSION_BETA  :
               !strcmp(type,PND_TYPE_ALPHA) ? PND_VERSION_ALPHA : PND_VERSION_RELEASE;
   return RETURN_OK;
}

/* \brief helper author setter */
static int _json_set_author(pndman_author *author, json_t *object)
{
   if (!object) return RETURN_FAIL;
   _json_set_string(author->name,      json_object_get(object,"name"),     PND_NAME);
   _json_set_string(author->website,   json_object_get(object,"website"),  PND_STR);
   _json_set_string(author->email,     json_object_get(object,"email"),    PND_STR);
   return RETURN_OK;
}

/* \brief helper previewpic setter */
static int _json_set_previewpics(pndman_package *pnd, json_t *object)
{
   pndman_previewpic *c;
   json_t *element;
   unsigned int p;
   char src[PND_PATH];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;

   memset(src, 0, PND_PATH);
   for (p = 0; p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(src, element, PND_PATH) != RETURN_OK)
         continue;

      /* copy */
      if ((c = _pndman_package_new_previewpic(pnd)))
         memcpy(c->src, src, PND_PATH);
   }

   return RETURN_OK;
}

/* \brief helper license setter */
static int _json_set_licenses(pndman_package *pnd, json_t *object)
{
   pndman_license *l;
   json_t *element;
   unsigned int p;
   char name[PND_SHRT_STR];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;

   memset(name, 0, PND_SHRT_STR);
   for (p = 0; p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(name, element, PND_SHRT_STR) != RETURN_OK)
         continue;

      /* copy */
      if ((l = _pndman_package_new_license(pnd)))
         memcpy(l->name, name, PND_SHRT_STR);
   }

   return RETURN_OK;
}

/* \brief helper license source setter (licenses must be set first!) */
static int _json_set_sources(pndman_package *pnd, json_t *object)
{
   pndman_license *l;
   json_t *element;
   unsigned int p;
   char url[PND_STR];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;
   if (!(l = pnd->license))      return RETURN_FAIL;

   memset(url, 0, PND_STR);
   for (p = 0; l && p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(url, element, PND_STR) != RETURN_OK) {
         l = l->next;
         continue;
      }

      /* copy */
      memcpy(l->sourcecodeurl, url, PND_STR);
      l = l->next;
   }

   return RETURN_OK;
}

/* \brief helper category setter */
static int _json_set_categories(pndman_package *pnd, json_t *object)
{
   pndman_category *c = NULL;
   json_t *element;
   unsigned int p;
   char cat[PND_SHRT_STR];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;

   memset(cat, 0, PND_SHRT_STR);
   for (p = 0; p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(cat, element, PND_SHRT_STR) != RETURN_OK)
         continue;

      /* copy either main or sub */
      if (!c) {
         if ((c = _pndman_package_new_category(pnd))) {
            memcpy(c->main, cat, PND_SHRT_STR);
         } else break;
      } else {
         memcpy(c->sub, cat, PND_SHRT_STR);
         c = NULL;
      }
   }

   return RETURN_OK;
}

/* \brief helper localization setter */
static int _json_set_localization(pndman_package *pnd, json_t *object)
{
   const char *key;
   json_t *element;
   void *iter;
   pndman_translated *t;

   if (!object) return RETURN_FAIL;
   iter = json_object_iter(object);
   while (iter) {
      key      = json_object_iter_key(iter);
      element  = json_object_iter_value(iter);

      /* bad element? */
      if (!element) {
         iter = json_object_iter_next(object, iter);
         continue;
      }

      /* add title */
      if ((t = _pndman_package_new_title(pnd))) {
            strncpy(t->lang, key, PND_SHRT_STR);
            _json_set_string(t->string, json_object_get(element, "title"), PND_STR);
      }

      /* add description */
      if ((t = _pndman_package_new_description(pnd))) {
            strncpy(t->lang, key, PND_SHRT_STR);
            _json_set_string(t->string, json_object_get(element, "description"), PND_STR);
      }

      /* next */
      iter = json_object_iter_next(object, iter);
   }

   return RETURN_OK;
}

/* \brief json parse repository header */
static int _pndman_json_repo_header(json_t *repo_header, pndman_repository *repo)
{
   json_t *element;
   assert(repo_header && repo);

   _json_set_string(repo->name,    json_object_get(repo_header, "name"), REPO_NAME);
   _json_set_string(repo->updates, json_object_get(repo_header, "updates"), REPO_URL);
   if ((element = json_object_get(repo_header, "version")))
      if (json_is_string(element))
         strncpy(repo->version, json_string_value(element), REPO_VERSION);
      else
         snprintf(repo->version, REPO_VERSION-1, "%.2f", json_number_value(element));
   _json_set_number((double*)&repo->timestamp, json_object_get(repo_header, "timestamp"));

   return RETURN_OK;
}

/* \brief json parse repository packages */
static int _pndman_json_process_packages(json_t *packages, pndman_repository *repo)
{
   json_t         *package;
   pndman_package *pnd;
   unsigned int p;
   char id[PND_ID];
   char path[PND_PATH];
   assert(packages && repo);

   memset(id,   0, PND_ID);
   memset(path, 0, PND_PATH);

   p = 0;
   for (; p != json_array_size(packages); ++p) {
      package = json_array_get(packages, p);
      if (!json_is_object(package)) continue;

      /* these are needed for checking duplicate pnd's */
      _json_set_string(id,   json_object_get(package,"id"),    PND_ID);
      _json_set_string(path, json_object_get(package, "path"), PND_PATH);
      pnd = _pndman_repository_new_pnd_check(id, path, repo);
      if (!pnd) return RETURN_FAIL;

      /* free old titles and descriptions (if instance or old) */
      _pndman_package_free_titles(pnd);
      _pndman_package_free_descriptions(pnd);
      _pndman_package_free_previewpics(pnd);
      _pndman_package_free_licenses(pnd);
      _pndman_package_free_categories(pnd);

      memcpy(pnd->id,     id,  PND_ID);
      memcpy(pnd->path, path,  PND_PATH);
      _json_set_string(pnd->md5,          json_object_get(package,"md5"),     PND_STR);
      _json_set_string(pnd->url,          json_object_get(package,"uri"),     PND_STR);
      _json_set_version(&pnd->version,    json_object_get(package,"version"));
      _json_set_localization(pnd,         json_object_get(package,"localizations"));
      _json_set_string(pnd->info,         json_object_get(package,"info"),    PND_INFO);
      _json_set_number((double*)&pnd->size, json_object_get(package, "size"));
      _json_set_number((double*)&pnd->modified_time, json_object_get(package, "modified_time"));
      _json_set_number((double*)&pnd->rating, json_object_get(package, "rating"));
      _json_set_author(&pnd->author,      json_object_get(package,"author"));
      _json_set_string(pnd->vendor,       json_object_get(package,"vendor"),  PND_NAME);
      _json_set_string(pnd->icon,         json_object_get(package,"icon"),    PND_PATH);
      _json_set_previewpics(pnd,          json_object_get(package,"previewpics"));
      _json_set_licenses(pnd,             json_object_get(package,"licenses"));
      _json_set_sources(pnd,              json_object_get(package,"source"));
      _json_set_categories(pnd,           json_object_get(package,"categories"));
   }
   return RETURN_OK;
}

/* INTERNAL */

/* \brief process retivied json data */
int _pndman_json_process(pndman_repository *repo, FILE *data)
{
   json_t *root, *repo_header, *packages;
   json_error_t error;
   assert(repo && data);

   /* flush and reset to beginning */
   fflush(data);
   fseek(data, 0L, SEEK_SET);
   root = json_loadf(data, 0, &error);
   if (!root) {
      DEBUGP("WARN: Bad json data, won't process sync for: %s\n", repo->url);
      return RETURN_FAIL;
   }

   repo_header = json_object_get(root, "repository");
   if (json_is_object(repo_header)) {
      if (_pndman_json_repo_header(repo_header, repo) == RETURN_OK) {
         packages = json_object_get(root, "packages");
         if (json_is_array(packages))
            _pndman_json_process_packages(packages, repo);
         else DEBUGP("WARN: No packages array for: %s\n", repo->url);
      }
   } else DEBUGP("WARN: Bad repo header for: %s\n", repo->url);
   json_decref(root);

   /* update timestamp */
   repo->timestamp = time(0);

   return RETURN_OK;
}

/* \brief print to file with escapes */
static void _cfprintf(FILE *f, char *str)
{
   assert(f && str);
   int len, i;
   if (!(len = strlen(str))) return;
   for (i = 0; i != len; ++i) {
      if (str[i] == '\\')
         fprintf(f, "\\%c", '\\');
      else if (str[i] == '\n')
         fprintf(f, "\\n");
      else if (str[i] == '\r')
         fprintf(f, "\\r");
      else if (str[i] == '\t')
         fprintf(f, "\\t");
      else if (str[i] == '"')
         fprintf(f, "\\\"");
      else
         fprintf(f, "%c", str[i]);
   }
}

/* \brief print json key to file */
static void _fkeyf(FILE *f, char *key, char *value, int delim)
{
   assert(f && key && value);
   fprintf(f, "\"%s\":\"", key);
   _cfprintf(f, value);
   fprintf(f, "\"%s", delim ? "," : "");
}

/* \brief print json string to file */
static void _fstrf(FILE *f, char *value, int delim)
{
   assert(f && value);
   fprintf(f, "\"");
   _cfprintf(f, value);
   fprintf(f, "\"%s", delim ? "," : "");
}

/* \brief outputs json for repository */
int _pndman_json_commit(pndman_repository *r, FILE *f)
{
   pndman_package *p;
   pndman_translated *t, *d;
   pndman_previewpic *pic;
   pndman_category *c;
   pndman_license *l;
   int found = 0;
   assert(f && r);

   fprintf(f, "{"); /* start */
   fprintf(f, "\"repository\":{\"name\":\"%s\",\"version\":\"%s\",\"updates\":\"%s\",\"timestamp\":%zu},",
         r->name, r->version, r->updates, r->timestamp);
   fprintf(f, "\"packages\":["); /* packages */
   for (p = r->pnd; p; p = p->next) {
      fprintf(f, "{");

      /* local repository */
      if (!r->prev) {
         _fkeyf(f, "path", p->path, 1);
         fprintf(f, "\"flags\":%d,", p->flags);
      }

      _fkeyf(f, "id", p->id, 1);
      _fkeyf(f, "uri", p->url, 1);

      /* version object */
      fprintf(f, "\"version\":{");
      _fkeyf(f, "major", p->version.major, 1);
      _fkeyf(f, "minor", p->version.minor, 1);
      _fkeyf(f, "release", p->version.release, 1);
      _fkeyf(f, "build", p->version.build, 1);
      _fkeyf(f, "type",
            p->version.type == PND_VERSION_BETA    ? "beta"    :
            p->version.type == PND_VERSION_ALPHA   ? "alpha"   : "release", 0);
      fprintf(f, "},");

      /* localization object */
      fprintf(f, "\"localizations\":{");
      for (t = p->title; t; t = t->next) {
         found = 0;
         for (d = p->description; d ; d = d->next)
            if (!strcmp(d->lang, t->lang)) {
               found = 1;
               break;
            }

         fprintf(f, "\"%s\":{", t->lang);
         _fkeyf(f, "title", t->string, 1);
         _fkeyf(f, "description", found ? d->string : "", 0);
         fprintf(f, "}%s", t->next ? "," : "");
      }

      /* fallback */
      if (!p->title)
         fprintf(f, "\"en_US\":{\"title\":\"\",\"description\":\"\"}");
      fprintf(f, "},");

      _fkeyf(f, "info", p->info, 1);

      fprintf(f, "\"size\":\"%zu\",", p->size);
      _fkeyf(f, "md5", p->md5, 1);
      fprintf(f, "\"modified-time\":\"%zu\",", p->modified_time);
      fprintf(f, "\"rating\":\"%d\",", p->rating);

      /* author object */
      fprintf(f, "\"author\":");
      fprintf(f, "{");
      _fkeyf(f, "name", p->author.name, 1);
      _fkeyf(f, "website", p->author.website, 1);
      _fkeyf(f, "email", p->author.email, 0);
      fprintf(f, "},");

      _fkeyf(f, "vendor", p->vendor, 1);
      _fkeyf(f, "icon", p->icon, 1);

      /* previewpics array */
      fprintf(f, "\"previewpics\":[");
      for (pic = p->previewpic; pic; pic = pic->next)
         _fstrf(f, pic->src, pic->next ? 1 : 0);
      fprintf(f, "],");

      /* licenses array */
      fprintf(f, "\"licenses\":[");
      for (l = p->license; l; l = l->next)
         _fstrf(f, l->name, l->next ? 1 : 0);
      fprintf(f, "],");

      /* sources array */
      fprintf(f, "\"source\":[");
      for (l = p->license; l; l = l->next)
         _fstrf(f, l->sourcecodeurl, l->next ? 1 : 0);
      fprintf(f, "],");

      /* categories array */
      fprintf(f, "\"categories\":[");
      for (c = p->category; c; c = c->next) {
         _fstrf(f, c->main, 1);
         _fstrf(f, c->sub, c->next ? 1 : 0);
      }
      fprintf(f, "]");
      fprintf(f, "}%s", p->next ? "," : "");
   }
   fprintf(f, "]}\n"); /* end */

   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
