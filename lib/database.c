#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <bzlib.h>

#ifdef __APPLE__
#  include <malloc/malloc.h>
#else
#  include <malloc.h>
#endif

#define BLOCK_FD   FILE*
#define BLOCK_INIT NULL

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
static int blockfile(char *path)
{
   int timeout=5;
   FILE *f;
   /* block until lock doesn't exist */
   while ((f = fopen(path, "r"))) {
      fclose(f);
#ifdef _WIN32
      Sleep(1000);
#else
      sleep(1);
#endif

      /* timeout */
      if (--timeout==0)
         goto timedout;
   }
   return RETURN_OK;

timedout:
   DEBUG(PNDMAN_LEVEL_WARN, DATABASE_LOCK_TIMEOUT, path);
   unlink(path);
   return RETURN_OK;
}

/* \brief block read until file is unlocked */
static int readblock(char *path)
{
   char lckpath[PNDMAN_PATH];
   strncpy(lckpath, path, PNDMAN_PATH-1);
   strncat(lckpath, ".lck", PNDMAN_PATH-1);
   if (blockfile(lckpath) != RETURN_OK)
      return RETURN_FAIL;
   return RETURN_OK;
}

/* \brief locks file from other processes from reading and writing */
static FILE* lockfile(char *path)
{
   FILE *f;
   char lckpath[PNDMAN_PATH];

   /* on non posix, do it uncertain way by creating lock file */
   strncpy(lckpath, path, PNDMAN_PATH-1);
   strncat(lckpath, ".lck", PNDMAN_PATH-1);

   /* block until ready */
   if (blockfile(lckpath) != RETURN_OK)
      return NULL;

   /* create .lck file */
   if (!(f = fopen(lckpath, "w")))
      return NULL;

   /* make sure it's created */
   fflush(f);
   return f;
}

/* \brief unlock the file */
static void unlockfile(FILE *f, char *path)
{
   char lckpath[PNDMAN_PATH];
   /* on non posix, do it uncertain way by creating lock file */
   strncpy(lckpath, path, PNDMAN_PATH-1);
   strncat(lckpath, ".lck", PNDMAN_PATH-1);
   fclose(f);
   unlink(lckpath);
}

/* \brief Store local database seperately */
static int _pndman_db_commit_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f = NULL;
   BLOCK_FD fd = BLOCK_INIT;
   char db_path[PNDMAN_PATH], *appdata;
   assert(device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata || !strlen(appdata)) goto fail;

   strncpy(db_path, appdata, PNDMAN_PATH-1);
   strncat(db_path, "/local.db", PNDMAN_PATH-1);
   DEBUG(PNDMAN_LEVEL_CRAP, "-!- writing to %s", db_path);

   /* lock the file */
   if (!(fd = lockfile(db_path)))
      goto fail;

   if (!(f = fopen(db_path, "w")))
      goto write_fail;

   /* write local db */
   _pndman_json_commit(repo, device, f);

   fclose(f);

   unlockfile(fd, db_path);
   return RETURN_OK;

write_fail:
   DEBFAIL(WRITE_FAIL, db_path);
fail:
   IFDO(fclose, fd);
   IFDO(fclose, f);
   return RETURN_FAIL;
}

/* \brief Store repositories to database */
static int _pndman_db_commit(pndman_repository *repo, pndman_device *device)
{
   FILE *f = NULL;
   BLOCK_FD fd = BLOCK_INIT;
   pndman_repository *r;
   char db_path[PNDMAN_PATH], *appdata;

   assert(device);

   /* find local db and read it first */
   repo = _pndman_repository_first(repo);
   _pndman_db_commit_local(repo, device);

   /* check appdata */
   appdata = _pndman_device_get_appdata(device);
   if (!appdata || !strlen(appdata)) goto fail;

   strncpy(db_path, appdata, PNDMAN_PATH-1);
   strncat(db_path, "/repo.db", PNDMAN_PATH-1);
   DEBUG(PNDMAN_LEVEL_CRAP, "-!- writing to %s", db_path);

   /* lock the file */
   if (!(fd = lockfile(db_path)))
      goto fail;

   if (!(f = fopen(db_path, "w")))
      goto write_fail;

   /* write repositories */
   r = repo;
   for (; r; r = r->next) {
      if (!strlen(r->url)) continue;
      fprintf(f, "[%s]\n", r->url);
      _pndman_json_commit(r, device, f);
   }

   fclose(f);
   unlockfile(fd, db_path);
   return RETURN_OK;

write_fail:
   DEBFAIL(WRITE_FAIL, db_path);
fail:
   IFDO(fclose, fd);
   IFDO(fclose, f);
   return RETURN_FAIL;
}

/* \brief Read local database information from device */
static int _pndman_db_get_local(pndman_repository *repo, pndman_device *device)
{
   FILE *f = NULL;
   char db_path[PNDMAN_PATH];
   char appdata[PNDMAN_PATH];
   assert(repo && device);

   /* check appdata */
   _pndman_device_get_appdata_no_create(appdata, device);
   if (!strlen(appdata)) goto fail;

   /* begin to read local database */
   strncpy(db_path, appdata, PNDMAN_PATH-1);
   strncat(db_path, "/local.db", PNDMAN_PATH-1);
   DEBUG(PNDMAN_LEVEL_CRAP, "-!- local from %s", db_path);

   /* block until ready for read */
   if (readblock(db_path) != RETURN_OK)
      goto fail;

   if (!(f = fopen(db_path, "r")))
      goto read_fail;

   /* read local database */
   _pndman_json_process(repo, device, f);

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
   char db_path[PNDMAN_PATH];
   char appdata[PNDMAN_PATH];
   char *ret;
   int  parse = 0;
   pndman_repository *r, *rs;
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
   strncpy(db_path, appdata, PNDMAN_PATH-1);
   strncat(db_path, "/repo.db", PNDMAN_PATH-1);
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
   rs = _pndman_repository_first(repo);
   memset(s, 0, LINE_MAX);
   snprintf(s2, LINE_MAX-1, "[%s]", repo->url);
   while ((ret = fgets(s, LINE_MAX, f))) {
      if (!parse && !memcmp(s, s2, strlen(s2))) parse = 1;
      else if (parse && strlen(s)) {
         for (r = rs; r && parse; r = r->next) {
            snprintf(s2, LINE_MAX-1, "[%s]", r->url);
            if (!memcmp(s, s2, strlen(s2))) parse = 0;
         }
         if (!parse) break;
         fprintf(f2, "%s", s);
      }
   }

   /* process and close */
   _pndman_json_process(repo, NULL, f2);
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

/* \brief decompress bzip2 compressed curl handle */
static int _pndman_bzip2_decompress(pndman_curl_handle *chandle)
{
   typedef void BZFILE;
   BZFILE *bf;
   FILE *df;
   int error, read;
   char buf[4096*2];

   if (!(df = _pndman_get_tmp_file()))
      return RETURN_FAIL;

   fflush(chandle->file); fseek(chandle->file, 0L, SEEK_SET);
   bf = BZ2_bzReadOpen(&error, chandle->file, 0, 0, NULL, 0);
   while (error == BZ_OK) {
      read = BZ2_bzRead(&error, bf, buf, sizeof(buf));
      fwrite(buf, 1, read, df);
   }

   /* ugly internal swap trick */
   if (error == BZ_OK || error == BZ_STREAM_END) {
      fclose(chandle->file);
      chandle->file = df;
   } else {
      fclose(df);
   }

   BZ2_bzReadClose(&error, bf);
   return RETURN_OK;
}

/* \brief perform internal sync */
static void _pndman_sync_done(pndman_curl_code code, void *data, const char *info,
      pndman_curl_handle *chandle)
{
   pndman_sync_handle *handle  = (pndman_sync_handle*)data;

   if (code == PNDMAN_CURL_FAIL)
      strncpy(handle->error, info, PNDMAN_STR-1);
   else if (code == PNDMAN_CURL_DONE) {
      if (strstr(chandle->url, "bzip=true")) {
         _pndman_bzip2_decompress(chandle);
      }

      if (_pndman_json_process(handle->repository, NULL, chandle->file) != RETURN_OK) {
         strncpy(handle->error, "json parse failed", PNDMAN_STR-1);
         code = PNDMAN_CURL_FAIL;
      }
   }

   /* callback to this handle */
   if (handle->callback) handle->callback(code, handle);
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
static int _pndman_sync_handle_init(pndman_sync_handle *object)
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

   /* this is local repository,
    * let's fail and inform developer */
   if (!object->repository->prev)
      goto local_repo_fail;

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

   _pndman_curl_handle_set_url(handle, url);
   free(url);

   /* do it */
   return _pndman_curl_handle_perform(handle);

local_repo_fail:
   DEBFAIL(DATABASE_CANT_SYNC_LOCAL);
   goto fail;
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

   /* this package already has update, try if the new proposed is newer */
   if (lp->update) {
      if (_pndman_vercmp(&lp->update->version, &rp->version)) {
         lp->update->update = NULL;
         lp->update = rp; rp->update = lp;
      }
      else return RETURN_FALSE;
   }

   /* check if we can check against modified_time */
   if (lp->modified_time && !strcmp(lp->repository, rp->repository)) {
      if (rp->modified_time > lp->modified_time) {
         if (lp->update) lp->update->update = NULL;
         lp->update = rp; rp->update = lp;
         return RETURN_TRUE;
      }
   } else if (_pndman_vercmp(&lp->version, &rp->version)) {
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
PNDMANAPI int pndman_device_read_repository(pndman_repository *repo,
      pndman_device *device)
{
   CHECKUSE(repo);
   CHECKUSE(device);
   return _pndman_db_get(repo, device);
}

/* \brief check for updates, returns the number of updates */
PNDMANAPI int pndman_repository_check_updates(pndman_repository *list)
{
   CHECKUSE(list);
   list = _pndman_repository_first(list); /* idiot proof */
   return _pndman_check_updates(list);
}

/* \brief create new synchorization request object */
PNDMANAPI int pndman_sync_handle_init(pndman_sync_handle *handle)
{
   CHECKUSE(handle);
   return _pndman_sync_handle_init(handle);
}

/* \brief perform the synchorization */
PNDMANAPI int pndman_sync_handle_perform(pndman_sync_handle *handle)
{
   CHECKUSE(handle);
   CHECKUSE(handle->repository);
   return _pndman_sync_handle_perform(handle);
}

/* \brief free sync request */
PNDMANAPI void pndman_sync_handle_free(pndman_sync_handle *handle)
{
   CHECKUSEV(handle);
   _pndman_sync_handle_free(handle);
}

/* \brief commits _all_ repositories to specific device */
PNDMANAPI int pndman_repository_commit_all(pndman_repository *repo,
      pndman_device *device)
{
   CHECKUSE(repo);
   CHECKUSE(device);
   return _pndman_db_commit(repo, device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
