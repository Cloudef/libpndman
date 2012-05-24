#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __unix__
#  define __POSIX__
#endif

#ifdef __POSIX__
#  include <fcntl.h>
#  define BLOCK_FD   int
#  define BLOCK_INIT 0
#else
#  define BLOCK_FD   file*
#  define BLOCK_INIT NULL
#endif

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
static int blockfile(int fd, char *path, struct flock *fl)
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
         fl->l_type = F_UNLCK;
         fcntl(fd, F_SETLK, fl);
         goto fail;
      }
   }
   fl->l_type = F_UNLCK;
   fcntl(fd, F_SETLK, fl);
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
      if (--timeout==0)
         goto fail;
   }
#endif
   return RETURN_OK;

fail:
   DEBFAIL(DATABASE_LOCK_TIMEOUT, path);
   return RETURN_FAIL;
}

/* \brief block read until file is unlocked */
static int readblock(char *path)
{
#ifdef __POSIX__
   struct flock fl;
   int fd;

   fl.l_type   = F_WRLCK | F_RDLCK;
   fl.l_whence = SEEK_SET;
   fl.l_start  = 0;
   fl.l_len    = 0;
   fl.l_pid    = getpid();
   if ((fd = open(path, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR)) == -1)
      return RETURN_FAIL;
   if (blockfile(fd, path, &fl) != RETURN_OK) {
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
   if ((fd = open(path, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR)) == -1)
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
   fcntl(fd, F_SETLK, &fl);
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

/* \brief Store local database seperately */
static int _pndman_db_commit_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f = NULL;
   BLOCK_FD fd = BLOCK_INIT;
   char db_path[PATH_MAX], *appdata;
   assert(device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata || !strlen(appdata)) goto fail;

   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   DEBUG(PNDMAN_LEVEL_CRAP, "-!- writing to %s", db_path);

   /* lock the file */
#ifdef __POSIX__
   if ((fd = lockfile(db_path)) == -1)
      goto fail;
#else
   if (!(fd = lockfile(db_path)))
      goto fail;
#endif

   if (!(f = fopen(db_path, "w")))
      goto write_fail;

   /* write local db */
   _pndman_json_commit(repo, f);

   fclose(f);

#ifdef __POSIX__
   unlockfile(fd);
#else
   unlockfile(fd, db_path);
#endif
   return RETURN_OK;

write_fail:
   DEBFAIL(WRITE_FAIL, db_path);
fail:
#ifdef __POSIX__
   if (fd != -1) close(fd);
#else
   IFDO(fclose, fd);
#endif
   IFDO(fclose, f);
   return RETURN_FAIL;
}

/* \brief Store repositories to database */
static int _pndman_db_commit(pndman_repository *repo, pndman_device *device)
{
   FILE *f = NULL;
   BLOCK_FD fd = BLOCK_INIT;
   pndman_repository *r;
   char db_path[PATH_MAX], *appdata;

   assert(device);

   /* find local db and read it first */
   repo = _pndman_repository_first(repo);
   _pndman_db_commit_local(repo, device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata || !strlen(appdata)) goto fail;

   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   DEBUG(PNDMAN_LEVEL_CRAP, "-!- writing to %s", db_path);

   /* lock the file */
#ifdef __POSIX__
   if ((fd = lockfile(db_path)) == -1)
      goto fail;
#else
   if (!(fd = lockfile(db_path)))
      goto fail;
#endif

   if (!(f = fopen(db_path, "w")))
      goto write_fail;

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

write_fail:
   DEBFAIL(WRITE_FAIL, db_path);
fail:
#ifdef __POSIX__
   if (fd != -1) close(fd);
#else
   IFDO(fclose, fd);
#endif
   IFDO(fclose, f);
   return RETURN_FAIL;
}

/* \brief Read local database information from device */
static int _pndman_db_get_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f = NULL;
   char db_path[PATH_MAX];
   char appdata[PATH_MAX];
   assert(repo && device);

   /* check appdata */
   _pndman_device_get_appdata_no_create(appdata, device);
   if (!strlen(appdata)) goto fail;

   /* begin to read local database */
   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/local.db", PATH_MAX-1);
   DEBUG(PNDMAN_LEVEL_CRAP, "-!- local from %s", db_path);

   /* block until ready for read */
   if (readblock(db_path) != RETURN_OK)
      goto fail;

   if (!(f = fopen(db_path, "r")))
      goto read_fail;

   /* read local database */
   _pndman_json_process(repo, f);

   fclose(f);
   return RETURN_OK;

read_fail:
   DEBFAIL(READ_FAIL, db_path);
fail:
   IFDO(fclose, f);
   return RETURN_FAIL;
}

/* \brief Read repository information from device */
int _pndman_db_get(pndman_repository *repo, pndman_device *device)
{
   FILE *f = NULL, *f2 = NULL;
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
   if (!strlen(repo->url))
      goto bad_url;

   /* check appdata */
   _pndman_device_get_appdata_no_create(appdata, device);
   if (!strlen(appdata)) goto fail;

   /* begin to read other repositories */
   strncpy(db_path, appdata, PATH_MAX-1);
   strncat(db_path, "/repo.db", PATH_MAX-1);
   DEBUG(PNDMAN_LEVEL_CRAP, "-!- reading from %s", db_path);

   /* block until ready for read */
   if (readblock(db_path) != RETURN_OK)
      goto fail;

   if (!(f = fopen(db_path, "r")))
      goto read_fail;

   /* write parse result here */
   if (!(f2 = _pndman_get_tmp_file()))
      goto fail;

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
   fclose(f2); fclose(f);
   return RETURN_OK;

bad_url:
   DEBFAIL(DATABASE_BAD_URL);
   goto fail;
read_fail:
   DEBFAIL(READ_FAIL, db_path);
fail:
   IFDO(fclose, f2);
   IFDO(fclose, f);
   return RETURN_FAIL;
}

/* \brief perform internal sync */
static void _pndman_sync_done(pndman_curl_code code, void *data, const char *info)
{
   pndman_sync_handle *handle  = (pndman_sync_handle*)data;
   pndman_curl_handle *chandle = (pndman_curl_handle*)handle->data;

   if (code == PNDMAN_CURL_FAIL) {
      strncpy(handle->error, info, PNDMAN_STR);
   } else if (code == PNDMAN_CURL_DONE) {
      if (_pndman_json_process(handle->repository, chandle->file) == RETURN_OK) {
         /* update timestamp */
         handle->repository->timestamp = time(0);
      }
   }

   /* callback to this handle */
   if (handle->callback)
      handle->callback(code, handle);
}

/* \brief free internal sync request */
static void _pndman_sync_handle_free(pndman_sync_handle *object)
{
   pndman_curl_handle *handle;
   handle = (pndman_curl_handle*)object->data;
   IFDO(_pndman_curl_handle_free, handle);
   object->data      = NULL;
   object->callback  = NULL;
}

/* \brief allocate internal sync request */
static int _pndman_new_sync_handle_init(pndman_sync_handle *object)
{
   /* init */
   memset(object, 0, sizeof(pndman_sync_handle));
   return RETURN_OK;
}

/* \brief perform sync request */
static int _pndman_sync_handle_perform(pndman_sync_handle *object)
{
   char *url = NULL;
   char timestamp[PNDMAN_TIMESTAMP];
   pndman_curl_handle *handle;
   assert(object && object->repository);
   handle = (pndman_curl_handle*)object->data;

   /* check wether, to do merging or full sync */
   if (strlen(object->repository->updates) &&
         object->repository->timestamp &&
         !(object->flags & PNDMAN_SYNC_FULL)) {
      snprintf(timestamp, PNDMAN_TIMESTAMP-1, "%lu", object->repository->timestamp);
      url = str_replace(object->repository->updates, "%time%", timestamp);
   } else url = strdup(object->repository->url);

   /* url copy failed */
   if (!url)
      goto url_cpy_fail;

   /* send to curl handler */
   object->data = handle = _pndman_curl_handle_new(object, &object->progress,
         _pndman_sync_done, NULL);
   if (!handle) goto fail;

   curl_easy_setopt(handle->curl, CURLOPT_URL, url);
   free(url);

   /* do it */
   return _pndman_curl_handle_perform(handle);

url_cpy_fail:
   DEBFAIL(DATABASE_URL_COPY_FAIL);
fail:
   IFDO(free, url);
   return RETURN_FAIL;
}

/* \brief do version comparision and set update pointer */
static int _pndman_version_check(pndman_package *lp, pndman_package *rp)
{
   /* md5sums are same, no update */
   if (!strcmp(lp->md5, rp->md5))
      return RETURN_FALSE;

   /* the md5 checking won't be enough here,
    * since if user has different repos.
    * They might provide even newer version. */

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

   /* maybe as last resort use the md5 differ?
    * might be good idea if no other way was update detected */
   if (!lp->update && strlen(lp->md5)) {
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

/* \brief try read repository information from device
 * (if it's been committed there before) */
PNDMANAPI int pndman_read_from_device(pndman_repository *repo, pndman_device *device)
{
   if (!repo || !device) {
      BADUSE("repository or device pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_db_get(repo, device);
}

/* \brief check for updates, returns the number of updates */
PNDMANAPI int pndman_check_updates(pndman_repository *list)
{
   if (!list) {
      BADUSE("list pointer is NULL");
      return RETURN_FAIL;
   }
   list = _pndman_repository_first(list); /* idiot proof */
   return _pndman_check_updates(list);
}

/* \brief create new synchorization request object */
PNDMANAPI int pndman_sync_handle_init(pndman_sync_handle *handle)
{
   if (!handle) {
      BADUSE("handle pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_new_sync_handle_init(handle);
}

/* \brief perform the synchorization */
PNDMANAPI int pndman_sync_handle_perform(pndman_sync_handle *handle)
{
   if (!handle) {
      BADUSE("handle pointer is NULL");
      return RETURN_FAIL;
   }
   if (!handle->repository) {
      BADUSE("handle has no repository");
      return RETURN_FAIL;
   }

   return _pndman_sync_handle_perform(handle);
}

/* \brief free sync request */
PNDMANAPI void pndman_sync_handle_free(pndman_sync_handle *handle)
{
   if (!handle) {
      BADUSE("handle pointer is NULL");
   }
   _pndman_sync_handle_free(handle);
}

/* \brief commits _all_ repositories to specific device */
PNDMANAPI int pndman_commit_all(pndman_repository *repo, pndman_device *device)
{
   if (!repo || !device) {
      BADUSE("repo or device pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_db_commit(repo, device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
