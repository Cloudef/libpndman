#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include "curl/curl.h"

#include "pndman.h"
#include "package.h"
#include "repository.h"
#include "device.h"
#include "curl.h"
#include "json.h"

/* \brief wrap curl_request internally and add *next pointer */
typedef struct _pndman_sync_request
{
   pndman_repository            *repo;
   FILE                         *file;
   CURL                         *curl;
   struct _pndman_sync_request  *next;
} _pndman_sync_request;

/* \brief curl multi handle for json.c */
static CURLM *_pndman_curlm = NULL;
static _pndman_sync_request *_pndman_internal_request = NULL;

/* \brief replace substring, precondition: s!=0, old!=0, new!=0 */
static char *str_replace(const char *s, const char *old, const char *new)
{
   size_t slen = strlen(s)+1;
   char *cout=0, *p=0, *tmp=NULL; cout=malloc(slen); p=cout;
   if( !p ) return 0;
   while( *s )
      if(!strncmp(s, old, strlen(old))) {
         p  -= (intptr_t)cout;
         cout= realloc(cout, slen += strlen(new)-strlen(old) );
         tmp = strcpy(p=cout+(intptr_t)p, new);
         p  += strlen(tmp);
         s  += strlen(old);
      } else *p++=*s++;
   *p=0;
   return cout;
}

/* \brief allocate internal sync request */
static _pndman_sync_request* _pndman_new_sync_request(_pndman_sync_request *first, pndman_repository *repo)
{
   _pndman_sync_request *object;
   _pndman_sync_request *r = first;
   char *url = NULL;
   char timestamp[REPO_TIMESTAMP];

   /* find last request */
   for(; first && r->next; r = r->next);
   object = malloc(sizeof(_pndman_sync_request));
   if (!object) return NULL;

   /* create temporary write, where to write the request */
   object->file = _pndman_get_tmp_file();
   if (!object->file) {
      free(object);
      return NULL;
   }

   object->repo = repo;
   object->next = NULL;

   /* reset curl */
   object->curl = curl_easy_init();
   if (!object->curl) {
      fclose(object->file);
      free(object);
      return NULL;
   }

   /* check wether, to do merging or full sync */
   if (strlen(repo->updates) && repo->timestamp) {
      snprintf(timestamp, REPO_TIMESTAMP-1, "%lu", repo->timestamp);
      url = str_replace(repo->updates, "%time%", timestamp);
   } else url = strdup(repo->url);
   if (!url) {
      curl_easy_cleanup(object->curl);
      fclose(object->file);
      free(object);
      return NULL;
   }

   printf("ADD: %s\n", url);
   printf("PRI: %p\n", object);

   /* set download URL */
   curl_easy_setopt(object->curl, CURLOPT_URL, url);
   curl_easy_setopt(object->curl, CURLOPT_PRIVATE, object);
   curl_easy_setopt(object->curl, CURLOPT_WRITEFUNCTION, curl_write_file);
   curl_easy_setopt(object->curl, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);
   //curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, 0);
   //curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
   curl_easy_setopt(object->curl, CURLOPT_WRITEDATA, object->file);

   /* add to multi interface */
   curl_multi_add_handle(_pndman_curlm, object->curl);
   free(url);
   return object;
}

/* \brief free internal sync request */
static void _pndman_free_sync_request(_pndman_sync_request *object)
{
   assert(object);
   curl_multi_remove_handle(_pndman_curlm, object->curl);
   curl_easy_cleanup(object->curl);
   fclose(object->file);
   free(object);
}

/* \brief Store local database seperately */
static int _pndman_db_commit_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   char db_path[PATH_MAX];

   if (!device || !device->exist) return RETURN_FAIL;
   strncpy(db_path, device->mount, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   printf("-!- writing to %s\n", db_path);
   f = fopen(db_path, "w");
   if (!f) return RETURN_FAIL;

   /* write local db */
   _pndman_json_commit(repo, f);

   fclose(f);
   return RETURN_OK;
}

/* \brief Store repositories to database */
static int _pndman_db_commit(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   pndman_repository *r;
   char db_path[PATH_MAX];
   if (!device || !device->exist) return RETURN_FAIL;

   /* find local db and read it first */
   r = repo;
   for (; r; r = r->next)
      if (!strcmp(r->name, LOCAL_DB_NAME)) {
         _pndman_db_commit_local(r, device);
         break;
      }

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
      _pndman_json_commit(r, f);
   }

   fclose(f);
   return RETURN_OK;
}

/* \brief Read local database information from device */
static int _pndman_db_get_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   char db_path[PATH_MAX];

   assert(repo);
   if (_pndman_curlm)               return RETURN_OK;
   if (!device || !device->exist)   return RETURN_FAIL;

   /* begin to read local database */
   strncpy(db_path, device->mount, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   printf("-!- local from %s\n", db_path);
   f = fopen(db_path, "r");
   if (!f) return RETURN_FAIL;

   /* read local database */
   _pndman_json_process(repo, f);

   fclose(f);
   return RETURN_OK;
}

/* \brief Read repository information from device */
int _pndman_db_get(pndman_repository *repo, pndman_device *device)
{
   FILE *f, *f2;
   char s[LINE_MAX];
   char s2[LINE_MAX];
   char db_path[PATH_MAX];
   char *ret;
   int  parse = 0;

   if (_pndman_curlm)               return RETURN_OK;
   if (!device || !device->exist)   return RETURN_FAIL;

   /* find local db and read it first */
   if (!strcmp(repo->name, LOCAL_DB_NAME)) {
      _pndman_db_get_local(repo, device);
      return RETURN_OK;
   }

   /* invalid url */
   if (!strlen(repo->url))
      return RETURN_FAIL;

   /* begin to read other repositories */
   strncpy(db_path, device->mount, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   printf("-!- reading from %s\n", db_path);
   f = fopen(db_path, "r");
   if (!f) return RETURN_FAIL;

   /* write parse result here */
   f2 = _pndman_get_tmp_file();
   if (!f2) return RETURN_FAIL;

   /* read repository */
   memset(s,  0, LINE_MAX);
   snprintf(s2, LINE_MAX-1, "[%s]", repo->url);
   while ((ret = fgets(s, LINE_MAX, f))) {
      if (!parse && !(memcmp(s, s2, strlen(s2)))) parse = 1;
      else if (parse && strlen(s)) fprintf(f2, "%s", s);
   }

   /* process and close */
   fflush(f2);
   _pndman_json_process(repo,f2);
   fclose(f2);
   fclose(f);
   return RETURN_OK;
}

/* \brief perform internal sync */
static int _pndman_sync_perform()
{
   int still_running;
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
   curl_multi_perform(_pndman_curlm, &still_running);

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
         printf("%s : done\n", request->repo->url);
         _pndman_json_process(request->repo, request->file);
      }
   }
#endif

   return still_running;
}

/* \brief query cleanup */
static void _pndman_query_cleanup(void)
{
   _pndman_sync_request *r2;

   /* loop code here */
   r2 = _pndman_internal_request;
   for (; r2; r2 = _pndman_internal_request) {
      _pndman_internal_request = r2->next;
      _pndman_free_sync_request(r2);
   }

   if (_pndman_curlm) curl_multi_cleanup(_pndman_curlm);
   _pndman_curlm            = NULL;
   _pndman_internal_request = NULL;
}

/* \brief request for synchorization for this repository */
static int _pndman_repository_sync_request(pndman_repository *repo)
{
   _pndman_sync_request *r2;

   /* create curl handle if doesn't exist */
   if (!_pndman_curlm) {
      /* get multi curl handle */
      _pndman_curlm = curl_multi_init();
      if (!_pndman_curlm) return RETURN_FAIL;
   }

   /* create indivual curl requests */
   if (!strlen(repo->url)) return RETURN_FAIL;
   r2 = _pndman_new_sync_request(_pndman_internal_request, repo);
   if (!_pndman_internal_request && r2) _pndman_internal_request = r2;
   else if (!r2) return RETURN_FAIL;

   /* failed to create even one request */
   if (!_pndman_internal_request)
      return RETURN_FAIL;
   return RETURN_OK;
}

/* API */

/* \brief synchorize all repositories
 * returns the number of synchorizing repos, on error returns -1 */
int pndman_sync()
{
   int still_running;
   if (!_pndman_curlm) return RETURN_FAIL;

   /* perform sync */
   if ((still_running = _pndman_sync_perform()) == -1) {
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

/* \brief try read repository information from device
 * (if it's been committed there before) */
int pndman_read_from_device(pndman_repository *repo, pndman_device *device)
{
   DEBUG("pndman_read_from_device");
   if (!repo) return RETURN_FAIL;
   return _pndman_db_get(repo, device);
}

/* \brief request synchorization for repository
 * call this for each repository you want to sync before pndman_sync */
int pndman_sync_request(pndman_repository *repo)
{
   DEBUG("pndman_sync_request");
   if (!repo) return RETURN_FAIL;
   return _pndman_repository_sync_request(repo);
}

/* \brief commits _all_ repositories to specific device */
int pndman_commit(pndman_repository *repo, pndman_device *device)
{
   DEBUG("pndman_commit_database");
   if (!repo || !device) return RETURN_FAIL;
   return _pndman_db_commit(repo, device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
