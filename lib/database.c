#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __POSIX__
#  include <fcntl.h>
#endif

#include <curl/curl.h>
#include "pndman.h"
#include "package.h"
#include "repository.h"
#include "device.h"
#include "curl.h"
#include "json.h"

/* strings */
static const char *URL_CPY_FAIL     = "Failed to copy url from repository.";
static const char *BAD_URL          = "Repository has empty url, or it is a local repository.";
static const char *TMP_FILE_FAIL    = "Failed to get temporary file.";
static const char *WRITE_FAIL       = "Failed to open %s, for writing.\n";
static const char *READ_FAIL        = "Failed to open %s, for reading.\n";
static const char *CURLM_FAIL       = "Internal CURLM initialization failed.";
static const char *CURL_HANDLE_FAIL = "Failed to allocate CURL handle.";
static const char *LOCK_BLOCK_TIMEOUT = "%s blocking for IO operation timed out.";

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
   if (!p) return 0;
   while (*s)
      if (!strncmp(s, old, strlen(old))) {
         p  -= (intptr_t)cout;
         cout= realloc(cout, slen += strlen(new)-strlen(old) );
         tmp = strcpy(p=cout+(intptr_t)p, new);
         p  += strlen(tmp);
         s  += strlen(old);
      } else *p++=*s++;
   *p=0;
   return cout;
}

/* \brief internal blocking function with timeout */
#ifdef __POSIX__
static int blockfile(int fd, char *path, flock *fl)
#else
static int blockfile(char *path)
#endif
{
   int timeout=5;
#ifdef __POSIX__
   /* block until file ready for use */
   while (fcntl(fd, F_SETLK, fl) != 0) {
      sleep(1);

      /* timeout */
      if (--timeout==0) {
         DEBFAILP(LOCK_BLOCK_TIMEOUT, path);
         return RETURN_FAIL;
      }
   }
#else
   FILE *f;
   /* block until lock doesn't exist */
   while ((f = fopen(path, "r"))) {
      fclose(f);
#ifdef __WIN32__
      Sleep(1000);
#else
      sleep(1);
#endif

      /* timeout */
      if (--timeout==0) {
         DEBFAILP(LOCK_BLOCK_TIMEOUT, path);
         return RETURN_FAIL;
      }
   }
#endif
   return RETURN_OK;
}

/* \brief block read until file is unlocked */
static int readblock(char *path)
{
#ifdef __POSIX__
   int fd;
   if ((fd = open(path, O_WRONLY)) == -1)
      return RETURN_FAIL;
   if (blockfile(fd, db_path) != RETURN_OK) {
      close(fd);
      return RETURN_FAIL;
   }
   close(fd);
#else
   char lckpath[PATH_MAX];
   /* on non posix, do it uncertain way by creating lock file */
   strcpy(lckpath, path);
   strncat(lckpath, ".lck", PATH_MAX-1);
   if (blockfile(lckpath) != RETURN_OK)
      return RETURN_FAIL;
#endif
   return RETURN_OK;
}

/* \brief locks file from other processes from reading and writing */
#ifdef __POSIX__
static int lockfile(char *path)
#else
static FILE* lockfile(char *path)
#endif
{
#ifdef __POSIX__
   struct flock fl;
   int fd;

   /* lock the file */
   fl.l_type   = F_WRLCK | F_RDLCK;
   fl.l_whence = SEEK_SET;
   fl.l_start  = 0;
   fl.l_len    = 0;
   fl.l_pid    = getpid();
   if ((fd = open(path, O_WRONLY)) == -1)
      return -1;

   /* block until ready */
   if (blockfile(fd, path, &fl) != RETURN_OK) {
      close(fd);
      return -1;
   }
   return fd;
#else
   FILE *f;
   char lckpath[PATH_MAX];

   /* on non posix, do it uncertain way by creating lock file */
   strcpy(lckpath, path);
   strncat(lckpath, ".lck", PATH_MAX-1);

   /* block until ready */
   if (blockfile(lckpath) != RETURN_OK)
      return NULL;

   /* create .lck file */
   if (!(f = fopen(lckpath, "w")))
      return NULL;

   /* make sure it's created */
   fflush(f);
   return f;
#endif
}

/* \brief unlock the file */
#ifdef __POSIX__
static void unlockfile(int fd)
#else
static void unlockfile(FILE *f, char *path)
#endif
{
#ifdef __POSIX__
   struct flock fl;
   fl.l_type = F_UNLCK;
   fcntl(fd, F_SETLK, &lockinfo);
   close(fd);
#else
   char lckpath[PATH_MAX];
   /* on non posix, do it uncertain way by creating lock file */
   strcpy(lckpath, path);
   strncat(lckpath, ".lck", PATH_MAX-1);
   fclose(f);
   unlink(lckpath);
#endif
}

/* \brief allocate internal sync request */
static int _pndman_new_sync_handle(pndman_sync_handle *object, pndman_repository *repo)
{
   char *url = NULL;
   char timestamp[REPO_TIMESTAMP];

   /* create temporary write, where to write the request */
   object->file = _pndman_get_tmp_file();
   if (!object->file) {
      DEBFAIL(TMP_FILE_FAIL);
      return RETURN_FAIL;
   }

   /* reset curl */
   object->curl = curl_easy_init();
   if (!object->curl) {
      fclose(object->file);
      DEBFAIL(CURL_HANDLE_FAIL);
      return RETURN_FAIL;
   }

   /* check wether, to do merging or full sync */
   if (strlen(repo->updates) && repo->timestamp && !(object->flags & PNDMAN_SYNC_FULL)) {
      snprintf(timestamp, REPO_TIMESTAMP-1, "%lu", repo->timestamp);
      url = str_replace(repo->updates, "%time%", timestamp);
   } else url = strdup(repo->url);
   if (!url) {
      curl_easy_cleanup(object->curl);
      fclose(object->file);
      DEBFAIL(URL_CPY_FAIL);
      return RETURN_FAIL;
   }

   DEBUGP(3, "ADD: %s\n", url);
   DEBUGP(3, "PRI: %p\n", object);

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
   char db_path[PATH_MAX], *appdata;
   assert(device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata || !strlen(appdata)) return RETURN_FAIL;

   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   DEBUGP(3, "-!- writing to %s\n", db_path);

   /* lock the file */
#ifdef __POSIX__
   int fd;
   if ((fd = lockfile(db_path)) == -1)
      return RETURN_FAIL;
#else
   FILE *fd;
   if (!(fd = lockfile(db_path)))
      return RETURN_FAIL;
#endif

   f = fopen(db_path, "w");
   if (!f) {
      DEBFAILP(WRITE_FAIL, db_path);
      return RETURN_FAIL;
   }

   /* write local db */
   _pndman_json_commit(repo, f);

   fclose(f);

#ifdef __POSIX__
   unlockfile(fd);
#else
   unlockfile(fd, db_path);
#endif
   return RETURN_OK;
}

/* \brief Store repositories to database */
static int _pndman_db_commit(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   pndman_repository *r;
   char db_path[PATH_MAX], *appdata;
   assert(device);

   /* find local db and read it first */
   repo = _pndman_repository_first(repo);
   _pndman_db_commit_local(repo, device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata || !strlen(appdata)) return RETURN_FAIL;

   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   DEBUGP(3, "-!- writing to %s\n", db_path);

   /* lock the file */
#ifdef __POSIX__
   int fd;
   if ((fd = lockfile(db_path)) == -1)
      return RETURN_FAIL;
#else
   FILE *fd;
   if (!(fd = lockfile(db_path)))
      return RETURN_FAIL;
#endif

   f = fopen(db_path, "w");
   if (!f) {
      DEBFAILP(WRITE_FAIL, db_path);
      return RETURN_FAIL;
   }
   /* write repositories */
   r = repo;
   for (; r; r = r->next) {
      if (!strlen(r->url)) continue;
      fprintf(f, "[%s]\n", r->url);
      _pndman_json_commit(r, f);
   }

   fclose(f);
#ifdef __POSIX__
   unlockfile(fd);
#else
   unlockfile(fd, db_path);
#endif
   return RETURN_OK;
}

/* \brief Read local database information from device */
static int _pndman_db_get_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f;
   char db_path[PATH_MAX];
   char appdata[PATH_MAX];
   assert(repo && device);

   /* check appdata */
   _pndman_device_get_appdata_no_create(appdata, device);
   if (!strlen(appdata)) return RETURN_FAIL;

   /* begin to read local database */
   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   DEBUGP(3, "-!- local from %s\n", db_path);

   /* block until ready for read */
   if (readblock(db_path) != RETURN_OK)
      return RETURN_FAIL;

   f = fopen(db_path, "r");
   if (!f) {
      DEBFAILP(READ_FAIL, db_path);
      return RETURN_FAIL;
   }

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
   char appdata[PATH_MAX];
   char *ret;
   int  parse = 0;
   assert(device);

   /* find local db and read it first */
   if (!repo->prev)
      return _pndman_db_get_local(repo, device);

   /* invalid url */
   if (!strlen(repo->url)) {
      DEBFAIL(BAD_URL);
      return RETURN_FAIL;
   }

   /* check appdata */
   _pndman_device_get_appdata_no_create(appdata, device);
   if (!strlen(appdata)) return RETURN_FAIL;

   /* begin to read other repositories */
   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   DEBUGP(3, "-!- reading from %s\n", db_path);

   /* block until ready for read */
   if (readblock(db_path) != RETURN_OK)
      return RETURN_FAIL;

   f = fopen(db_path, "r");
   if (!f) {
      DEBFAILP(READ_FAIL, db_path);
      return RETURN_FAIL;
   }

   /* write parse result here */
   f2 = _pndman_get_tmp_file();
   if (!f2) {
      fclose(f);
      DEBFAIL(TMP_FILE_FAIL);
      return RETURN_FAIL;
   }

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
         handle->progress.done = msg->data.result==CURLE_OK?1:0;
         if (msg->data.result != CURLE_OK) strncpy(handle->error, curl_easy_strerror(msg->data.result), LINE_MAX);
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
   if (!strlen(repo->url)) {
      DEBFAIL(BAD_URL);
      return RETURN_FAIL;
   }

   /* create curl handle if doesn't exist */
   if (!_pndman_curlm) {
      /* get multi curl handle */
      _pndman_curlm = curl_multi_init();
      if (!_pndman_curlm) {
         DEBFAIL(CURLM_FAIL);
         return RETURN_FAIL;
      }
   }

   /* create indivual curl requests */
   if (_pndman_new_sync_handle(handle, repo) != RETURN_OK)
      return RETURN_FAIL;

   return RETURN_OK;
}

/* \brief do version comparision and set update pointer */
static int _pndman_version_check(pndman_package *lp, pndman_package *rp)
{
   /* check if we can check against modified_time */
   if (lp->modified_time && !strcmp(lp->repository, rp->repository)) {
      if (rp->modified_time > lp->modified_time) {
         if (lp->update) lp->update->update = NULL;
         lp->update = rp; rp->update = lp;
         return RETURN_TRUE;
      }
   }

   /* this package already has update, try if the new proposed is newer */
   if (lp->update) {
      if (_pndman_vercmp(&lp->update->version, &rp->version)) {
         lp->update->update = NULL;
         lp->update = rp; rp->update = lp;
      }
      else return RETURN_FALSE;
   }

   if (_pndman_vercmp(&lp->version, &rp->version)) {
      lp->update = rp;
      rp->update = lp;
   }
   return lp->update?RETURN_TRUE:RETURN_FALSE;
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

   // DEBUGP(3, "%d : %p\n", still_running, _pndman_internal_request);
   return still_running;
}

/* \brief try read repository information from device
 * (if it's been committed there before) */
int pndman_read_from_device(pndman_repository *repo, pndman_device *device)
{
   if (!repo) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "repo pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_db_get(repo, device);
}

/* \brief check for updates, returns the number of updates */
int pndman_check_updates(pndman_repository *list)
{
   if (!list) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "list pointer is NULL");
      return RETURN_FAIL;
   }
   list = _pndman_repository_first(list); /* idiot proof */
   return _pndman_check_updates(list);
}

/* \brief request synchorization for repository
 * call this for each repository you want to sync before pndman_sync */
int pndman_sync_request(pndman_sync_handle *handle, unsigned int flags, pndman_repository *repo)
{
   if (!repo || !handle) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "repo or handle pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_repository_sync_request(handle, flags, repo);
}

/* \brief free sync request */
int pndman_sync_request_free(pndman_sync_handle *handle)
{
   if (!handle) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "handle pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_sync_handle_free(handle);
}

/* \brief commits _all_ repositories to specific device */
int pndman_commit_all(pndman_repository *repo, pndman_device *device)
{
   if (!repo || !device) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "repo or device pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_db_commit(repo, device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
