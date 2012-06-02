#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#ifdef __linux__
#  include <dirent.h>
#  include <mntent.h>
#  include <sys/vfs.h>
#  include <sys/stat.h>
#  include <sys/statvfs.h>
#  define LINUX_MTAB "/etc/mtab"
#endif

/* \brief creates new device */
static pndman_device* _pndman_device_init()
{
   pndman_device *device;

   if (!(device = malloc(sizeof(pndman_device))))
      goto fail;
   memset(device, 0, sizeof(pndman_device));

   /* return */
   return device;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_device");
   return NULL;
}

/* \brief Find first device */
inline pndman_device* _pndman_device_first(pndman_device *device)
{
   /* find first */
   for(; device->prev; device = device->prev);
   return device;
}

/* \brief Find last device */
inline pndman_device* _pndman_device_last(pndman_device *device)
{
   /* find last */
   for(; device->next; device = device->next);
   return device;
}

/* \brief remove temp files in appdata (don't create tree) */
static void _pndman_remove_tmp_files(pndman_device *device)
{
   char tmp[PNDMAN_PATH];
   char tmp2[PNDMAN_PATH];
#ifndef _WIN32
   DIR *dp;
   struct dirent *ep;
#else
   WIN32_FIND_DATA dp;
   HANDLE hFind = NULL;
#endif
   assert(device && device->mount);

   memset(tmp, 0, PNDMAN_PATH);
   memset(tmp2,0, PNDMAN_PATH);
   _pndman_device_get_appdata_no_create(tmp, device);
   if (!strlen(tmp)) return;

#ifndef _WIN32
   dp = opendir(tmp);
   if (!dp) return;
   while (ep = readdir(dp)) {
      if (strstr(ep->d_name, ".tmp")) {
         strncpy(tmp2, tmp, PNDMAN_PATH-1);
         strncat(tmp2, "/", PNDMAN_PATH-1);
         strncat(tmp2, ep->d_name, PNDMAN_PATH-1);
         remove(tmp2);
      }
   }
   closedir(dp);
#else
   strncpy(tmp2, tmp, PNDMAN_PATH-1);
   strncat(tmp2, "/*.tmp", PNDMAN_PATH-1);

   if ((hFind = FindFirstFile(tmp, &dp)) == INVALID_HANDLE_VALUE)
      return;

   do {
      strncpy(tmp2, tmp, PNDMAN_PATH-1);
      strncat(tmp2, "/", PNDMAN_PATH-1);
      strncat(tmp2, dp.cFileName, PNDMAN_PATH-1);
      remove(tmp2);
   } while (FindNextFile(hFind, &dp));
   FindClose(hFind);
#endif
}

static int _check_create_tree_dir(char *path)
{
   if (access(path, F_OK) != 0) {
      if (errno == EACCES)
         return RETURN_FAIL;
#ifdef _WIN32
      if (mkdir(path) == -1)
#else
      if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
#endif
         return RETURN_FAIL;
   }
   return RETURN_OK;
}

/* \brief check the pnd tree before writing to ->appdata */
static int _pndman_device_check_pnd_tree(pndman_device *device)
{
   char tmp[PNDMAN_PATH];
   char tmp2[PNDMAN_PATH];
   assert(device && strlen(device->mount));

   /* <device>/pandora */
   strncpy(tmp, device->mount, PNDMAN_PATH-1);
   strncat(tmp, "/pandora", PNDMAN_PATH-1);
   if (_check_create_tree_dir(tmp) == -1)
      return RETURN_FAIL;

   /* <device>/apps */
   memcpy(tmp2, tmp, PNDMAN_PATH);
   strncat(tmp2, "/apps", PNDMAN_PATH-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/menu */
   memcpy(tmp2, tmp, PNDMAN_PATH);
   strncat(tmp2, "/menu", PNDMAN_PATH-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/desktop */
   memcpy(tmp2, tmp, PNDMAN_PATH);
   strncat(tmp2, "/desktop", PNDMAN_PATH-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/appdata */
   memcpy(tmp2, tmp, PNDMAN_PATH);
   strncat(tmp2, "/appdata", PNDMAN_PATH-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/appdata/<LIBPNDMAN_APPDATA_FOLDER> */
   strncat(tmp2, "/"PNDMAN_APPDATA, PNDMAN_PATH-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   memcpy(device->appdata, tmp2, PNDMAN_PATH);
   return RETURN_OK;
}

/* \brief Allocate next device */
static pndman_device* _pndman_device_new(pndman_device **device)
{
   pndman_device *new;
   assert(device);

   /* find last device */
   *device = _pndman_device_last(*device);
   new = _pndman_device_init();
   if (!new) return NULL;

   /* set defaults */
   new->prev       = *device;
   (*device)->next = new;
   *device         = new;
   return *device;
}

/* \brief Allocate new if exists */
static pndman_device* _pndman_device_new_if_exist(pndman_device **device, const char *check_existing)
{
   pndman_device *d;

   if (!*device) return *device = _pndman_device_init();
   if (check_existing) {
      d = _pndman_device_first(*device);
      for(; d; d = d->next)
#ifdef _WIN32 /* windows is incasesensitive */
         if (!_strupcmp(d->mount, check_existing)) {
#else
         if (!strcmp(d->mount, check_existing)) {
#endif
            DEBFAIL(DEVICE_EXISTS, d->mount);
            return NULL;
         }
   }

   return _pndman_device_new(device);
}

/* \brief Free one device, returns the pointer to first device */
static pndman_device* _pndman_device_free(pndman_device *device)
{
   pndman_device *first;
   assert(device);

   /* set previous device point to the next device */
   if (device->prev)
      device->prev->next = device->next;

   /* set next point back to the previous device */
   if (device->next)
      device->next->prev = device->prev;

   /* get first item */
   first = device->prev ? _pndman_device_first(device) : device->next;

   /* remove temp files */
   _pndman_remove_tmp_files(device); /* remove tmp files */

   /* free the actual device */
   free(device);
   return first;
}

/* \brief Free all devices */
static void _pndman_device_free_all(pndman_device *device)
{
   pndman_device *next;
   assert(device);

   /* find the first device */
   device = _pndman_device_first(device);

   /* free everything */
   for(; device; device = next) {
      next = device->next;
      _pndman_remove_tmp_files(device);
      free(device);
   }
}

#ifdef __linux__
/* \brief Stat absolute path, and fill the device struct according to that. */
static pndman_device* _pndman_device_add_absolute(const char *path, pndman_device *device)
{
   struct stat st;
   struct statvfs fs;

   if (stat(path, &st) != 0) return NULL;
   if (!S_ISDIR(st.st_mode)) {
      DEBFAIL(DEVICE_IS_NOT_DIR, path);
      return NULL;
   }

   /* check for permissions */
   if (access(path, R_OK | W_OK) != 0) {
      DEBFAIL(ACCESS_FAIL, path);
      return NULL;
   }

   /* get root device */
   if (statvfs(path, &fs) != 0) {
      DEBFAIL(DEVICE_ROOT_FAIL, path);
      return NULL;
   }

   /* create new if needed */
   if (!_pndman_device_new_if_exist(&device, path))
      return NULL;

   /* fill device struct */
   strncpy(device->mount,  path, PNDMAN_PATH-1);
   strncpy(device->device, path, PNDMAN_PATH-1);
   device->free      = fs.f_bfree  * fs.f_bsize;
   device->size      = fs.f_blocks * fs.f_bsize;
   device->available = fs.f_bavail * fs.f_bsize;
   _strip_slash(device->device);
   _strip_slash(device->mount);

   return device;
}
#endif

/* \brief
 * Check that path is a correct device, mount point or absolute path,
 * and fill the details to device struct according to that.
 */
static pndman_device* _pndman_device_add(const char *path, pndman_device *device)
{
#ifdef __linux__
   FILE *mtab;
   struct mntent *m;
   struct mntent mnt;
   struct statfs fs;
   char strings[4096];

   if (!path) return NULL;
   mtab = setmntent(LINUX_MTAB, "r");
   memset(strings, 0, 4096);
   while ((m = getmntent_r(mtab, &mnt, strings, sizeof(strings)))) {
      if (mnt.mnt_dir != NULL) {
         if (!strcmp(mnt.mnt_fsname, path) ||
             !strcmp(mnt.mnt_dir,    path))
             break;
      }
      m = NULL;
   }
   endmntent(mtab);

   if (!m)
      return _pndman_device_add_absolute(path, device);

   /* check for read && write permissions */
   if (access(mnt.mnt_dir, R_OK | W_OK) != 0) {
      DEBFAIL(ACCESS_FAIL, mnt.mnt_dir);
      return NULL;
   }

   if (statfs(mnt.mnt_dir, &fs) != 0) {
      DEBFAIL(ACCESS_FAIL, mnt.mnt_dir);
      return NULL;
   }

   /* create new if needed */
   if (!_pndman_device_new_if_exist(&device, mnt.mnt_dir))
      return NULL;

   /* fill device struct */
   strncpy(device->mount,  mnt.mnt_dir,    PNDMAN_PATH-1);
   strncpy(device->device, mnt.mnt_fsname, PNDMAN_PATH-1);
   device->free      = fs.f_bfree  * fs.f_bsize;
   device->size      = fs.f_blocks * fs.f_bsize;
   device->available = fs.f_bavail * fs.f_bsize;
   _strip_slash(device->device);
   _strip_slash(device->mount);
#elif _WIN32
   char szName[PNDMAN_PATH];
   char szDrive[3] = { ' ', ':', '\0' };
   szDrive[0] = path[0];
   ULARGE_INTEGER bytes_available, bytes_size, bytes_free;

   memset(szName, 0, PNDMAN_PATH);
   if (!QueryDosDevice(szDrive, szName, PNDMAN_PATH-1)) {
      DEBFAIL(DEVICE_ROOT_FAIL, szName);
      return NULL;
   }

   /* check for read && write perms */
   if (access(path, R_OK | W_OK) == -1) {
      DEBFAIL(ACCESS_FAIL, path);
      return NULL;
   }

   if (!GetDiskFreeSpaceEx(szDrive,
        &bytes_available, &bytes_size, &bytes_free))
      return NULL;

   /* create new if needed */
   if (!_pndman_device_new_if_exist(&device, strlen(path)>3?path:szDrive))
      return NULL;

   /* fill device struct */
   if (strlen(path) > 3 && _strupcmp(szDrive, path))
      strncpy(device->mount, path, PNDMAN_PATH-1);
   else
      strncpy(device->mount,  szDrive, PNDMAN_PATH-1);
   strncpy(device->device, szName, PNDMAN_PATH-1);
   device->free      = bytes_free.QuadPart;
   device->size      = bytes_size.QuadPart;
   device->available = bytes_available.QuadPart;
   _strip_slash(device->device);
   _strip_slash(device->mount);
#endif

   return device;
}

/* \brief Detect devices and fill them automatically. */
static pndman_device* _pndman_device_detect(pndman_device *device)
{
   pndman_device *first;
#ifdef __linux__
   FILE *mtab;
   struct mntent *m;
   struct mntent mnt;
   struct statfs fs;
   char strings[4096], pandoradir[PNDMAN_PATH];

   first = device;
   mtab = setmntent(LINUX_MTAB, "r");
   memset(strings, 0, 4096); memset(pandoradir, 0, PNDMAN_PATH);
   while ((m = getmntent_r(mtab, &mnt, strings, sizeof(strings)))) {
      if ((mnt.mnt_dir != NULL) && (statfs(mnt.mnt_dir, &fs) == 0)) {
         if(strstr(mnt.mnt_fsname, "/dev")                   &&
            strcmp(mnt.mnt_dir, "/")		        != 0 &&
            strcmp(mnt.mnt_dir, "/home")		!= 0 &&
            strcmp(mnt.mnt_dir, "/boot")		!= 0
#ifdef PANDORA /* don't add /mnt/utmp entries, PND's are there */
            && !strstr(mnt.mnt_dir, "/mnt/utmp")
#endif
            )
         {
            /* check for read && write perms
             * test against both, / and /pandora,
             * this might be SD with rootfs install.
             * where only /pandora is owned by everyone. */
            strncpy(pandoradir, mnt.mnt_dir, PNDMAN_PATH-1);
            strncat(pandoradir, "/pandora", PNDMAN_PATH-1);
            if (access(pandoradir,  R_OK | W_OK) == -1 &&
                access(mnt.mnt_dir, R_OK | W_OK) == -1)
               continue;

            if (!_pndman_device_new_if_exist(&device, mnt.mnt_dir))
               break;

            DEBUG(PNDMAN_LEVEL_CRAP, "DETECT: %s", mnt.mnt_dir);
            strncpy(device->mount,  mnt.mnt_dir,    PNDMAN_PATH-1);
            strncpy(device->device, mnt.mnt_fsname, PNDMAN_PATH-1);
            device->free      = fs.f_bfree  * fs.f_bsize;
            device->size      = fs.f_blocks * fs.f_bsize;
            device->available = fs.f_bavail * fs.f_bsize;
            _strip_slash(device->device);
            _strip_slash(device->mount);
         }
      }
      m = NULL;
   }
   endmntent(mtab);
#elif _WIN32
   char szTemp[512], szName[PNDMAN_PATH-1];
   char szDrive[3] = { ' ', ':', '\0' };
   char pandoradir[PNDMAN_PATH], *p;
   ULARGE_INTEGER bytes_free, bytes_available, bytes_size;

   memset(szTemp, 0, 512); memset(pandoradir, 0, PNDMAN_PATH);
   if (!GetLogicalDriveStrings(511, szTemp))
      return NULL;

   first = device;
   p = szTemp;
   while (*p) {
      *szDrive = *p;

      if (QueryDosDevice(szDrive, szName, PNDMAN_PATH-1)) {
         /* check for read && write perms
          * test against both, / and /pandora,
          * this might be SD with rootfs install.
          * where only /pandora is owned by everyone. */
         strncpy(pandoradir, szDrive, PNDMAN_PATH-1);
         strncat(pandoradir, "/pandora", PNDMAN_PATH-1);
         if (access(pandoradir,  R_OK | W_OK) == -1 &&
             access(szDrive,     R_OK | W_OK) == -1)
         { while (*p++); continue; }

         if (!GetDiskFreeSpaceEx(szDrive,
            &bytes_available, &bytes_size, &bytes_free))
         { while (*p++); continue; }

         if (!_pndman_device_new_if_exist(&device, szDrive))
            break;

         DEBUG(PNDMAN_LEVEL_CRAP, "DETECT: %s : %s", szDrive, szName);

         strncpy(device->mount,  szDrive, PNDMAN_PATH-1);
         strncpy(device->device, szName,  PNDMAN_PATH-1);
         device->free      = bytes_free.QuadPart;
         device->size      = bytes_size.QuadPart;
         device->available = bytes_available.QuadPart;
         _strip_slash(device->device);
         _strip_slash(device->mount);
      }

      /* go to the next NULL character. */
      while (*p++);
   }
#else
#  error "No device support for your OS"
#endif

   if (first && first->next)  first = first->next;
   else if (device)           first = _pndman_device_first(device);
   return first;
}

/* INTERNAL */

/* \brief check's devices structure and returns appdata */
char* _pndman_device_get_appdata(pndman_device *device)
{
   assert(device);

   /* check that all needed dirs exist */
   if (_pndman_device_check_pnd_tree(device) != RETURN_OK)
      return NULL;

   return device->appdata;
}

/* \brief get appdata, return null if doesn't exist */
void _pndman_device_get_appdata_no_create(char *appdata, pndman_device *device)
{
   assert(device && appdata);
   memset(appdata, 0, PNDMAN_PATH);
   if (strlen(device->appdata) && access(device->appdata, F_OK) == RETURN_OK)
      strncpy(appdata, device->appdata, PNDMAN_PATH-1);
   else {
      strncpy(appdata, device->mount, PNDMAN_PATH-1);
      strncat(appdata, "/pandora", PNDMAN_PATH-1);
      strncat(appdata, "/appdata", PNDMAN_PATH-1);
      strncat(appdata, "/"PNDMAN_APPDATA, PNDMAN_PATH-1);
      if (access(appdata, F_OK) != RETURN_OK)
         memset(appdata, 0, PNDMAN_PATH);
   }
}

/* API */

/* \brief add all found devices */
PNDMANAPI pndman_device* pndman_device_detect(pndman_device *device)
{
   pndman_device *d, *d2;
   if ((d = _pndman_device_detect(device)))
      for (d2 = d; d2; d2 = d2->next) {
         _pndman_remove_tmp_files(d2);
         _pndman_device_get_appdata_no_create(d2->appdata, d2);
      }
   return d;
}

/* \brief add new device */
PNDMANAPI pndman_device* pndman_device_add(const char *path, pndman_device *device)
{
   pndman_device *d;
   CHECKUSEP(path);

   if ((d = _pndman_device_add(path, device))) {
      _pndman_remove_tmp_files(d);
      _pndman_device_get_appdata_no_create(d->appdata, d);
   }
   return d;
}

/* \brief free one device */
PNDMANAPI pndman_device* pndman_device_free(pndman_device *device)
{
   CHECKUSEP(device);
   return _pndman_device_free(device);
}

/* \brief free all devices */
PNDMANAPI void pndman_device_free_all(pndman_device *device)
{
   CHECKUSEV(device);
   _pndman_device_free_all(device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
