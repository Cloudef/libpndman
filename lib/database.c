#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include <curl/curl.h>
#include "pndman.h"
#include "package.h"
#include "repository.h"
#include "device.h"
#include "curl.h"
#include "json.h"

/* \brief flags for sync request to determite what to do */
typedef enum pndman_sync_flags
{
   PNDMAN_SYNC_FULL = 0x001,
} pndman_sync_flags;

/* \brief sync handle struct */
typedef struct pndman_sync_handle
{
   char                 error[LINE_MAX];
   pndman_repository    *repository;

   /* get set on function */
   unsigned int         flags;

   /* progress */
   curl_progress        progress;

   /* internal */
   FILE                 *file;
   CURL                 *curl;
} pndman_sync_handle;

/* \brief curl multi handle for json.c */
static CURLM *_pndman_curlm = NULL;

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
static int _pndman_new_sync_handle(pndman_sync_handle *object, pndman_repository *repo)
{
   char *url = NULL;
   char timestamp[REPO_TIMESTAMP];

   /* create temporary write, where to write the request */
   object->file = _pndman_get_tmp_file();
   if (!object->file) return RETURN_FAIL;

   /* reset curl */
   object->curl = curl_easy_init();
   if (!object->curl) {
      fclose(object->file);
      return RETURN_FAIL;;
   }

   /* check wether, to do merging or full sync */
   if (strlen(repo->updates) && repo->timestamp && !(object->flags & PNDMAN_SYNC_FULL)) {
      snprintf(timestamp, REPO_TIMESTAMP-1, "%lu", repo->timestamp);
      url = str_replace(repo->updates, "%time%", timestamp);
   } else url = strdup(repo->url);
   if (!url) {
      curl_easy_cleanup(object->curl);
      fclose(object->file);
      return RETURN_FAIL;
   }

   DEBUGP("ADD: %s\n", url);
   DEBUGP("PRI: %p\n", object);

   /* set download URL */
   curl_easy_setopt(object->curl, CURLOPT_URL, url);
   curl_easy_setopt(object->curl, CURLOPT_PRIVATE, object);
   curl_easy_setopt(object->curl, CURLOPT_WRITEFUNCTION, curl_write_file);
   curl_easy_setopt(object->curl, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);
   curl_easy_setopt(object->curl, CURLOPT_NOPROGRESS, 0);
   curl_easy_setopt(object->curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
   curl_easy_setopt(object->curl, CURLOPT_PROGRESSDATA, &object->progress);
   curl_easy_setopt(object->curl, CURLOPT_WRITEDATA, object->file);

   /* add to multi interface */
   curl_multi_add_handle(_pndman_curlm, object->curl);
   free(url);
   return RETURN_OK;
}

/* \brief free internal sync request */
static int _pndman_sync_handle_free(pndman_sync_handle *object)
{
   if (object->curl) {
      if (_pndman_curlm) curl_multi_remove_handle(_pndman_curlm, object->curl);
      curl_easy_cleanup(object->curl);
      object->curl = NULL;
   }
   if (object->file) fclose(object->file);
   object->file = NULL;
   return RETURN_OK;
}

/* \brief Store local database seperately */
static int _pndman_db_commit_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   char db_path[PATH_MAX];
   char *appdata;
   assert(device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata) return RETURN_FAIL;

   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   DEBUGP("-!- writing to %s\n", db_path);
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
   char *appdata;
   assert(device);

   /* find local db and read it first */
   repo = _pndman_repository_first(repo);
   _pndman_db_commit_local(repo, device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata) return RETURN_FAIL;

   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   DEBUGP("-!- writing to %s\n", db_path);
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
   char *appdata;
   assert(repo && device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata) return RETURN_FAIL;

   /* begin to read local database */
   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   DEBUGP("-!- local from %s\n", db_path);
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
   char *appdata;
   int  parse = 0;
   assert(device);

   /* find local db and read it first */
   if (!repo->prev) {
      _pndman_db_get_local(repo, device);
      return RETURN_OK;
   }

   /* invalid url */
   if (!strlen(repo->url))
      return RETURN_FAIL;

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata) return RETURN_FAIL;

   /* begin to read other repositories */
   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   DEBUGP("-!- reading from %s\n", db_path);
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

   pndman_sync_handle *handle;

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
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &handle);
      if (msg->msg == CURLMSG_DONE) { /* DONE */
         handle->progress.done = 1;
         _pndman_json_process(handle->repository, handle->file);
      }
   }
#endif

   return still_running;
}

/* \brief query cleanup */
static void _pndman_query_cleanup(void)
{
   if (_pndman_curlm) curl_multi_cleanup(_pndman_curlm);
   _pndman_curlm = NULL;
}

/* \brief request for synchorization for this repository */
static int _pndman_repository_sync_request(pndman_sync_handle *handle, unsigned int flags, pndman_repository *repo)
{
   /* init */
   handle->repository = repo;
   handle->curl   = NULL;
   handle->file   = NULL;
   handle->flags  = flags;
   curl_init_progress(&handle->progress);
   memset(handle->error, 0, LINE_MAX);

   /* no valid url == FAIL, same for local repository, this is expected */
   if (!strlen(repo->url)) return RETURN_FAIL;

   /* create curl handle if doesn't exist */
   if (!_pndman_curlm) {
      /* get multi curl handle */
      _pndman_curlm = curl_multi_init();
      if (!_pndman_curlm) return RETURN_FAIL;
   }

   /* create indivual curl requests */
   if (_pndman_new_sync_handle(handle, repo) != RETURN_OK)
      return RETURN_FAIL;

   return RETURN_OK;
}

static int _pndman_vercmp(pndman_package *lp, pndman_package *rp)
{
   int major, minor, release, build;
   int major2, minor2, release2, build2;

   /* do vanilla version checking */
   major     = strtol(lp->version.major, (char **) NULL, 10);
   minor     = strtol(lp->version.minor, (char **) NULL, 10);
   release   = strtol(lp->version.release, (char **) NULL, 10);
   build     = strtol(lp->version.build, (char **) NULL, 10);

   /* remote */
   major2    = strtol(rp->version.major, (char **) NULL, 10);
   minor2    = strtol(rp->version.minor, (char **) NULL, 10);
   release2  = strtol(rp->version.release, (char **) NULL, 10);
   build2    = strtol(rp->version.build, (char **) NULL, 10);

   if (major2 > major)
      return 1;
   else if (major2 == major && minor2 > minor)
      return 1;
   else if (major2 == major && minor2 == minor && release2 > release)
      return 1;
   else if (major2 == major && minor2 == minor && release2 == release && build2 > build)
      return 1;
   return 0;
}

/* \brief do version comparision and set update pointer */
static int _pndman_version_check(pndman_package *lp, pndman_package *rp)
{
   /* check if we can check against modified_time */
   if (!strcmp(lp->repository, rp->repository)) {
      if (rp->modified_time > lp->modified_time) {
         if (lp->update) lp->update->update = NULL;
         lp->update = rp; rp->update = lp;
         return 1;
      }
   }

   /* this package already has update, try if the new proposed is newer */
   if (lp->update) {
      if (_pndman_vercmp(lp->update, rp)) {
         lp->update->update = NULL;
         lp->update = rp; rp->update = lp;
      }
      else return 0;
   }

   if (_pndman_vercmp(lp, rp)) {
      lp->update = rp;
      rp->update = lp;
   }
   return lp->update?1:0;
}

/* \brief check updates, returns the number of updates found */
static int _pndman_check_updates(pndman_repository *list)
{
   pndman_repository *r;
   pndman_package    *p, *pnd;
   int updates = 0;
   assert(list);

   if (!list->next) return 0;
   for (pnd = list->pnd; pnd; pnd = pnd->next)
      for (r = list->next; r; r = r->next)
         for (p = r->pnd; p; p = p->next)
            if (!strcmp(pnd->id, p->id))
               updates += _pndman_version_check(pnd, p);
   return updates;
}

/* API */

/* \brief synchorize all repositories
 * returns the number of synchorizing repos, on error returns -1 */
int pndman_sync()
{
   int still_running;

   /* we are done :) */
   if (!_pndman_curlm) return 0;

   /* perform sync */
   if ((still_running = _pndman_sync_perform()) == -1) {
      _pndman_query_cleanup();
      return RETURN_FAIL;
   }

   /* destoroy curlm when done,
    * and return exit code */
   if (!still_running) {
      _pndman_query_cleanup();

      /* fake so that we are still running, why?
       * this lets user to catch the final completed download/sync
       * in download/sync loop */
      still_running = 1;
   }

   // DEBUGP("%d : %p\n", still_running, _pndman_internal_request);
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

/* \brief check for updates, returns the number of updates */
int pndman_check_updates(pndman_repository *list)
{
   DEBUG("check updates");
   if (!list) return RETURN_FAIL;
   list = _pndman_repository_first(list); /* idiot proof */
   return _pndman_check_updates(list);
}

/* \brief request synchorization for repository
 * call this for each repository you want to sync before pndman_sync */
int pndman_sync_request(pndman_sync_handle *handle, unsigned int flags, pndman_repository *repo)
{
   DEBUG("pndman_sync_request");
   if (!repo || !handle) return RETURN_FAIL;
   return _pndman_repository_sync_request(handle, flags, repo);
}

/* \brief free sync request */
int pndman_sync_request_free(pndman_sync_handle *handle)
{
   DEBUG("pndman_sync_request_free");
   if (!handle) return RETURN_FAIL;
   return _pndman_sync_handle_free(handle);
}

/* \brief commits _all_ repositories to specific device */
int pndman_commit_all(pndman_repository *repo, pndman_device *device)
{
   DEBUG("pndman_commit_database");
   if (!repo || !device) return RETURN_FAIL;
   return _pndman_db_commit(repo, device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
