#include "internal.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>

#ifdef __linux__
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

/* \brief get backup directory */
static int _get_backup_dir(char *buffer, pndman_device *device)
{
   assert(buffer && device);
   strncpy(buffer, device->mount, PNDMAN_PATH-1);
   strncat(buffer, "/pandora", PNDMAN_PATH-1);
   strncat(buffer, "/backup", PNDMAN_PATH-1);
   if (access(buffer, F_OK) != 0) {
      if (errno == EACCES)
         return RETURN_FAIL;
#ifdef _WIN32
      if (mkdir(buffer) == -1)
#else
      if (mkdir(buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
#endif
            return RETURN_FAIL;
   }
   return RETURN_OK;
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
   char tmp[PNDMAN_PATH], bckdir[PNDMAN_PATH], path[PNDMAN_PATH];
   assert(pnd && device);

   /* get full path */
   _pndman_pnd_get_path(pnd, path);

   /* can't backup from void */
   if (_file_exist(path) != RETURN_OK)
      goto fail;

   /* get backup directory */
   if (_get_backup_dir(bckdir, device) != RETURN_OK)
      goto backup_fail;

   /* copy path */
   strncpy(tmp, bckdir, PNDMAN_PATH-1);
   strncat(tmp, "/", PNDMAN_PATH-1);
   strncat(tmp, pnd->id, PNDMAN_PATH-1);
   strncat(tmp, " - ", PNDMAN_PATH-1);
   strncat(tmp, pnd->version.major, PNDMAN_PATH-1);
   strncat(tmp, ".", PNDMAN_PATH-1);
   strncat(tmp, pnd->version.minor, PNDMAN_PATH-1);
   strncat(tmp, ".", PNDMAN_PATH-1);
   strncat(tmp, pnd->version.release, PNDMAN_PATH-1);
   strncat(tmp, ".", PNDMAN_PATH-1);
   strncat(tmp, pnd->version.build, PNDMAN_PATH-1);
   strncat(tmp, ".pnd", PNDMAN_PATH-1);

   DEBUG(PNDMAN_LEVEL_CRAP, "backup: %s -> %s", path, tmp);
   return _pndman_move_file(path, tmp);

backup_fail:
   DEBFAIL(HANDLE_BACKUP_DIR_FAIL, device);
fail:
   return RETURN_FAIL;
}

/* \brief check conflicts */
static int _conflict(char *id, char *path, pndman_repository *local)
{
   pndman_package *pnd;
   assert(id && path && local);

   for (pnd = local->pnd; pnd; pnd = pnd->next)
      if (strcmp(id,pnd->id) && !strcmp(path, pnd->path)) {
         DEBUG(PNDMAN_LEVEL_CRAP, "CONFLICT: %s - %s", pnd->id, pnd->path);
         return RETURN_TRUE;
      }
   return RETURN_FALSE;
}

/* \brief pre routine when object has install flag */
static int _pndman_package_handle_download(pndman_package_handle *object)
{
   char tmp_path[PNDMAN_PATH];
   char *appdata;
   pndman_device *d;
   pndman_curl_handle *handle;
   assert(object);

   if (!object->pnd)
      goto object_no_pnd;

   if (!strlen(object->pnd->url))
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
      if (!object->pnd->update || !strlen(object->pnd->update->path))
         goto object_wtf;
      for (d = _pndman_device_first(object->device);
           d && strcmp(d->mount, object->pnd->update->mount);
           d = d->next);
      if (!d) goto fail; /* can't find installed device */
      object->device = d; /* assign device, old pnd is installed on */
   }

   /* check appdata */
   appdata = _pndman_device_get_appdata(object->device);
   if (!appdata || !strlen(appdata))
      goto fail;

   snprintf(tmp_path, PNDMAN_PATH-1, "%s/%s.tmp", appdata, object->pnd->id);
   object->data = handle = _pndman_curl_handle_new(object, &object->progress,
         _pndman_package_handle_done, tmp_path);
   if (!handle) goto fail;

   /* commercial or logged download */
   if (object->repository && (object->pnd->commercial ||
            (object->flags & PNDMAN_PACKAGE_LOG_HISTORY))) {
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
   return RETURN_FAIL;
}

/* \brief parse filename from HTTP header */
static int _parse_filename_from_header(const char *filename,
      const char *haystack)
{
   char *needle = "filename=\"";
   char *file   = (char*)filename;
   int pos = 0, rpos = 0, found = 0;
   assert(filename && haystack);

   for(; rpos < strlen(haystack); rpos++) {
      if(!found) {
         if(haystack[rpos] == needle[pos]) {
            if(++pos==strlen(needle)) {
               found = 1;
               pos = 0;
            }
         } else
            pos = 0;
      } else {
         if(haystack[rpos] != '"')
            file[pos++] = haystack[rpos];
         else
            break;
      }
   }

   /* zero terminate parsed filename */
   file[pos] = '\0';

   if (!found)
      goto fail;

   return RETURN_OK;

fail:
   DEBUG(PNDMAN_LEVEL_WARN, HANDLE_HEADER_FAIL);
   return RETURN_FAIL;
}

/* \brief post routine when handle has install flag */
static int _pndman_package_handle_install(pndman_package_handle *object,
      pndman_repository *local)
{
   char install[PNDMAN_PATH];
   char relative[PNDMAN_PATH];
   char filename[PNDMAN_PATH];
   char tmp[PNDMAN_PATH];
   char *appdata, *md5 = NULL;
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

   memset(relative, 0, PNDMAN_PATH);
   memset(install, 0, PNDMAN_PATH);
   memset(filename, 0, PNDMAN_PATH);
   memset(tmp, 0, PNDMAN_PATH);

   /* check appdata */
   appdata = _pndman_device_get_appdata(object->device);
   if (!appdata || !strlen(appdata)) return RETURN_FAIL;

   /* check MD5 */
   md5 = _pndman_md5(handle->path);
   if (!md5 && !(object->flags & PNDMAN_PACKAGE_FORCE))
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "R: %s L: %s", object->pnd->md5, md5);
   DEBUG(PNDMAN_LEVEL_CRAP, "%s", handle->path);

   /* do check against remote */
   if (md5 && strcmp(md5, object->pnd->md5)) {
      if (!(object->flags & PNDMAN_PACKAGE_FORCE))
         goto md5_fail;
      else DEBUG(2, HANDLE_MD5_DIFF);
   } free(md5); md5 = NULL;

   if (object->pnd->update &&
      !(object->flags & PNDMAN_PACKAGE_INSTALL_DESKTOP) &&
      !(object->flags & PNDMAN_PACKAGE_INSTALL_MENU)    &&
      !(object->flags & PNDMAN_PACKAGE_INSTALL_APPS)) {
      /* get basename of old path (update) */
      strncpy(relative, object->pnd->update->path, PNDMAN_PATH-1);
      dirname(relative);
   } else {
      /* get install directory */
      strncpy(relative, "pandora", PNDMAN_PATH-1);
      if (object->flags & PNDMAN_PACKAGE_INSTALL_DESKTOP)
         strncat(relative, "/desktop", PNDMAN_PATH-1);
      else if (object->flags & PNDMAN_PACKAGE_INSTALL_MENU)
         strncat(relative, "/menu", PNDMAN_PATH-1);
      else if (object->flags & PNDMAN_PACKAGE_INSTALL_APPS)
         strncat(relative, "/apps", PNDMAN_PATH-1);
   }

   /* parse download header */
   if (_parse_filename_from_header(filename, (char*)handle->header.data) != RETURN_OK) {
      /* use PND's id as filename as fallback */
      strncpy(filename, object->pnd->id, PNDMAN_PATH-1);
      strncat(filename, ".pnd", PNDMAN_PATH-1); /* add extension! */
   }

   /* check if we have same pnd id installed already,
    * skip the search if this is update. We know old one already. */
   oldp = NULL;
   if (object->pnd->update) oldp = object->pnd->update;
   for (pnd = local->pnd; pnd && !oldp; pnd = pnd->next)
      if (!strcmp(object->pnd->id, pnd->id) &&
          !strcmp(object->pnd->mount, pnd->mount))
         oldp = pnd;

   /* temporary path used for conflict checking */
   strncpy(tmp, relative, PNDMAN_PATH-1);
   strncat(tmp, "/", PNDMAN_PATH-1);
   strncat(tmp, filename, PNDMAN_PATH-1);

   /* if there is conflict use pnd id as filename */
   if ((!oldp || _conflict(object->pnd->id, tmp, local)) && _file_exist(tmp)) {
      strncat(relative, "/", PNDMAN_PATH-1);
      strncat(relative, object->pnd->id, PNDMAN_PATH-1);
      snprintf(tmp, PNDMAN_PATH-1, "%s.pnd", relative);
      while (_file_exist(tmp) && strcmp(tmp, oldp?oldp->path:"/dev/null")) {
         uniqueid++;
         snprintf(tmp, PNDMAN_PATH-1, "%s_%d.pnd", relative, uniqueid);
      }
      strncpy(relative, tmp, PNDMAN_PATH-1);
   } else {
      /* join filename to install path */
      strncat(relative, "/", PNDMAN_PATH-1);
      strncat(relative, filename, PNDMAN_PATH-1);
   }

   /* backup? */
   if (oldp && (object->flags & PNDMAN_PACKAGE_BACKUP))
      _pndman_backup(oldp, object->device);
   else
   /* remove old pnd if no backup specified and path differs */
   if (oldp && strcmp(oldp->path, relative)) {
      _pndman_pnd_get_path(oldp, tmp);
      unlink(tmp);
   }

   /* remove old pnd from repo */
   if (object->pnd->update)
      _pndman_repository_free_pnd(object->pnd->update, local);
   object->pnd->update = NULL;

   /* Copy the pnd object to local database
    * path should be always "" when installing from remote repository */
   pnd = _pndman_repository_new_pnd_check(object->pnd, relative,
         object->device->mount, local);
   if (!pnd) goto fail;
   _pndman_copy_pnd(pnd, object->pnd);

   /* complete install path */
   strncpy(install, object->device->mount, PNDMAN_PATH-1);
   strncat(install, "/", PNDMAN_PATH-1);
   strncat(install, relative, PNDMAN_PATH-1);

   DEBUG(PNDMAN_LEVEL_CRAP, "install: %s", install);
   DEBUG(PNDMAN_LEVEL_CRAP, "from: %s", handle->path);
   if (_pndman_move_file(handle->path, install) != RETURN_OK)
      goto fail;

   /* mark installed */
   DEBUG(PNDMAN_LEVEL_CRAP, "install mark");
   strncpy(pnd->path, relative, PNDMAN_PATH-1);
   strncpy(pnd->mount, object->device->mount, PNDMAN_PATH-1);
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
   IFDO(free, md5);
   return RETURN_FAIL;
}

/* \brief post routine when handle has removal flag */
static int _pndman_package_handle_remove(pndman_package_handle *object, pndman_repository *local)
{
   FILE *f;
   char path[PNDMAN_PATH];
   assert(object && local);

   /* get full path to pnd */
   _pndman_pnd_get_path(object->pnd, path);

   /* sanity checks */
   if (!(f = fopen(path, "rb")))
      goto read_fail;
   fclose(f);

   /* remove */
   DEBUG(PNDMAN_LEVEL_CRAP, "remove: %s", path);
   if (unlink(path) != 0) goto fail;

   /* this can't be updated anymore */
   if (object->pnd->update)
      object->pnd->update->update = NULL;
   object->pnd->update = NULL;

   /* remove from local repo */
   return _pndman_repository_free_pnd(object->pnd, local);

read_fail:
   DEBFAIL(READ_FAIL, path);
fail:
   return RETURN_FAIL;
}

/* INTERNAL API */

/* \brief handle callback */
void _pndman_package_handle_done(pndman_curl_code code, void *data, const char *info,
      pndman_curl_handle *chandle)
{
   pndman_api_status status;
   pndman_package_handle *handle  = (pndman_package_handle*)data;

   if (code == PNDMAN_CURL_FAIL) {
      strncpy(handle->error, info, PNDMAN_STR-1);
   } else if (code == PNDMAN_CURL_DONE) {
      /* we success, but this might be a json error */
      if ((_pndman_json_client_api_return(chandle->file,
                  &status) != RETURN_OK) && strlen(status.text)) {
         code = PNDMAN_CURL_FAIL;
         strncpy(handle->error, status.text, PNDMAN_STR-1);
      }
   }

   /* callback to this handle */
   if (handle->callback) handle->callback(code, handle);
}

/* API */

/* \brief Allocate new pndman_package_handle for transfer */
PNDMANAPI int pndman_package_handle_init(const char *name, pndman_package_handle *handle)
{
   CHECKUSE(name);
   CHECKUSE(handle);

   memset(handle, 0, sizeof(pndman_package_handle));
   strncpy(handle->name, name, PNDMAN_NAME-1);
   return RETURN_OK;
}

/* \brief Free pndman_package_handle */
PNDMANAPI void pndman_package_handle_free(pndman_package_handle *handle)
{
   pndman_curl_handle *chandle;
   CHECKUSEV(handle);

   chandle = (pndman_curl_handle*)handle->data;
   IFDO(_pndman_curl_handle_free, chandle);
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
   memset(handle->error, 0, LINE_MAX);

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

   /* make this idiot proof */
   local = _pndman_repository_first(local);

   if ((handle->flags & PNDMAN_PACKAGE_REMOVE))
      if (_pndman_package_handle_remove(handle, local) != RETURN_OK)
         return RETURN_FAIL;
   if ((handle->flags & PNDMAN_PACKAGE_INSTALL))
      if (_pndman_package_handle_install(handle, local) != RETURN_OK)
         return RETURN_FAIL;

   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
