#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <curl/curl.h>
#include "pndman.h"
#include "package.h"
#include "repository.h"
#include "device.h"
#include "curl.h"
#include "md5.h"

#ifdef __linux__
#  include <sys/stat.h>
#endif

#define HANDLE_NAME PND_ID

/* strings */
static const char *MD5_FAIL         = "MD5 check fail for %s\n";
static const char *MD5_DIFF         = "MD5 differ, but forcing anyways.\n";
static const char *MV_FAIL          = "Failed to move from %s, to %s\n";
static const char *RM_FAIL          = "Failed to remove file %s\n";
static const char *BACKUP_DIR_FAIL  = "Failed to create backup directory to device: %s\n";
static const char *CURLM_FAIL       = "Failed to init internal CURLM";
static const char *HANDLE_NO_PND    = "Handle has no PND!";
static const char *HANDLE_PND_URL   = "PND assigned to handle has invalid url.";
static const char *HANDLE_NO_DEV    = "Handle has no device! (install)";
static const char *HANDLE_NO_DEV_UP = "Handle has no device list! (update)";
static const char *HANDLE_NO_DST    = "Handle has no destination, nor the PND has upgrade.";
static const char *HANDLE_WTF       = "WTF. Something that should never happen, just happened!";
static const char *CURL_REQ_FAIL    = "Failed to init curl request.";
static const char *WRITE_FAIL       = "Failed to open %s for writing.\n";
static const char *READ_FAIL        = "Failed to open %s for reading.\n";
static const char *HEADER_FAIL      = "Failed to parse filename from HTTP header.";

/* \brief flags for handle to determite what to do */
typedef enum pndman_handle_flags
{
   PNDMAN_HANDLE_INSTALL         = 0x001,
   PNDMAN_HANDLE_REMOVE          = 0x002,
   PNDMAN_HANDLE_FORCE           = 0x004,
   PNDMAN_HANDLE_INSTALL_DESKTOP = 0x008,
   PNDMAN_HANDLE_INSTALL_MENU    = 0x010,
   PNDMAN_HANDLE_INSTALL_APPS    = 0x020,
   PNDMAN_HANDLE_BACKUP          = 0x040,
} pndman_handle_flags;

/* \brief pndman_handle struct */
typedef struct pndman_handle
{
   char           name[HANDLE_NAME];
   char           error[LINE_MAX];
   pndman_package *pnd;
   pndman_device  *device;
   unsigned int   flags;
   curl_progress  progress;

   /* internal */
   curl_request   request;
   FILE           *file;
} pndman_handle;

static CURLM *_pndman_curlm;

/* \brief move file, src -> dst */
static int _pndman_move_file(char* src, char* dst)
{
   FILE *f;

   /* remove dst, if exists */
   if ((f = fopen(dst, "r"))) {
      fclose(f);
      if (unlink(dst) != 0)
         goto rm_fail;
   }

   /* rename */
   if (rename(src, dst) != 0)
      goto mv_fail;

   return RETURN_OK;

rm_fail:
   DEBFAIL(RM_FAIL, dst);
   goto fail;
mv_fail:
   DEBFAIL(MV_FAIL, src, dst);
fail:
   return RETURN_FAIL;
}

/* \brief get backup directory */
static int _get_backup_dir(char *buffer, pndman_device *device)
{
   strcpy(buffer, device->mount);
   strncat(buffer, "/pandora", PATH_MAX-1);
   strncat(buffer, "/backup", PATH_MAX-1);
   if (access(buffer, F_OK) != 0) {
      if (errno == EACCES)
         return RETURN_FAIL;
#ifdef __WIN32__
      if (mkdir(buffer) == -1)
#else
      if (mkdir(buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
#endif
            return RETURN_FAIL;
   }
   return RETURN_OK;
}

/* \brief backup pnd */
static int _pndman_backup(pndman_package *pnd, pndman_device *device)
{
   char tmp[PATH_MAX], bckdir[PATH_MAX];

   /* can't backup from void */
   if (!strlen(pnd->path))
      return RETURN_FAIL;

   /* get backup directory */
   if (_get_backup_dir(bckdir, device) != RETURN_OK) {
      DEBFAIL(BACKUP_DIR_FAIL, device);
      return RETURN_FAIL;
   }

   /* copy path */
   strcpy(tmp, bckdir);
   strncat(tmp, "/", PATH_MAX-1);
   strncat(tmp, pnd->id, PATH_MAX-1);
   strncat(tmp, " - ", PATH_MAX-1);
   strncat(tmp, pnd->version.major, PATH_MAX-1);
   strncat(tmp, ".", PATH_MAX-1);
   strncat(tmp, pnd->version.minor, PATH_MAX-1);
   strncat(tmp, ".", PATH_MAX-1);
   strncat(tmp, pnd->version.release, PATH_MAX-1);
   strncat(tmp, ".", PATH_MAX-1);
   strncat(tmp, pnd->version.build, PATH_MAX-1);
   strncat(tmp, ".pnd", PATH_MAX-1);

   DEBUG(3, "backup: %s -> %s\n", pnd->path, tmp);
   return _pndman_move_file(pnd->path, tmp);
}

/* \brief check if file exists */
static int _file_exist(char *path)
{
   FILE *f;

   if ((f = fopen(path, "r"))) {
      fclose(f);
      return RETURN_TRUE;
   }

   return RETURN_FALSE;
}

/* \brief check conflicts */
static int _conflict(char *id, char *path, pndman_repository *local)
{
   pndman_package *pnd;
   for (pnd = local->pnd; pnd; pnd = pnd->next)
      if (strcmp(id,pnd->id) && !strcmp(path,pnd->path)) {
         DEBUG(3, "CONFLICT: %s - %s\n", pnd->id, pnd->path);
         return RETURN_TRUE;
      }
   return RETURN_FALSE;
}

/* \brief Internal allocation of curl multi handle with checks */
static int _pndman_curl_init(void)
{
   if (_pndman_curlm) return RETURN_OK;
   if (!(_pndman_curlm = curl_multi_init())) {
      DEBFAIL(CURLM_FAIL);
      return RETURN_FAIL;
   }

   return RETURN_OK;
}

/* \brief Internal free of curl multi handle with checks */
static int _pndman_curl_free(void)
{
   if (!_pndman_curlm) return RETURN_OK;
   curl_multi_cleanup(_pndman_curlm);
   _pndman_curlm = NULL;
   return RETURN_OK;
}

/* \brief pre routine when handle has install flag */
static int _pndman_handle_download(pndman_handle *handle)
{
   char tmp_path[PATH_MAX];
   char *appdata;
   pndman_device *d;

   if (!handle->pnd)
      goto handle_no_pnd;

   if (!strlen(handle->pnd->url))
      goto handle_pnd_url;

   if (!handle->device)
      goto handle_no_dev;

   if (!handle->pnd->update                             &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_DESKTOP) &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_MENU)    &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_APPS))
      goto handle_no_dst;

   if (handle->pnd->update &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_DESKTOP) &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_MENU)    &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_APPS)) {
      if (!handle->pnd->update || !strlen(handle->pnd->update->path))
         goto handle_wtf;
      for (d = _pndman_device_first(handle->device); d && strcmp(d->mount, handle->pnd->update->mount); d = d->next);
      if (!d) goto fail; /* can't find installed device */
      handle->device = d; /* assign device, old pnd is installed on */
   }

   /* reset curl */
   if (curl_init_request(&handle->request) != RETURN_OK)
      goto curl_req_fail;

   if (handle->pnd->commercial)
      _pndman_handshake(&handle->request, "http://repo.openpandora.org/includes/client_access.php", _MY_PRIVATE_API_KEY, "Cloudef");

   /* check appdata */
   appdata = _pndman_device_get_appdata(handle->device);
   if (!appdata || !strlen(appdata))
      goto fail;

   /* open file to write */
   snprintf(tmp_path, PATH_MAX-1, "%s/%p.tmp", appdata, handle);
   if (!(handle->file = fopen(tmp_path, "wb")))
      goto write_fail;

   /* set download URL */
   curl_easy_setopt(handle->request.curl, CURLOPT_URL,     handle->pnd->url);
   curl_easy_setopt(handle->request.curl, CURLOPT_PRIVATE, handle);
   curl_easy_setopt(handle->request.curl, CURLOPT_HEADERFUNCTION, curl_write_request);
   curl_easy_setopt(handle->request.curl, CURLOPT_WRITEFUNCTION, curl_write_file);
   curl_easy_setopt(handle->request.curl, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);
   curl_easy_setopt(handle->request.curl, CURLOPT_NOPROGRESS, 0);
   curl_easy_setopt(handle->request.curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
   curl_easy_setopt(handle->request.curl, CURLOPT_PROGRESSDATA, &handle->progress);
   curl_easy_setopt(handle->request.curl, CURLOPT_WRITEHEADER, &handle->request);
   curl_easy_setopt(handle->request.curl, CURLOPT_WRITEDATA, handle->file);
   curl_easy_setopt(handle->request.curl, CURLOPT_COOKIEFILE, "");

   /* add to multi interface */
   curl_multi_add_handle(_pndman_curlm, handle->request.curl);

   return RETURN_OK;

handle_no_pnd:
   DEBFAIL(HANDLE_NO_PND);
   goto fail;
handle_pnd_url:
   DEBFAIL(HANDLE_PND_URL);
   goto fail;
handle_no_dev:
   if (!handle->pnd->update) {
      DEBFAIL(HANDLE_NO_DEV);
   } else {
      DEBFAIL(HANDLE_NO_DEV_UP);
   }
   goto fail;
handle_no_dst:
   DEBFAIL(HANDLE_NO_DST);
   goto fail;
handle_wtf:
   DEBFAIL(HANDLE_WTF);
   goto fail;
curl_req_fail:
   DEBFAIL(CURL_REQ_FAIL);
   goto fail;
write_fail:
   DEBFAIL(WRITE_FAIL, tmp_path);
fail:
   curl_free_request(&handle->request);
   return RETURN_FAIL;
}

/* \brief parse filename from HTTP header */
static int _parse_filename_from_header(char *filename, pndman_handle *handle)
{
   char* haystack = (char*)handle->request.result.data;
   char* needle   = "filename=\"";
   int pos = 0, rpos = 0, found = 0;
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
            filename[pos++] = haystack[rpos];
         else
            break;
      }
   }

   /* zero terminate parsed filename */
   filename[pos] = '\0';

   if (!found) {
      DEBUG(2, HEADER_FAIL);
      return RETURN_FAIL;
   }

   return RETURN_OK;
}

/* \brief post routine when handle has install flag */
static int _pndman_handle_install(pndman_handle *handle, pndman_repository *local)
{
   char install[PATH_MAX];
   char filename[PATH_MAX];
   char tmp[PATH_MAX], tmp2[PATH_MAX];
   char *appdata, *md5 = NULL;
   int uniqueid = 0;
   pndman_package *pnd, *oldp;

   if (!handle->device)
      goto handle_no_dev;
   if (!handle->pnd)
      goto handle_no_pnd;

   if (!handle->pnd->update                             &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_DESKTOP) &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_MENU)    &&
       !(handle->flags & PNDMAN_HANDLE_INSTALL_APPS)) {
      DEBFAIL(HANDLE_NO_DST);
      return RETURN_FAIL;
   }

   /* check appdata */
   appdata = _pndman_device_get_appdata(handle->device);
   if (!appdata || !strlen(appdata)) return RETURN_FAIL;

   /* complete handle's path */
   snprintf(tmp, PATH_MAX-1, "%s/%p.tmp", appdata, handle);

   /* close the download file, so we can move it, and md5 check */
   if (handle->file) fclose(handle->file);
   handle->file = NULL;

   /* check MD5 */
   md5 = _pndman_md5(tmp);
   if (!md5 && !(handle->flags & PNDMAN_HANDLE_FORCE))
      goto fail;

   DEBUG(3, "R: %s L: %s\n", handle->pnd->md5, md5);

   /* do check against remote */
   if (md5 && strcmp(md5, handle->pnd->md5)) {
      if (!(handle->flags & PNDMAN_HANDLE_FORCE))
         goto md5_fail;
      else DEBUG(2, MD5_DIFF);
   } free(md5); md5 = NULL;

   if (handle->pnd->update &&
      !(handle->flags & PNDMAN_HANDLE_INSTALL_DESKTOP) &&
      !(handle->flags & PNDMAN_HANDLE_INSTALL_MENU)    &&
      !(handle->flags & PNDMAN_HANDLE_INSTALL_APPS)) {
      /* get basename of old path (update) */
      strcpy(install, handle->pnd->update->path);
      dirname(install);
   } else {
      /* get install directory */
      strncpy(install, handle->device->mount, PATH_MAX-1);
      strncat(install, "/pandora", PATH_MAX-1);
      if (handle->flags & PNDMAN_HANDLE_INSTALL_DESKTOP)
         strncat(install, "/desktop", PATH_MAX-1);
      else if (handle->flags & PNDMAN_HANDLE_INSTALL_MENU)
         strncat(install, "/menu", PATH_MAX-1);
      else if (handle->flags & PNDMAN_HANDLE_INSTALL_APPS)
         strncat(install, "/apps", PATH_MAX-1);
   }

   /* parse download header */
   if (_parse_filename_from_header(filename, handle) != RETURN_OK) {
      /* use PND's id as filename as fallback */
      strncpy(filename, handle->pnd->id, PATH_MAX-1);
      strncat(filename, ".pnd", PATH_MAX-1); /* add extension! */
   }

   /* check if we have same pnd id installed already,
    * skip the search if this is update. We know old one already. */
   oldp = NULL;
   if (handle->pnd->update) oldp = handle->pnd->update;
   for (pnd = local->pnd; pnd && !oldp; pnd = pnd->next)
      if (!strcmp(handle->pnd->id, pnd->id))
         oldp = pnd;

   /* temporary path used for conflict checking */
   strcpy(tmp2, install);
   strncat(tmp2, "/", PATH_MAX-1);
   strncat(tmp2, filename, PATH_MAX-1);

   /* if there is conflict use pnd id as filename */
   if ((!oldp || _conflict(handle->pnd->id, tmp2, local)) && _file_exist(tmp2)) {
      strncat(install, "/", PATH_MAX-1);
      strncat(install, handle->pnd->id, PATH_MAX-1);
      snprintf(tmp2, PATH_MAX-1, "%s.pnd", install);
      while (_file_exist(tmp2) && strcmp(tmp2, oldp?oldp->path:"/dev/null")) {
         uniqueid++;
         snprintf(tmp2, PATH_MAX-1, "%s_%d.pnd", install, uniqueid);
      }
      strcpy(install, tmp2);
   } else {
      /* join filename to install path */
      strncat(install, "/", PATH_MAX-1);
      strncat(install, filename, PATH_MAX-1);
   }

   /* backup? */
   if (oldp && (handle->flags & PNDMAN_HANDLE_BACKUP))
      _pndman_backup(oldp, handle->device);
   else
   /* remove old pnd if no backup specified and path differs */
   if (oldp && strcmp(oldp->path, install))
      unlink(oldp->path);

   /* remove old pnd from repo */
   if (handle->pnd->update)
      _pndman_repository_free_pnd(handle->pnd->update, local);
   handle->pnd->update = NULL;

   /* Copy the pnd object to local database
    * path should be always "" when installing from remote repository */
   pnd = _pndman_repository_new_pnd_check(handle->pnd->id, install, &handle->pnd->version, local);
   if (!pnd) goto fail;
   _pndman_copy_pnd(pnd, handle->pnd);

   DEBUG(3, "install: %s\n", install);
   DEBUG(3, "from: %s\n", tmp);
   if (_pndman_move_file(tmp, install) != RETURN_OK)
      goto fail;

   /* mark installed */
   DEBUG(3, "install mark");
   strcpy(pnd->path, install);
   strcpy(pnd->mount, handle->device->mount);
   return RETURN_OK;

handle_no_dev:
   DEBFAIL(HANDLE_NO_DEV);
   goto fail;
handle_no_pnd:
   DEBFAIL(HANDLE_NO_PND);
   goto fail;
md5_fail:
   DEBFAIL(MD5_FAIL, handle->pnd->id);
fail:
   if (handle->file) fclose(handle->file);
   IFREE(md5);
   return RETURN_FAIL;
}

/* \brief post routine when handle has removal flag */
static int _pndman_handle_remove(pndman_handle *handle, pndman_repository *local)
{
   FILE *f;

   /* sanity checks */
   if (!(f = fopen(handle->pnd->path, "r")))
      goto read_fail;
   fclose(f);

   /* remove */
   DEBUG(3, "remove: %s\n", handle->pnd->path);
   if (unlink(handle->pnd->path) != 0) goto fail;

   /* this can't be updated anymore */
   if (handle->pnd->update)
      handle->pnd->update->update = NULL;
   handle->pnd->update = NULL;

   /* remove from local repo */
   return _pndman_repository_free_pnd(handle->pnd, local);

read_fail:
   DEBFAIL(READ_FAIL, handle->pnd->path);
fail:
   return RETURN_FAIL;
}

/* API */

/* \brief Allocate new pndman_handle for transfer */
int pndman_handle_init(const char *name, pndman_handle *handle)
{
   if (!handle) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "You should pass reference to pndman_handle");
      return RETURN_FAIL;
   }

   memset(handle, 0, sizeof(pndman_handle));
   memset(&handle->request, 0, sizeof(curl_request));
   memset(&handle->request.result, 0, sizeof(curl_write_result));
   strncpy(handle->name, name, HANDLE_NAME-1);
   curl_init_progress(&handle->progress);

   if (_pndman_curl_init() != RETURN_OK)
      return RETURN_FAIL;

   return RETURN_OK;
}

/* \brief Free pndman_handle */
int pndman_handle_free(pndman_handle *handle)
{
   char tmp_path[PATH_MAX];
   char *appdata;

   if (!handle) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "handle pointer is NULL");
      return RETURN_FAIL;
   }

   /* free curl handle */
   if (_pndman_curlm)
      curl_multi_remove_handle(_pndman_curlm, handle->request.curl);

   /* free request */
   if (handle->request.curl)
      curl_free_request(&handle->request);

   /* get rid of the temporary file */
   if (handle->file) {
      fclose(handle->file);

      /* check appdata */
      appdata = _pndman_device_get_appdata(handle->device);
      if (appdata && strlen(appdata)) {
         snprintf(tmp_path, PATH_MAX-1, "%s/%p.tmp", appdata, handle);
         unlink(tmp_path);
      }
   }
   handle->file = NULL;

   return RETURN_OK;
}

/* \brief Perform handle, currently only needed for INSTALL handles */
int pndman_handle_perform(pndman_handle *handle)
{
   if (!handle) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "handle pointer is NULL");
      return RETURN_FAIL;
   }
   if (!handle->pnd) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "handle has no PND");
      return RETURN_FAIL;
   }
   if (!handle->flags) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "handle has no flags");
      return RETURN_FAIL;
   }

   /* reset done */
   handle->progress.done = 0;
   memset(handle->error, 0, LINE_MAX);

   if ((handle->flags & PNDMAN_HANDLE_INSTALL))
      if (_pndman_handle_download(handle) != RETURN_OK) {
         handle->flags = 0; /* don't allow continue */
         return RETURN_FAIL;
      }

   return RETURN_OK;
}

/* \brief Perform download on all handles
 * returns the number of downloads currently going on, on error returns -1 */
int pndman_download()
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
   pndman_handle *handle;

   /* we are done :) */
   if (!_pndman_curlm) return 0;

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

   /* update status of curl handles */
   while ((msg = curl_multi_info_read(_pndman_curlm, &msgs_left))) {
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &handle);
      if (msg->msg == CURLMSG_DONE) {
         handle->progress.done = msg->data.result==CURLE_OK?1:0;
         if (msg->data.result != CURLE_OK) strncpy(handle->error, curl_easy_strerror(msg->data.result), LINE_MAX);
      }
   }

   /* it's okay to get rid of this */
   if (!still_running) {
      _pndman_curl_free();

      /* fake so that we are still running, why?
       * this lets user to catch the final completed download/sync
       * in download/sync loop */
      still_running = 1;
   }

   return still_running;
}

/* \brief Commit handle's state to ram
 * remember to call pndman_commit afterwards! */
int pndman_handle_commit(pndman_handle *handle, pndman_repository *local)
{
   if (!handle) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "handle pointer is NULL");
      return RETURN_FAIL;
   }
   if (!handle->flags) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "handle has no flags");
      return RETURN_FAIL;
   }
   if (!handle->pnd) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "handle has no pnd");
      return RETURN_FAIL;
   }
   if (!local) {
      DEBUG(1, _PNDMAN_WRN_BAD_USE, "local pointer is NULL");
      return RETURN_FAIL;
   }

   /* make this idiot proof */
   local = _pndman_repository_first(local);

   if ((handle->flags & PNDMAN_HANDLE_REMOVE))
      if (_pndman_handle_remove(handle, local) != RETURN_OK)
         return RETURN_FAIL;
   if ((handle->flags & PNDMAN_HANDLE_INSTALL))
      if (_pndman_handle_install(handle, local) != RETURN_OK)
         return RETURN_FAIL;

   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
