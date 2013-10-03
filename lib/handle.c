#include "internal.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>

#ifndef _WIN32
#  include <sys/stat.h>
#endif

/* \brief move file, src -> dst */
static int _pndman_move_file(const char* src, const char* dst)
{
   FILE *f;
   assert(src && dst);

   /* remove dst, if exists */
   if ((f = fopen(dst, "rb"))) {
      fclose(f);
      if (unlink(dst) != 0)
         goto rm_fail;
   }

   /* rename */
   if (rename(src, dst) != 0)
      goto mv_fail;

   return RETURN_OK;

rm_fail:
   DEBFAIL(HANDLE_RM_FAIL, dst);
   goto fail;
mv_fail:
   DEBFAIL(HANDLE_MV_FAIL, src, dst);
fail:
   return RETURN_FAIL;
}

/* \brief get backup directory, free when done */
static char* _get_backup_dir(pndman_device *device)
{
   assert(device);

   char *buffer;
   int size = snprintf(NULL, 0, "%s/pandora/backup", device->mount)+1;
   if (!(buffer = malloc(size))) goto fail;
   sprintf(buffer, "%s/pandora/backup", device->mount);

   if (access(buffer, F_OK) != 0) {
      if (errno == EACCES) goto fail;
#ifdef _WIN32
      if (mkdir(buffer) == -1)
#else
      if (mkdir(buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
#endif
         goto fail;
   }

   free(buffer);
   return RETURN_OK;

fail:
   IFDO(free, buffer);
   return NULL;
}

/* \brief check if file exists */
static int _file_exist(char *path)
{
   FILE *f;
   assert(path);

   if ((f = fopen(path, "rb"))) {
      fclose(f);
      return RETURN_TRUE;
   }

   return RETURN_FALSE;
}

/* \brief backup pnd */
static int _pndman_backup(pndman_package *pnd, pndman_device *device)
{
   char *tmp = NULL, *bckdir = NULL, *path = NULL;
   assert(pnd && device);

   /* get full path */
   if (!(path = _pndman_pnd_get_path(pnd)))
      goto fail;

   /* can't backup from void */
   if (_file_exist(path) != RETURN_OK)
      goto fail;

   /* get backup directory */
   if (!(bckdir = _get_backup_dir(device)))
      goto backup_fail;

   /* copy path */
   int size = snprintf(NULL, 0, "%s/%s - %s.%s.%s.%s.pnd", bckdir, pnd->id,
         pnd->version.major, pnd->version.minor, pnd->version.release, pnd->version.build)+1;
   if (!(tmp = malloc(size)))
      goto fail;

   sprintf(tmp, "%s/%s - %s.%s.%s.%s.pnd", bckdir, pnd->id,
         pnd->version.major, pnd->version.minor, pnd->version.release, pnd->version.build);
   free(bckdir);

   DEBUG(PNDMAN_LEVEL_CRAP, "backup: %s -> %s", path, tmp);
   int ret = _pndman_move_file(path, tmp);
   free(path);
   free(tmp);
   return ret;

backup_fail:
   DEBFAIL(HANDLE_BACKUP_DIR_FAIL, device);
fail:
   IFDO(free, tmp);
   IFDO(free, bckdir);
   IFDO(free, path);
   return RETURN_FAIL;
}

/* \brief check conflicts */
static int _conflict(char *id, char *path, pndman_device *device, pndman_repository *local)
{
   pndman_package *pnd;
   assert(id && path && local);

   for (pnd = local->pnd; pnd; pnd = pnd->next)
      if (id && pnd->id && strcmp(id, pnd->id) &&
          path && pnd->path && !strcmp(path, pnd->path) &&
          device->mount && pnd->mount && !strcmp(device->mount, pnd->mount)) {
         DEBUG(PNDMAN_LEVEL_CRAP, "CONFLICT: %s - %s", pnd->id, pnd->path);
         return RETURN_TRUE;
      }
   return RETURN_FALSE;
}

/* \brief pre routine when object has install flag */
static int _pndman_package_handle_download(pndman_package_handle *object)
{
   char *tmp_path = NULL, *appdata = NULL;
   pndman_device *d;
   pndman_curl_handle *handle;
   assert(object);

   if (!object->pnd)
      goto object_no_pnd;

   if (!object->pnd->url)
      goto object_pnd_url;

   if (!object->device)
      goto object_no_dev;

   if (!object->pnd->update &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_DESKTOP) &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_MENU)    &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_APPS))
      goto object_no_dst;

   if (object->pnd->update &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_DESKTOP) &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_MENU)    &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_APPS)) {
      if (!object->pnd->update || !object->pnd->update->path)
         goto object_wtf;
      for (d = _pndman_device_first(object->device);
           d && (d->mount && object->pnd->update->mount && strcmp(d->mount, object->pnd->update->mount));
           d = d->next);
      if (!d) goto fail; /* can't find installed device */
      object->device = d; /* assign device, old pnd is installed on */
   }

   /* check appdata */
   if (!(appdata = _pndman_device_get_appdata(object->device)))
      goto fail;

   int size = snprintf(NULL, 0, "%s/%s.tmp", appdata, object->pnd->id)+1;
   if (!(tmp_path = malloc(size))) goto fail;
   sprintf(tmp_path, "%s/%s.tmp", appdata, object->pnd->id);
   NULLDO(free, appdata);

   object->data = handle = _pndman_curl_handle_new(object, &object->progress, _pndman_package_handle_done, tmp_path);
   NULLDO(free, tmp_path);
   if (!handle) goto fail;

   /* commercial or logged download */
   if (object->pnd->repositoryptr && (object->pnd->commercial || (object->flags & PNDMAN_PACKAGE_LOG_HISTORY))) {
      return _pndman_api_commercial_download(handle, object);
   } else {
      /* normal anonyomous */
      _pndman_curl_handle_set_url(handle, object->pnd->url);
      return _pndman_curl_handle_perform(handle);
   }

object_no_pnd:
   DEBFAIL(HANDLE_NO_PND);
   goto fail;
object_pnd_url:
   DEBFAIL(HANDLE_PND_URL);
   goto fail;
object_no_dev:
   if (!object->pnd->update) {
      DEBFAIL(HANDLE_NO_DEV);
   } else {
      DEBFAIL(HANDLE_NO_DEV_UP);
   }
   goto fail;
object_no_dst:
   DEBFAIL(HANDLE_NO_DST);
   goto fail;
object_wtf:
   DEBFAIL(HANDLE_WTF);
fail:
   IFDO(free, tmp_path);
   IFDO(free, appdata);
   return RETURN_FAIL;
}

/* \brief parse filename from HTTP header */
static char* _parse_filename_from_header(const char *haystack)
{
   char *needle = "filename=\"";
   char file[256];
   unsigned int pos = 0, rpos = 0, found = 0;
   assert(haystack);

   for (; rpos < strlen(haystack); rpos++) {
      if (!found) {
         if (haystack[rpos] == needle[pos]) {
            if (++pos==strlen(needle)) {
               found = 1;
               pos = 0;
            }
         } else
            pos = 0;
      } else {
         if (haystack[rpos] != '"') {
            file[pos++] = haystack[rpos];
         } else {
            break;
         }
      }
   }

   /* zero terminate parsed filename */
   file[pos] = '\0';

   if (!found || !strlen(file))
      goto fail;

   return strdup(file);

fail:
   DEBUG(PNDMAN_LEVEL_WARN, HANDLE_HEADER_FAIL);
   return NULL;
}

/* \brief post routine when handle has install flag */
static int _pndman_package_handle_install(pndman_package_handle *object, pndman_repository *local)
{
   char *install = NULL, *relative = NULL, *filename = NULL, *prefix = NULL, *tmp = NULL, *tmp2 = NULL, *md5 = NULL;
   int uniqueid = 0;
   pndman_package *pnd, *oldp;
   pndman_curl_handle *handle;
   assert(object && local);

   handle = (pndman_curl_handle*)object->data;
   if (!object->device)
      goto handle_no_dev;
   if (!object->pnd)
      goto handle_no_pnd;

   if (!object->pnd->update &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_DESKTOP) &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_MENU)    &&
       !(object->flags & PNDMAN_PACKAGE_INSTALL_APPS))
      goto handle_no_dst;

   /* check MD5 */
   md5 = _pndman_md5(handle->path);
   if (!md5 && !(object->flags & PNDMAN_PACKAGE_FORCE))
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "R: %s L: %s", object->pnd->md5, md5);
   DEBUG(PNDMAN_LEVEL_CRAP, "%s", handle->path);

   /* do check against remote */
   if (md5 && object->pnd->md5 && strcmp(md5, object->pnd->md5)) {
      if (!(object->flags & PNDMAN_PACKAGE_FORCE))
         goto md5_fail;
      else DEBUG(2, HANDLE_MD5_DIFF);
   } NULLDO(free, md5);

   if (object->pnd->update && object->pnd->update->path &&
      !(object->flags & PNDMAN_PACKAGE_INSTALL_DESKTOP) &&
      !(object->flags & PNDMAN_PACKAGE_INSTALL_MENU)    &&
      !(object->flags & PNDMAN_PACKAGE_INSTALL_APPS)) {
      /* get basename of old path (update) */
      if (!(relative = strdup(object->pnd->update->path)))
         goto fail;
      dirname(relative);
   } else {
      char *place = "desktop";
      if (object->flags & PNDMAN_PACKAGE_INSTALL_MENU) place = "menu";
      else if (object->flags & PNDMAN_PACKAGE_INSTALL_APPS) place = "apps";

      int size = snprintf(NULL, 0, "pandora/%s", place)+1;
      if (!(relative = malloc(size)))
         goto fail;

      sprintf(relative, "pandora/%s", place);
   }

   /* parse download header */
   if (!(filename = _parse_filename_from_header((char*)handle->header.data))) {
      /* use PND's id as filename as fallback */
      int size = snprintf(NULL, 0, "%s.pnd", object->pnd->id)+1;
      if (!(filename = malloc(size))) goto fail;
      sprintf(filename, "%s.pnd", object->pnd->id);
   }

   /* check if we have same pnd id installed already,
    * skip the search if this is update. We know old one already. */
   oldp = NULL;
   if (object->pnd->update) oldp = object->pnd->update;
   for (pnd = local->pnd; pnd && !oldp; pnd = pnd->next) {
      if (object->pnd->id && pnd->id && !strcmp(object->pnd->id, pnd->id) &&
          object->device->mount && pnd->mount && !strcmp(object->device->mount, pnd->mount))
         oldp = pnd;
   }

   /* temporary path used for conflict checking */
   int size = snprintf(NULL, 0, "%s/%s/%s", object->device->mount, relative, filename)+1;
   if (!(tmp = malloc(size))) goto fail;
   sprintf(tmp, "%s/%s/%s", object->device->mount, relative, filename);

   /* if there is conflict use pnd id as filename */
   if (!oldp || (_file_exist(tmp) && _conflict(object->pnd->id, relative, object->device, local))) {
      int size2 = snprintf(NULL, 0, "%s/%s", relative, object->pnd->id)+1;
      if (!(prefix = malloc(size2))) goto fail;
      sprintf(prefix, "%s/%s", relative, object->pnd->id);

      size2 = snprintf(NULL, 0, "%s.pnd", prefix)+1;
      IFDO(free, tmp);
      if (!(tmp = malloc(size2))) goto fail;
      sprintf(tmp, "%s.pnd", prefix);

      size2 = snprintf(NULL, 0, "%s/%s", object->device->mount, tmp)+1;
      IFDO(free, tmp2);
      if (!(tmp2 = malloc(size2))) goto fail;
      sprintf(tmp2, "%s/%s", object->device->mount, tmp);

      while (_file_exist(tmp2) && strcmp(relative, oldp?oldp->path:"/dev/null")) {
         uniqueid++;

         size2 = snprintf(NULL, 0, "%s_%d.pnd", prefix,uniqueid)+1;
         IFDO(free, tmp);
         if (!(tmp = malloc(size2))) goto fail;
         sprintf(tmp, "%s_%d.pnd", prefix, uniqueid);

         size2 = snprintf(NULL, 0, "%s/%s", object->device->mount, tmp)+1;
         IFDO(free, tmp2);
         if (!(tmp2 = malloc(size2))) goto fail;
         sprintf(tmp2, "%s/%s", object->device->mount, tmp);
      }

      IFDO(free, relative);
      relative = strdup(tmp);
      NULLDO(free, prefix); NULLDO(free, tmp); NULLDO(free, tmp2);
   } else {
      /* join filename to install path */
      IFDO(free, tmp);
      tmp = strdup(relative); free(relative);
      int size2 = snprintf(NULL, 0, "%s/%s", tmp, filename)+1;
      if (!(relative = malloc(size2))) goto fail;
      sprintf(relative, "%s/%s", tmp, filename);
      NULLDO(free, tmp);
   }

   IFDO(free, tmp);
   NULLDO(free, filename);

   /* backup? */
   if (oldp && (object->flags & PNDMAN_PACKAGE_BACKUP)) {
      _pndman_backup(oldp, object->device);
   } else {
      /* remove old pnd if no backup specified and path differs */
      if (oldp && oldp->path && strcmp(oldp->path, relative)) {
         if ((tmp = _pndman_pnd_get_path(oldp))) {
            unlink(tmp);
            NULLDO(free, tmp);
         }
      }
   }

   /* remove old pnd from repo */
   if (object->pnd->update)
      _pndman_repository_free_pnd(object->pnd->update, local);
   object->pnd->update = NULL;

   /* Copy the pnd object to local database
    * path should be always "" when installing from remote repository */
   pnd = _pndman_repository_new_pnd_check(object->pnd, relative, object->device->mount, local);
   if (!pnd) goto fail;
   _pndman_copy_pnd(pnd, object->pnd);

   /* complete install path */
   size = snprintf(NULL, 0, "%s/%s", object->device->mount, relative)+1;
   if (!(install = malloc(size))) goto fail;
   sprintf(install, "%s/%s", object->device->mount, relative);

   DEBUG(PNDMAN_LEVEL_CRAP, "install: %s", install);
   DEBUG(PNDMAN_LEVEL_CRAP, "from: %s", handle->path);
   if (_pndman_move_file(handle->path, install) != RETURN_OK)
      goto fail;

   /* mark installed */
   DEBUG(PNDMAN_LEVEL_CRAP, "install mark");
   IFDO(free, pnd->path);
   pnd->path = strdup(relative);
   IFDO(free, pnd->mount);
   pnd->mount = strdup(object->device->mount);

   free(install);
   free(relative);
   return RETURN_OK;

handle_no_dev:
   DEBFAIL(HANDLE_NO_DEV);
   goto fail;
handle_no_pnd:
   DEBFAIL(HANDLE_NO_PND);
   goto fail;
handle_no_dst:
   DEBFAIL(HANDLE_NO_DST);
   goto fail;
md5_fail:
   DEBFAIL(HANDLE_MD5_FAIL, object->pnd->id);
fail:
   IFDO(free, filename);
   IFDO(free, prefix);
   IFDO(free, relative);
   IFDO(free, install);
   IFDO(free, tmp);
   IFDO(free, tmp2);
   IFDO(free, md5);
   return RETURN_FAIL;
}

/* \brief post routine when handle has removal flag */
static int _pndman_package_handle_remove(pndman_package_handle *object, pndman_repository *local)
{
   FILE *f;
   char *path;
   assert(object && local);

   /* get full path to pnd */
   if (!(path = _pndman_pnd_get_path(object->pnd)))
      goto fail;

   /* sanity checks */
   if (!(f = fopen(path, "rb")))
      goto read_fail;
   fclose(f);

   /* remove */
   DEBUG(PNDMAN_LEVEL_CRAP, "remove: %s", path);
   if (unlink(path) != 0) goto fail;
   NULLDO(free, path);

   /* this can't be updated anymore */
   if (object->pnd->update)
      object->pnd->update->update = NULL;
   object->pnd->update = NULL;

   /* remove from local repo */
   return _pndman_repository_free_pnd(object->pnd, local);

read_fail:
   DEBFAIL(READ_FAIL, path);
fail:
   IFDO(free, path);
   return RETURN_FAIL;
}

/* INTERNAL API */

/* \brief handle callback */
void _pndman_package_handle_done(pndman_curl_code code, void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_api_status status;
   pndman_package_handle *handle  = (pndman_package_handle*)data;

   if (code == PNDMAN_CURL_FAIL) {
      IFDO(free, handle->error);
      if (info) handle->error = strdup(info);
   } else if (code == PNDMAN_CURL_DONE) {
      /* we success, but this might be a json error */
      if ((_pndman_json_client_api_return(chandle->file, &status) != RETURN_OK)) {
         code = PNDMAN_CURL_FAIL;
         IFDO(free, handle->error);
         if(status.text) handle->error = strdup(status.text);
      }
   }

   /* callback to this handle */
   if (handle->callback) handle->callback(code, handle);
}

/* API */

/* \brief Allocate new pndman_package_handle for transfer */
PNDMANAPI int pndman_package_handle_init(const char *name, pndman_package_handle *handle)
{
   CHECKUSE(handle);
   memset(handle, 0, sizeof(pndman_package_handle));
   if (name) handle->name = strdup(name);
   return RETURN_OK;
}

/* \brief Free pndman_package_handle */
PNDMANAPI void pndman_package_handle_free(pndman_package_handle *handle)
{
   pndman_curl_handle *chandle;
   CHECKUSEV(handle);

   chandle = (pndman_curl_handle*)handle->data;
   IFDO(_pndman_curl_handle_free, chandle);
   IFDO(free, handle->name);
   IFDO(free, handle->error);
   handle->data      = NULL;
   handle->callback  = NULL;
}

/* \brief Perform handle, currently only needed for INSTALL handles */
PNDMANAPI int pndman_package_handle_perform(pndman_package_handle *handle)
{
   CHECKUSE(handle);
   CHECKUSE(handle->pnd);
   CHECKUSE(handle->flags);

   /* reset done */
   handle->progress.done = 0;
   IFDO(free, handle->error);

   if ((handle->flags & PNDMAN_PACKAGE_INSTALL))
      if (_pndman_package_handle_download(handle) != RETURN_OK) {
         handle->flags = 0; /* don't allow continue */
         return RETURN_FAIL;
      }

   return RETURN_OK;
}

/* \brief Commit handle's state to ram
 * remember to call pndman_commit afterwards! */
PNDMANAPI int pndman_package_handle_commit(pndman_package_handle *handle, pndman_repository *local)
{
   CHECKUSE(handle);
   CHECKUSE(handle->flags);
   CHECKUSE(handle->pnd);
   CHECKUSE(local);
   clock_t now = clock();

   /* make this idiot proof */
   local = _pndman_repository_first(local);

   if ((handle->flags & PNDMAN_PACKAGE_REMOVE))
      if (_pndman_package_handle_remove(handle, local) != RETURN_OK)
         return RETURN_FAIL;
   if ((handle->flags & PNDMAN_PACKAGE_INSTALL))
      if (_pndman_package_handle_install(handle, local) != RETURN_OK)
         return RETURN_FAIL;

   DEBUG(PNDMAN_LEVEL_CRAP, "Handle commit took %.2f seconds", (double)(clock()-now)/CLOCKS_PER_SEC);
   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
