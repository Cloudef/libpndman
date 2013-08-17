#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <jansson.h>

/* \brief helpfer string setter */
static int _json_set_string(char *string, json_t *object, size_t max)
{
   if (!object) return RETURN_FAIL;
   strncpy(string, json_string_value(object), max-1);
   return RETURN_OK;
}

/* \brief helper int setter */
#define  _json_set_number(i, object, type) \
         if (object) *i = (type)json_number_value(object);

/* \brief helper version setter */
static int _json_set_version(pndman_version *ver, json_t *object)
{
   char type[PNDMAN_VERSION];
   if (!object) return RETURN_FAIL;
   memset(type, 0, PNDMAN_VERSION);
   _json_set_string(ver->major,     json_object_get(object,"major"),    PNDMAN_VERSION);
   _json_set_string(ver->minor,     json_object_get(object,"minor"),    PNDMAN_VERSION);
   _json_set_string(ver->release,   json_object_get(object,"release"),  PNDMAN_VERSION);
   _json_set_string(ver->build,     json_object_get(object,"build"),    PNDMAN_VERSION);
   _json_set_string(type,           json_object_get(object,"type"),     PNDMAN_VERSION);
   ver->type = !_strupcmp(type,PND_TYPE_BETA)  ? PND_VERSION_BETA  :
               !_strupcmp(type,PND_TYPE_ALPHA) ? PND_VERSION_ALPHA : PND_VERSION_RELEASE;
   return RETURN_OK;
}

/* \brief helper author setter */
static int _json_set_author(pndman_author *author, json_t *object)
{
   if (!object) return RETURN_FAIL;
   _json_set_string(author->name,      json_object_get(object,"name"),     PNDMAN_NAME);
   _json_set_string(author->website,   json_object_get(object,"website"),  PNDMAN_STR);
   _json_set_string(author->email,     json_object_get(object,"email"),    PNDMAN_STR);
   return RETURN_OK;
}

/* \brief helper previewpic setter */
static int _json_set_previewpics(pndman_package *pnd, json_t *object)
{
   pndman_previewpic *c;
   json_t *element;
   unsigned int p;
   char src[PNDMAN_PATH];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;

   memset(src, 0, PNDMAN_PATH);
   for (p = 0; p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(src, element, PNDMAN_PATH) != RETURN_OK)
         continue;

      /* copy */
      if ((c = _pndman_package_new_previewpic(pnd)))
         memcpy(c->src, src, PNDMAN_PATH);
   }

   return RETURN_OK;
}

/* \brief helper license setter */
static int _json_set_licenses(pndman_package *pnd, json_t *object)
{
   pndman_license *l;
   json_t *element;
   unsigned int p;
   char name[PNDMAN_SHRT_STR];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;

   memset(name, 0, PNDMAN_SHRT_STR);
   for (p = 0; p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(name, element, PNDMAN_SHRT_STR) != RETURN_OK)
         continue;

      /* copy */
      if ((l = _pndman_package_new_license(pnd)))
         memcpy(l->name, name, PNDMAN_SHRT_STR);
   }

   return RETURN_OK;
}

/* \brief helper license source setter (licenses must be set first!) */
static int _json_set_sources(pndman_package *pnd, json_t *object)
{
   pndman_license *l;
   json_t *element;
   unsigned int p;
   char url[PNDMAN_STR];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;
   if (!(l = pnd->license))      return RETURN_FAIL;

   memset(url, 0, PNDMAN_STR);
   for (p = 0; l && p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(url, element, PNDMAN_STR) != RETURN_OK) {
         l = l->next;
         continue;
      }

      /* copy */
      memcpy(l->sourcecodeurl, url, PNDMAN_STR);
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
   char cat[PNDMAN_SHRT_STR];

   if (!object)                  return RETURN_FAIL;
   if (!json_is_array(object))   return RETURN_FAIL;

   memset(cat, 0, PNDMAN_SHRT_STR);
   for (p = 0; p != json_array_size(object); ++p) {
      element = json_array_get(object, p);

      /* check fail */
      if (_json_set_string(cat, element, PNDMAN_SHRT_STR) != RETURN_OK)
         continue;

      /* copy either main or sub */
      if (!c) {
         if ((c = _pndman_package_new_category(pnd))) {
            memcpy(c->main, cat, PNDMAN_SHRT_STR);
         } else break;
      } else {
         memcpy(c->sub, cat, PNDMAN_SHRT_STR);
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
            strncpy(t->lang, key, PNDMAN_SHRT_STR);
            _json_set_string(t->string, json_object_get(element, "title"), PNDMAN_STR);
      }

      /* add description */
      if ((t = _pndman_package_new_description(pnd))) {
            strncpy(t->lang, key, PNDMAN_SHRT_STR);
            _json_set_string(t->string, json_object_get(element, "description"), PNDMAN_STR);
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

   _json_set_string(repo->name,    json_object_get(repo_header, "name"), PNDMAN_NAME);
   _json_set_string(repo->updates, json_object_get(repo_header, "updates"), PNDMAN_URL);
   _json_set_string(repo->api.root, json_object_get(repo_header, "client_api"), PNDMAN_URL);
   _json_set_string(repo->api.username, json_object_get(repo_header, "username"), PNDMAN_SHRT_STR);
   _json_set_string(repo->api.key, json_object_get(repo_header, "api_key"), PNDMAN_STR);

   /* save next time too */
   if (strlen(repo->api.username) &&
       strlen(repo->api.key))
      repo->api.store_credentials = 1;

   _strip_slash(repo->updates);
   _strip_slash(repo->api.root);

   if ((element = json_object_get(repo_header, "version"))) {
      if (json_is_string(element)) {
         strncpy(repo->version, json_string_value(element), PNDMAN_VERSION);
      } else {
         snprintf(repo->version, PNDMAN_VERSION-1, "%.2f", json_number_value(element));
      }
   }
   _json_set_number(&repo->timestamp, json_object_get(repo_header, "last_updated"), time_t);
   _json_set_number(&repo->timestamp, json_object_get(repo_header, "timestamp"), time_t);

   return RETURN_OK;
}

/* \brief json parse repository packages */
static int _pndman_json_process_packages(json_t *packages, pndman_repository *repo,
      pndman_device *device)
{
   json_t         *package;
   pndman_package *pnd, tmp;
   unsigned int p;
   assert(packages && repo);

   p = 0;
   for (; p != json_array_size(packages); ++p) {
      package = json_array_get(packages, p);
      if (!json_is_object(package)) continue;

      /* init temporary pnd */
      memset(&tmp, 0, sizeof(pndman_package));

      /* these are needed for checking duplicate pnd's */
      _json_set_string(tmp.id,         json_object_get(package,"id"),    PNDMAN_ID);
      _json_set_string(tmp.path,       json_object_get(package, "path"), PNDMAN_PATH);
      _json_set_version(&tmp.version,  json_object_get(package,"version"));
      _strip_slash(tmp.path);

      pnd = _pndman_repository_new_pnd_check(&tmp, tmp.path, device->mount, repo);
      if (!pnd) return RETURN_FAIL;

      /* free old titles and descriptions (if instance or old) */
      _pndman_package_free_titles(pnd);
      _pndman_package_free_descriptions(pnd);
      _pndman_package_free_previewpics(pnd);
      _pndman_package_free_licenses(pnd);
      _pndman_package_free_categories(pnd);

      memcpy(pnd->id,      tmp.id,  PNDMAN_ID);
      memcpy(pnd->path,    tmp.path,  PNDMAN_PATH);
      memcpy(&pnd->version, &tmp.version, sizeof(pndman_version));
      _json_set_string(pnd->repository,   json_object_get(package,"repository"), PNDMAN_STR);
      _json_set_string(pnd->md5,          json_object_get(package,"md5"),     PNDMAN_MD5);
      _json_set_string(pnd->url,          json_object_get(package,"uri"),     PNDMAN_STR);
      _json_set_localization(pnd,         json_object_get(package,"localizations"));
      _json_set_string(pnd->info,         json_object_get(package,"info"),    PNDMAN_STR);
      _json_set_number(&pnd->size,        json_object_get(package, "size"),            size_t);
      _json_set_number(&pnd->modified_time, json_object_get(package, "modified-time"), time_t);
      _json_set_number(&pnd->local_modified_time, json_object_get(package, "local-modified-time"), time_t);
      _json_set_number(&pnd->rating,      json_object_get(package, "rating"),          int);
      _json_set_author(&pnd->author,      json_object_get(package,"author"));
      _json_set_string(pnd->vendor,       json_object_get(package,"vendor"),  PNDMAN_NAME);
      _json_set_string(pnd->icon,         json_object_get(package,"icon"),    PNDMAN_PATH);
      _json_set_previewpics(pnd,          json_object_get(package,"previewpics"));
      _json_set_licenses(pnd,             json_object_get(package,"licenses"));
      _json_set_sources(pnd,              json_object_get(package,"source"));
      _json_set_categories(pnd,           json_object_get(package,"categories"));
      _json_set_number(&pnd->commercial,  json_object_get(package,"commercial"), int);

      /* update mount, if device given */
      if (device)
         strncpy(pnd->mount, device->mount, PNDMAN_PATH-1);

      _strip_slash(pnd->url);
      _strip_slash(pnd->icon);
   }
   return RETURN_OK;
}

/* INTERNAL */

/* \brief get return value from client api */
int _pndman_json_client_api_return(void *file, pndman_api_status *status)
{
   json_t *root = NULL, *object;
   json_error_t error;
   assert(file && status);
   memset(status, 0, sizeof(pndman_api_status));

   /* flush and reset to beginning */
   fflush(file); fseek(file, 0L, SEEK_SET);
   if (!(root = json_loadf(file, 0, &error)))
      goto fail;

   status->status = API_ERROR;
   if (!(object = json_object_get(root, "error"))) {
      if (!(object = json_object_get(root, "success")))
         goto fail;
      status->status = API_SUCCESS;
   }

   _json_set_number(&status->number, json_object_get(object, "number"), int);
   _json_set_string(status->text, json_object_get(object,"text"), PNDMAN_SHRT_STR);

   json_decref(root);
   return status->status==API_SUCCESS?
          RETURN_OK:RETURN_FAIL;

fail:
   IFDO(json_decref, root);
   return RETURN_FAIL;
}

/* \brief get value from single json object */
int _pndman_json_get_value(const char *key, char *value,
      size_t size, void *file)
{
   json_t *root = NULL;
   json_error_t error;
   assert(key && value && file);
   memset(value, 0, size);

   /* flush and reset to beginning */
   fflush(file); fseek(file, 0L, SEEK_SET);
   if (!(root = json_loadf(file, 0, &error)))
      goto bad_json;

   _json_set_string(value, json_object_get(root, key), size);
   json_decref(root);
   return RETURN_OK;

bad_json:
   DEBFAIL(JSON_BAD_JSON, error.text, "client api");
   IFDO(json_decref, root);
   return RETURN_FAIL;
}

/* \brief get int value from single json object */
int _pndman_json_get_int_value(const char *key, int *value,
      void *file)
{
   json_t *root = NULL, *object;
   json_error_t error;
   assert(key && value && file);

   /* flush and reset to beginning */
   fflush(file); fseek(file, 0L, SEEK_SET);
   if (!(root = json_loadf(file, 0, &error)))
      goto bad_json;

   if ((object = json_object_get(root, "success")))
      _json_set_number(value, json_object_get(object, key), int);

   json_decref(root);
   return RETURN_OK;

bad_json:
   DEBFAIL(JSON_BAD_JSON, error.text, "client api");
   IFDO(json_decref, root);
   return RETURN_FAIL;
}

/* define some macros for api json functions */
#define json_fast_string(x,y)    if (x) y = json_string_value(x)
#define json_fast_number(x,y,c)  if (x) y = (c)json_number_value(x)

/* \brief process comment json data */
int _pndman_json_comment_pull(void *user_data,
      pndman_api_comment_callback callback,
      pndman_package *pnd, void *file)
{
   time_t date = 0;
   const char *comment = NULL, *username = NULL;
   pndman_version version;
   pndman_api_comment_packet cpacket;
   json_t *root, *versions, *varray, *comments, *carray;
   json_error_t error;
   size_t v = 0, c = 0, ncomments = 0;
   assert(file && callback);

   /* flush and reset to beginning */
   fflush(file); fseek(file, 0L, SEEK_SET);
   if (!(root = json_loadf(file, 0, &error)))
      goto bad_json;

   memset(&cpacket, 0, sizeof(pndman_api_comment_packet));
   versions = json_object_get(root, "versions");
   if (json_is_array(versions)) {
      while ((varray = json_array_get(versions, v++))) {
         memset(&version, 0, sizeof(pndman_version));
         _json_set_version(&version, json_object_get(varray,"version"));
         comments = json_object_get(varray, "comments");
         if (!json_is_array(comments)) continue; c = 0;
         while ((carray = json_array_get(comments, c++))) {
            json_fast_number(json_object_get(carray, "date"), date, time_t);
            json_fast_string(json_object_get(carray, "username"), username);
            json_fast_string(json_object_get(carray, "comment"), comment);

            cpacket.pnd = pnd; cpacket.date = date;
            cpacket.version = &version; cpacket.username = username;
            cpacket.comment = comment; cpacket.user_data = user_data;
            callback(PNDMAN_CURL_DONE, &cpacket);
            ++ncomments;
         }
      }
   } else DEBUG(PNDMAN_LEVEL_WARN, JSON_NO_V_ARRAY, "comment pull");

   /* no comments */
   if (!ncomments) {
      cpacket.user_data = user_data;
      strcpy(cpacket.error, "No comments");
      callback(PNDMAN_CURL_DONE, &cpacket);
   }

   json_decref(root);
   return RETURN_OK;

bad_json:
   DEBFAIL(JSON_BAD_JSON, error.text, "comment pull");
   IFDO(json_decref, root);
   return RETURN_FAIL;
}

/* \brief process download history json data */
int _pndman_json_download_history(void *user_data,
      pndman_api_history_callback callback,
      void *file)
{
   time_t date = 0;
   const char *id = NULL;
   pndman_version version;
   pndman_api_history_packet hpacket;
   json_t *root, *history, *packages, *parray;
   json_error_t error;
   size_t p = 0, nhistory = 0;
   assert(file && callback);

   /* flush and reset to beginning */
   fflush(file); fseek(file, 0L, SEEK_SET);
   if (!(root = json_loadf(file, 0, &error)))
      goto bad_json;

   memset(&hpacket, 0, sizeof(pndman_api_history_packet));
   history = json_object_get(root, "history");
   if (json_is_object(history)) {
      packages = json_object_get(history, "packages");
      if (json_is_array(packages)) {
         while ((parray = json_array_get(packages, p++))) {
            memset(&version, 0, sizeof(pndman_version));
            json_fast_string(json_object_get(parray, "id"), id);
            _json_set_version(&version, json_object_get(parray,"version"));
            json_fast_number(json_object_get(parray, "download_date"), date, time_t);

            hpacket.id = id; hpacket.version = &version;
            hpacket.download_date = date; hpacket.user_data = user_data;
            callback(PNDMAN_CURL_DONE, &hpacket);
            ++nhistory;
         }
      } else DEBUG(PNDMAN_LEVEL_WARN, JSON_NO_P_ARRAY, "download history");
   }

   if (!nhistory) {
      hpacket.user_data = user_data;
      strcpy(hpacket.error, "No history");
      callback(PNDMAN_CURL_DONE, &hpacket);
   }

   json_decref(root);
   return RETURN_OK;

bad_json:
   DEBFAIL(JSON_BAD_JSON, error.text, "download history");
   IFDO(json_decref, root);
   return RETURN_FAIL;
}

/* \brief process archived pnd json data */
int _pndman_json_archived_pnd(
      pndman_package *pnd, void *file) {
   time_t date = 0;
   pndman_version version;
   pndman_package *p;
   json_t *root, *archived, *versions, *varray;
   json_error_t error;
   size_t v = 0;
   assert(pnd && file);

   /* flush and reset to beginning */
   fflush(file); fseek(file, 0L, SEEK_SET);
   if (!(root = json_loadf(file, 0, &error)))
      goto bad_json;

   /* free old history */
   if ((p = pnd->next_installed))
      while ((p = _pndman_free_pnd(p->next_installed)));
   p = pnd;

   archived = json_object_get(root, "archived");
   if (json_is_object(archived)) {
      versions = json_object_get(archived, "versions");
      if (json_is_array(versions)) {
         while ((varray = json_array_get(versions, v++))) {
            if (!(p->next_installed = _pndman_new_pnd())) continue;
            _pndman_copy_pnd(p->next_installed, pnd);
            memset(&version, 0, sizeof(pndman_version));
            _json_set_version(&version, json_object_get(varray,"version"));
            json_fast_number(json_object_get(varray, "date"), date, time_t);
            p->next_installed->modified_time = date;
            p->next_installed->repositoryptr = pnd->repositoryptr;
            memcpy(&p->next_installed->version, &version, sizeof(pndman_version));
            p = p->next_installed;
         }
      } else DEBUG(PNDMAN_LEVEL_WARN, JSON_NO_V_ARRAY, "archived pnd");
   }

   json_decref(root);
   return RETURN_OK;

bad_json:
   DEBFAIL(JSON_BAD_JSON, error.text, "archieved pnd");
   IFDO(json_decref, root);
   return RETURN_FAIL;
}

/* undef macros for above functions */
#undef json_fast_number
#undef json_fast_string

/* \brief process retivied json data */
int _pndman_json_process(pndman_repository *repo,
      pndman_device *device, void *file)
{
   json_t *root = NULL, *repo_header, *packages;
   json_error_t error;
   assert(repo && file);

   /* flush and reset to beginning */
   fflush(file); fseek(file, 0L, SEEK_SET);
   if (!(root = json_loadf(file, 0, &error)))
      goto bad_json;

   repo_header = json_object_get(root, "repository");
   if (json_is_object(repo_header)) {
      if (_pndman_json_repo_header(repo_header, repo) == RETURN_OK) {
         packages = json_object_get(root, "packages");
         if (json_is_array(packages))
            _pndman_json_process_packages(packages, repo, device);
         else DEBUG(PNDMAN_LEVEL_WARN, JSON_NO_P_ARRAY, repo->url);
      }
   } else DEBUG(PNDMAN_LEVEL_WARN, JSON_NO_R_HEADER, repo->url);

   json_decref(root);
   return RETURN_OK;

bad_json:
   DEBFAIL(JSON_BAD_JSON, error.text, repo->url);
   IFDO(json_decref, root);
   return RETURN_FAIL;
}

/* \brief print to file with escapes */
static void _cfprintf(FILE *f, char *str)
{
   int len, i;
   assert(f && str);
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
   fprintf(f, "\"%s\n", delim ? "," : "");
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
int _pndman_json_commit(pndman_repository *r,
      pndman_device *d, void *f)
{
   pndman_package *p;
   pndman_translated *t, *td;
   pndman_previewpic *pic;
   pndman_category *c;
   pndman_license *l;
   int found = 0, delim = 0;
   clock_t now = clock();
   assert(f && d && r);

   fprintf(f, "{\n"); /* start */
   fprintf(f, "\"repository\":{\n");
   _fkeyf(f, "name", r->name, 1);
   _fkeyf(f, "version", r->version, r->prev?1:0);

   /* non local repository */
   if (r->prev) {
      _fkeyf(f, "updates", r->updates, 1);
      fprintf(f, "\"timestamp\":%lu,\n", r->timestamp);
      _fkeyf(f, "client_api", r->api.root, r->api.store_credentials?1:0);
      if (r->api.store_credentials) {
         _fkeyf(f, "username", r->api.username, 1);
         _fkeyf(f, "api_key", r->api.key, 0);
      }
   }

   fprintf(f, "},\n");
   fprintf(f, "\"packages\":[\n"); /* packages */
   for (p = r->pnd; p; p = p->next) {
      /* this pnd doesn't belong to this device */
      if (!r->prev && strcmp(p->mount, d->mount))
         continue;

      fprintf(f, "%s{\n", delim?",":""); delim = 1;

      /* local repository */
      if (!r->prev) {
         _fkeyf(f, "path", p->path, 1);
         _fkeyf(f, "repository", p->repository, 1);
      }

      _fkeyf(f, "id", p->id, 1);
      _fkeyf(f, "uri", p->url, 1);
      fprintf(f, "\"commercial\":%d,\n", p->commercial);

      /* version object */
      fprintf(f, "\"version\":{\n");
      _fkeyf(f, "major", p->version.major, 1);
      _fkeyf(f, "minor", p->version.minor, 1);
      _fkeyf(f, "release", p->version.release, 1);
      _fkeyf(f, "build", p->version.build, 1);
      _fkeyf(f, "type",
            p->version.type == PND_VERSION_BETA    ? "beta"    :
            p->version.type == PND_VERSION_ALPHA   ? "alpha"   : "release", 0);
      fprintf(f, "},\n");

      /* localization object */
      fprintf(f, "\"localizations\":{\n");
      for (t = p->title; t; t = t->next) {
         found = 0;
         for (td = p->description; td ; td = td->next)
            if (!_strupcmp(td->lang, t->lang)) {
               found = 1;
               break;
            }

         fprintf(f, "\"%s\":{\n", t->lang);
         _fkeyf(f, "title", t->string, 1);
         _fkeyf(f, "description", found ? td->string : "", 0);
         fprintf(f, "}%s\n", t->next ? "," : "");
      }

      /* fallback */
      if (!p->title)
         fprintf(f, "\"en_US\":{\"title\":\"\",\"description\":\"\"}\n");
      fprintf(f, "},\n");

      _fkeyf(f, "info", p->info, 1);

      fprintf(f, "\"size\":%zu,\n", p->size);
      _fkeyf(f, "md5", p->md5, 1);
      fprintf(f, "\"modified-time\":%lu,\n", p->modified_time);
      if (!r->prev) fprintf(f, "\"local-modified-time\":%lu,\n", p->local_modified_time);
      fprintf(f, "\"rating\":%d,\n", p->rating);

      /* author object */
      fprintf(f, "\"author\":\n");
      fprintf(f, "{\n");
      _fkeyf(f, "name", p->author.name, 1);
      _fkeyf(f, "website", p->author.website, 1);
      _fkeyf(f, "email", p->author.email, 0);
      fprintf(f, "},\n");

      _fkeyf(f, "vendor", p->vendor, 1);
      _fkeyf(f, "icon", p->icon, 1);

      /* previewpics array */
      fprintf(f, "\"previewpics\":[\n");
      for (pic = p->previewpic; pic; pic = pic->next)
         _fstrf(f, pic->src, pic->next ? 1 : 0);
      fprintf(f, "],\n");

      /* licenses array */
      fprintf(f, "\"licenses\":[\n");
      for (l = p->license; l; l = l->next)
         _fstrf(f, l->name, l->next ? 1 : 0);
      fprintf(f, "],\n");

      /* sources array */
      fprintf(f, "\"source\":[\n");
      for (l = p->license; l; l = l->next)
         _fstrf(f, l->sourcecodeurl, l->next ? 1 : 0);
      fprintf(f, "],\n");

      /* categories array */
      fprintf(f, "\"categories\":[");
      for (c = p->category; c; c = c->next) {
         _fstrf(f, c->main, 1);
         _fstrf(f, c->sub, c->next ? 1 : 0);
      }
      fprintf(f, "]\n");
      fprintf(f, "}\n");
   }
   fprintf(f, "]}\n"); /* end */
   fflush(f);

   DEBUG(PNDMAN_LEVEL_CRAP, "JSON write took %.2f seconds", (double)(clock()-now)/CLOCKS_PER_SEC);
   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
