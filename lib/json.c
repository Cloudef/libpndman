#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <curl/curl.h>
#include <jansson.h>

#include "pndman.h"
#include "package.h"
#include "device.h"
#include "repository.h"
#include "curl.h"

/* \brief wrap curl_request internally and add *next pointer */
typedef struct _pndman_sync_request
{
   pndman_repository            *repo;
   curl_request                  request;
   struct _pndman_sync_request  *next;
} _pndman_sync_request;

/* \brief curl multi handle for json.c */
static CURLM *_pndman_curlm = NULL;
static _pndman_sync_request *_pndman_internal_request = NULL;

/* \brief allocate internal sync request */
static _pndman_sync_request* _pndman_new_sync_request(_pndman_sync_request *first, pndman_repository *repo)
{
   _pndman_sync_request *object;
   _pndman_sync_request *r = first;
   for(; first && r->next; r = r->next);

   object = malloc(sizeof(_pndman_sync_request));
   if (!object)
      return NULL;

   object->repo = repo;
   object->next = NULL;

   /* reset curl */
   object->request.curl = curl_easy_init();
   if (!object->request.curl)
   {
      free(object);
      return NULL;
   }

   object->request.result.pos = 0;
   memset(object->request.result.data, 0, MAX_REQUEST-1);

   printf("ADD: %s\n", repo->url);
   printf("PRI: %p\n", object);

   /* set download URL */
   curl_easy_setopt(object->request.curl, CURLOPT_URL,     repo->url);
   curl_easy_setopt(object->request.curl, CURLOPT_PRIVATE, object);
   curl_easy_setopt(object->request.curl, CURLOPT_WRITEFUNCTION, curl_write_request);
   curl_easy_setopt(object->request.curl, CURLOPT_CONNECTTIMEOUT, 5L );
   //curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, 0);
   //curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
   curl_easy_setopt(object->request.curl, CURLOPT_WRITEDATA, &object->request);

   /* add to multi interface */
   curl_multi_add_handle(_pndman_curlm, object->request.curl);

   return object;
}

/* \brief free internal sync request */
static void _pndman_free_sync_request(_pndman_sync_request *object)
{
   assert(object);
   curl_multi_remove_handle(_pndman_curlm, object->request.curl);
   curl_easy_cleanup(object->request.curl);
   free(object);
}

/* \brief json parse repository header */
static int _pndman_json_repo_header(json_t *repo_header, pndman_repository *repo)
{
   json_t *element;

   assert(repo_header);
   assert(repo);

   if ((element = json_object_get(repo_header, "name")))
      strncpy(repo->name, json_string_value(element), REPO_NAME-1);
   if ((element = json_object_get(repo_header, "updates")))
      strncpy(repo->updates, json_string_value(element), REPO_URL-1);
   if ((element = json_object_get(repo_header, "version")))
      repo->version = json_number_value(element);

   return RETURN_OK;
}

/* \brief json parse repository packages */
static int _pndman_json_repo_packages(json_t *packages, pndman_repository *repo)
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
      strcpy(pnd->id, json_string_value(json_object_get(package,"id")));
   }
   return RETURN_OK;
}

/* \brief process retivied json data */
static int _pndman_sync_process(pndman_repository *repo, char *data)
{
   pndman_package *pnd;
   json_t *root, *repo_header, *packages;
   json_error_t error;

   root = json_loads(data, 0, &error);
   if (!root) {
      printf("WARN: Bad json data, won't process sync for: %s\n", repo->url);
      return RETURN_FAIL;
   }

   repo_header = json_object_get(root, "repository");
   if (json_is_object(repo_header)) {
      if (_pndman_json_repo_header(repo_header, repo) == RETURN_OK) {
         packages = json_object_get(root, "packages");
         if (json_is_array(packages))
            _pndman_json_repo_packages(packages, repo);
         else printf("WARN: No packages array for: %s\n", repo->url);
      }
   } else printf("WARN: Bad repo header for: %s\n", repo->url);

   json_decref(root);
   return RETURN_OK;
}

/* \brief write json data to local database */
static int _pndman_commit_database(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   pndman_repository *r;
   pndman_package    *p;
   char db_path[PATH_MAX];

   if (!device) return RETURN_FAIL;
   strncpy(db_path, device->mount, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   printf("-!- writing to %s\n", db_path);
   f = fopen(db_path, "w");
   if (!f) return RETURN_FAIL;

   /* write repositories */
   r = repo;
   for (; r; r = r->next) {
      if (!strlen(r->url)) continue;
      fprintf(f, "[%s]\n", r->url);
      fprintf(f, "{"); /* start */
      fprintf(f, "\"repository\":{\"name\":\"%s\",\"version\":%.2f,\"updates\":\"%s\"},",
              r->name, r->version, r->updates);
      fprintf(f, "\"packages\":["); /* packages */
      for (p = r->pnd; p; p = p->next) {
         fprintf(f, "{");
         fprintf(f, "\"id\":\"%s\"", p->id);
         fprintf(f, "}");
         if (p->next) fprintf(f, ",");
      }
      fprintf(f, "]}\n"); /* end */
   }

   fclose(f);
   return RETURN_OK;
}

/* \brief read local database information from device */
static int _pndman_query_local_from_devices(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   char s[LINE_MAX];
   char data[MAX_REQUEST];
   char db_path[PATH_MAX];
   char *ret;

   assert(repo);
   if (_pndman_curlm) return RETURN_OK;
   if (!device)       return RETURN_FAIL;
   strncpy(db_path, device->mount, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   printf("-!- local from %s\n", db_path);
   f = fopen(db_path, "r");
   if (!f) return RETURN_FAIL;

   /* read local database */
   memset(s, 0, LINE_MAX);
   while ((ret = fgets(s, LINE_MAX, f))) strncat(data, s, LINE_MAX-1);
   _pndman_sync_process(repo, data);

   fclose(f);
   return RETURN_OK;
}

/* \brief read repository information from device */
int _pndman_query_repository_from_devices(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   char s[LINE_MAX];
   char s2[LINE_MAX];
   char data[MAX_REQUEST];
   char db_path[PATH_MAX];
   char *ret;
   pndman_repository *r, *c = NULL;

   if (_pndman_curlm) return RETURN_OK;
   if (!device)       return RETURN_FAIL;
   strncpy(db_path, device->mount, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   printf("-!- reading from %s\n", db_path);
   f = fopen(db_path, "r");
   if (!f) return RETURN_FAIL;

   /* read repositories */
   memset(s,  0, LINE_MAX);
   while ((ret = fgets(s, LINE_MAX, f))) {
      r = repo;
      for (; r; r = r->next) {
         if (!strlen(r->url)) {
            if (!strcmp(r->name, LOCAL_DB_NAME)) { _pndman_query_local_from_devices(r, device); continue; } /* local db */
            else continue;
         }
         snprintf(s2, LINE_MAX-1, "[%s]", r->url);
         if (!(memcmp(s, s2, strlen(s2)))) {
            if (c) _pndman_sync_process(c,data);
            c = r;
            memset(data, 0, MAX_REQUEST);
            memset(s, 0, LINE_MAX);
         }
      }
      if (c && strlen(s)) strncat(data, s, MAX_REQUEST-1);
   }
   if (c) _pndman_sync_process(c,data);

   fclose(f);
   return RETURN_OK;
}

/* \brief perform internal sync */
int _pndman_sync_perform(int *still_running)
{
   int maxfd = -1;
   struct timeval timeout;
   fd_set fdread;
   fd_set fdwrite;
   fd_set fdexcep;
   long curl_timeout = -1;

   CURLMsg *msg;
   int msgs_left;

   _pndman_sync_request *request;

   /* perform download */
   curl_multi_perform(_pndman_curlm, still_running);

   /* zero file descriptions */
   FD_ZERO(&fdread);
   FD_ZERO(&fdwrite);
   FD_ZERO(&fdexcep);

   /* set a suitable timeout to play around with */
   timeout.tv_sec = 1;
   timeout.tv_usec = 0;

   /* timeout */
   curl_multi_timeout(_pndman_curlm, &curl_timeout);
   if(curl_timeout >= 0) {
      timeout.tv_sec = curl_timeout / 1000;
      if(timeout.tv_sec > 1) timeout.tv_sec = 1;
      else timeout.tv_usec = (curl_timeout % 1000) * 1000;
   }

   /* get file descriptors from the transfers */
   curl_multi_fdset(_pndman_curlm, &fdread, &fdwrite, &fdexcep, &maxfd);
   if (maxfd < -1) return RETURN_FAIL;
   select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

#if 1
   /* update status of curl handles */
   while ((msg = curl_multi_info_read(_pndman_curlm, &msgs_left))) {
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);
      if (msg->msg == CURLMSG_DONE) { /* DONE */
         _pndman_sync_process(request->repo, request->request.result.data);
      }
   }
#endif

   return RETURN_OK;
}

/* query cleanup */
static void _pndman_query_cleanup(void)
{
   _pndman_sync_request *r2;

   assert(_pndman_curlm);
   assert(_pndman_internal_request);

   /* loop code here */
   r2 = _pndman_internal_request;
   for (; r2; r2 = _pndman_internal_request)
   { _pndman_internal_request = r2->next; _pndman_free_sync_request(r2); }

   curl_multi_cleanup(_pndman_curlm);

   _pndman_curlm            = NULL;
   _pndman_internal_request = NULL;
}

/* \brief query repository information from json */
int _pndman_query_repository_from_json(pndman_repository *repo)
{
   pndman_repository *r;
   _pndman_sync_request *r2;
   int still_running;

   if (!_pndman_curlm) {
      assert(!_pndman_internal_request);

      /* get multi curl handle */
      _pndman_curlm = curl_multi_init();
      if (!_pndman_curlm)
         return RETURN_FAIL;

      /* create indivual curl requests */
      r = _pndman_repository_first(repo);
      for (; r; r = r->next) {
         if (!strlen(r->url)) continue;
         r2 = _pndman_new_sync_request(_pndman_internal_request, r);
         if (!_pndman_internal_request && r2) _pndman_internal_request = r2;
         else if (!r2) break;
      }

      /* failed to create even one request */
      if (!_pndman_internal_request)
         return RETURN_FAIL;
   }

   /* perform sync */
   if (_pndman_sync_perform(&still_running) != RETURN_OK) {
      _pndman_query_cleanup();
      return RETURN_FAIL;
   }

   /* destoroy curlm when done,
    * and return exit code */
   if (!still_running)
      _pndman_query_cleanup();

   // printf("%d : %p\n", still_running, _pndman_internal_request);
   return still_running;
}

int pndman_commit_database(pndman_repository *repo, pndman_device *device)
{
   DEBUG("pndman_commit_database");
   if (!repo || !device) return RETURN_FAIL;
   return _pndman_commit_database(repo, device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
