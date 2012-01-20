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

/* \brief json parse repo header */
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

/* \brief process retivied json data */
static int _pndman_sync_process(pndman_repository *repo, char *data)
{
   json_t *root, *repo_header;
   json_error_t error;

   root = json_loads(data, 0, &error);
   if (!root)
   { puts("WARN: bad json data"); return RETURN_FAIL; }

   repo_header = json_object_get(root, "repository");
   if (repo_header)
   {
      if (_pndman_json_repo_header(repo_header, repo))
      {
         /* read packages */
      }
   }

   json_decref(root);
   return RETURN_OK;
}

/* \brief write json data to local database */
static int _pndman_commit_database(pndman_repository *repo)
{
   FILE *f;
   pndman_repository *r;
   f = fopen("/tmp/repo.db", "w");
   if (!f) return RETURN_FAIL;

   /* write repositories */
   r = repo;
   for (; r; r = r->next) {
      if (!r->url) continue;
      fprintf(f, "[%s]\n", r->url);
      fprintf(f, "{"); /* start */
      fprintf(f, "\"repository\":{\"name\":\"%s\",\"version\":%.2f,\"updates\":\"%s\"},",
              r->name, r->version, r->updates);
      fprintf(f, "\"packages\":["); /* packages */
      fprintf(f, "]}\n"); /* end */
   }

   fclose(f);
   return RETURN_OK;
}

/* \brief read repository information from devices */
int _pndman_query_repository_from_devices(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   char s[LINE_MAX];
   char s2[LINE_MAX];
   char data[MAX_REQUEST];
   char     *ret;
   pndman_repository *r, *c = NULL;

   f = fopen("/tmp/repo.db", "r");
   if (!f) return RETURN_FAIL;

   /* read repositories */
   memset(s,  0, LINE_MAX);
   while ((ret = fgets(s, LINE_MAX, f)))
   {
      r = repo;
      for (; r; r = r->next) {
         if (!r->url) continue;
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
   while ((msg = curl_multi_info_read(_pndman_curlm, &msgs_left)))
   {
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);
      if (msg->msg == CURLMSG_DONE)
      {
         /* done */
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

   if (!_pndman_curlm)
   {
      assert(!_pndman_internal_request);

      /* get multi curl handle */
      _pndman_curlm = curl_multi_init();
      if (!_pndman_curlm)
         return RETURN_FAIL;

      /* create indivual curl requests */
      r = _pndman_repository_first(repo);
      for (; r; r = r->next)
      {
         r2 = _pndman_new_sync_request(_pndman_internal_request, r);
         if (!_pndman_internal_request && r2) _pndman_internal_request = r2;
         else if (!r2)                        break;
      }

      /* failed to create even one request */
      if (!_pndman_internal_request)
         return RETURN_FAIL;
   }

   /* perform sync */
   if (_pndman_sync_perform(&still_running) != RETURN_OK)
   {
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

int pndman_commit_database(pndman_repository *repo)
{
   DEBUG("pndman_commit_database");
   if (!repo) return RETURN_FAIL;
   return _pndman_commit_database(repo);
}

/* vim: set ts=8 sw=3 tw=0 :*/
