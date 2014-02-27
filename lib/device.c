#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#ifdef __linux__
#  include <dirent.h>
#  include <mntent.h>
#  include <sys/vfs.h>
#  include <sys/stat.h>
#  include <sys/statvfs.h>
#  define LINUX_MTAB "/etc/mtab" /* "/proc/mounts" */
#endif

#ifdef __APPLE__
#  include <dirent.h>
#  include <sys/stat.h>
#  include <sys/statvfs.h>
#  define MAC_VOLUMES "/Volumes"
#endif

#ifdef __APPLE__
#  include <malloc/malloc.h>
#else
#  include <malloc.h>
#endif

/* \brief creates new device */
static pndman_device* _pndman_device_init()
{
   pndman_device *device;

   if (!(device = calloc(1, sizeof(pndman_device))))
      goto fail;

   /* return */
   return device;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_device");
   return NULL;
}

static void _pndman_device_memory_free(pndman_device *device)
{
   IFDO(free, device->mount);
   IFDO(free, device->device);
   IFDO(free, device->appdata);
   free(device);
}

static void _pndman_device_set_mount(pndman_device *device, const char *mount)
{
   assert(device);
   IFDO(free, device->mount);
   if (mount) {
      device->mount = strdup(mount);
      if (strcmp(mount, "/")) _strip_slash(device->mount);
   }
}

static void _pndman_device_set_device(pndman_device *device, const char *cdevice)
{
   assert(device);
   IFDO(free, device->device);
   if (cdevice) {
      device->device = strdup(cdevice);
      if (strcmp(cdevice, "/")) _strip_slash(device->device);
   }
}

static void _pndman_device_set_appdata(pndman_device *device, const char *appdata)
{
   assert(device);
   IFDO(free, device->appdata);
   if (appdata) device->appdata = strdup(appdata);
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
   char *tmp = NULL;
#ifndef _WIN32
   DIR *dp;
   struct dirent *ep;
#else
   WIN32_FIND_DATA dp;
   HANDLE hFind = NULL;
#endif
   assert(device && device->mount);

   if (!(tmp = _pndman_device_get_appdata_no_create(device)))
      goto fail;

#ifndef _WIN32
   dp = opendir(tmp);
   if (!dp) return;
   while ((ep = readdir(dp))) {
      if (strstr(ep->d_name, ".tmp")) {
         char *tmp2;
         int size2 = snprintf(NULL, 0, "%s/%s", tmp, ep->d_name)+1;
         if (!(tmp2 = malloc(size2))) continue;
         sprintf(tmp2, "%s/%s", tmp, ep->d_name);
         remove(tmp2);
         free(tmp2);
      }
   }
   closedir(dp);
#else
   if ((hFind = FindFirstFile(tmp, &dp)) == INVALID_HANDLE_VALUE)
      return;

   do {
      char *tmp2;
      int size2 = snprintf(NULL, 0, "%s/%s", tmp, dp.cFileName)+1;
      if (!(tmp2 = malloc(size2))) continue;
      sprintf(tmp2, "%s/%s", tmp, dp.cFileName);
      remove(tmp2);
      free(tmp2);
   } while (FindNextFile(hFind, &dp));
   FindClose(hFind);
#endif

fail:
   IFDO(free, tmp);
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
   char *tmp = NULL;
   char *tmp2 = NULL;
   assert(device && device->mount);

   /* <device>/pandora */
   int size = snprintf(NULL, 0, "%s/pandora", device->mount)+1;
   if (!(tmp = malloc(size))) goto fail;
   sprintf(tmp, "%s/pandora", device->mount);
   if (_check_create_tree_dir(tmp) == -1)
      goto fail;

   /* <device>/apps */
   int size2 = snprintf(NULL, 0, "%s/apps", tmp)+1;
   if (!(tmp2 = malloc(size2))) goto fail;
   sprintf(tmp2, "%s/apps", tmp);
   if (_check_create_tree_dir(tmp2) == -1)
      goto fail;
   free(tmp2);

   /* <device>/menu */
   size2 = snprintf(NULL, 0, "%s/menu", tmp)+1;
   if (!(tmp2 = malloc(size2))) goto fail;
   sprintf(tmp2, "%s/menu", tmp);
   if (_check_create_tree_dir(tmp2) == -1)
      goto fail;
   free(tmp2);

   /* <device>/desktop */
   size2 = snprintf(NULL, 0, "%s/desktop", tmp)+1;
   if (!(tmp2 = malloc(size2))) goto fail;
   sprintf(tmp2, "%s/desktop", tmp);
   if (_check_create_tree_dir(tmp2) == -1)
      goto fail;
   free(tmp2);

   /* <device>/appdata */
   size2 = snprintf(NULL, 0, "%s/appdata", tmp)+1;
   if (!(tmp2 = malloc(size2))) goto fail;
   sprintf(tmp2, "%s/appdata", tmp);
   if (_check_create_tree_dir(tmp2) == -1)
      goto fail;
   free(tmp2);

   /* <device>/appdata/<LIBPNDMAN_APPDATA_FOLDER> */
   size2 = snprintf(NULL, 0, "%s/appdata/%s", tmp, PNDMAN_APPDATA)+1;
   if (!(tmp2 = malloc(size2))) goto fail;
   sprintf(tmp2, "%s/appdata/%s", tmp, PNDMAN_APPDATA);
   if (_check_create_tree_dir(tmp2) == -1)
      goto fail;

   _pndman_device_set_appdata(device, tmp2);
   free(tmp2);
   free(tmp);
   return RETURN_OK;

fail:
   IFDO(free, tmp);
   IFDO(free, tmp2);
   return RETURN_FAIL;
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
         if (d->mount && check_existing && !_strupcmp(d->mount, check_existing)) {
#else
         if (d->mount && check_existing && !strcmp(d->mount, check_existing)) {
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
   _pndman_device_memory_free(device);
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
      _pndman_device_memory_free(device);
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
   _pndman_device_set_mount(device, path);
   _pndman_device_set_device(device, path);
   device->free      = (uint64_t)fs.f_bfree  * (uint64_t)fs.f_bsize;
   device->size      = (uint64_t)fs.f_blocks * (uint64_t)fs.f_bsize;
   device->available = (uint64_t)fs.f_bavail * (uint64_t)fs.f_bsize;

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

   char *pandoradir;
   int size = snprintf(NULL, 0, "%s/pandora", mnt.mnt_dir)+1;
   if (!(pandoradir = malloc(size))) {
      DEBFAIL(OUT_OF_MEMORY);
      return NULL;
   }
   sprintf(pandoradir, "%s/pandora", mnt.mnt_dir);

   /* check for read && write permissions */
   if (access(mnt.mnt_dir, R_OK | W_OK) != 0 &&
       access(pandoradir, R_OK | W_OK) != 0) {
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
   _pndman_device_set_mount(device, mnt.mnt_dir);
   _pndman_device_set_device(device, mnt.mnt_fsname);
   device->free      = (uint64_t)fs.f_bfree  * (uint64_t)fs.f_bsize;
   device->size      = (uint64_t)fs.f_blocks * (uint64_t)fs.f_bsize;
   device->available = (uint64_t)fs.f_bavail * (uint64_t)fs.f_bsize;
#elif _WIN32
   char szName[1024];
   char szDrive[3] = { ' ', ':', '\0' };
   szDrive[0] = path[0];
   ULARGE_INTEGER bytes_available, bytes_size, bytes_free;

   memset(szName, 0, sizeof(szName));
   if (!QueryDosDevice(szDrive, szName, sizeof(szName)-1)) {
      DEBFAIL(DEVICE_ROOT_FAIL, szName);
      return NULL;
   }

   char *pandoradir;
   int size = snprintf(NULL, 0, "%s/pandora", path)+1;
   if (!(pandoradir = malloc(size))) {
      DEBFAIL(OUT_OF_MEMORY);
      return NULL;
   }
   sprintf(pandoradir, "%s/pandora", path);

   /* check for read && write perms */
   if (access(path, R_OK | W_OK) != 0 &&
       access(pandoradir, R_OK | W_OK) != 0) {
      DEBFAIL(ACCESS_FAIL, path);
      return NULL;
   }

   if (!GetDiskFreeSpaceEx(szDrive,
        &bytes_available, &bytes_size, &bytes_free)) {
      DEBFAIL(ACCESS_FAIL, szDrive);
      return NULL;
   }

   /* create new if needed */
   if (!_pndman_device_new_if_exist(&device, strlen(path)>3?path:szDrive))
      return NULL;

   /* fill device struct */
   if (path && strlen(path) > 3 && _strupcmp(szDrive, path)) {
      _pndman_device_set_mount(device, path);
   } else {
      _pndman_device_set_mount(device, szDrive);
   }

   _pndman_device_set_device(device, szName);
   device->free      = bytes_free.QuadPart;
   device->size      = bytes_size.QuadPart;
   device->available = bytes_available.QuadPart;
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
   char strings[4096];

   first = device;
   mtab = setmntent(LINUX_MTAB, "r");
   memset(strings, 0, 4096);
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
            char *pandoradir;
            int size = snprintf(NULL, 0, "%s/pandora", mnt.mnt_dir)+1;
            if (!(pandoradir = malloc(size))) continue;
            sprintf(pandoradir, "%s/pandora", mnt.mnt_dir);
            if (access(pandoradir,  R_OK | W_OK) == -1) {
               free(pandoradir);
               continue;
            }
            free(pandoradir);

            if (!_pndman_device_new_if_exist(&device, mnt.mnt_dir))
               break;

            DEBUG(PNDMAN_LEVEL_CRAP, "DETECT: %s", mnt.mnt_dir);
            _pndman_device_set_mount(device, mnt.mnt_dir);
            _pndman_device_set_device(device, mnt.mnt_fsname);
            device->free      = (uint64_t)fs.f_bfree  * (uint64_t)fs.f_bsize;
            device->size      = (uint64_t)fs.f_blocks * (uint64_t)fs.f_bsize;
            device->available = (uint64_t)fs.f_bavail * (uint64_t)fs.f_bsize;
         }
      }
      m = NULL;
   }
   endmntent(mtab);
#elif _WIN32
   char szTemp[512], szName[1024];
   char szDrive[3] = { ' ', ':', '\0' };
   char pandoradir[1024], *p;
   ULARGE_INTEGER bytes_free, bytes_available, bytes_size;

   memset(szTemp, 0, 512); memset(pandoradir, 0, sizeof(pandoradir));
   if (!GetLogicalDriveStrings(511, szTemp))
      return NULL;

   first = device;
   p = szTemp;
   while (*p) {
      *szDrive = *p;

      if (QueryDosDevice(szDrive, szName, sizeof(szName)-1)) {
         /* check for read && write perms
          * test against both, / and /pandora,
          * this might be SD with rootfs install.
          * where only /pandora is owned by everyone. */
         strncpy(pandoradir, szDrive, sizeof(pandoradir)-1);
         strncat(pandoradir, "/pandora", sizeof(pandoradir)-1);
         if (access(pandoradir,  R_OK | W_OK) == -1 &&
             access(szDrive,     R_OK | W_OK) == -1)
         { while (*p++); continue; }

         if (!GetDiskFreeSpaceEx(szDrive,
            &bytes_available, &bytes_size, &bytes_free))
         { while (*p++); continue; }

         if (!_pndman_device_new_if_exist(&device, szDrive))
            break;

         DEBUG(PNDMAN_LEVEL_CRAP, "DETECT: %s : %s", szDrive, szName);

         _pndman_device_set_mount(device, szDrive);
         _pndman_device_set_device(device, szName);
         device->free      = bytes_free.QuadPart;
         device->size      = bytes_size.QuadPart;
         device->available = bytes_available.QuadPart;
      }

      /* go to the next NULL character. */
      while (*p++);
   }
#else
   DEBUG(PNDMAN_LEVEL_ERROR, "No device support for your OS yet.");
   first = NULL;
#endif

   if (first && first->next)  first = first->next;
   else if (device)           first = _pndman_device_first(device);
   return first;
}

/* \brief update space information of device */
void _pndman_device_update(pndman_device *device)
{
   char *path = device->mount;
   if (!path) return;

#ifdef __linux__
   struct statfs fs;

   if (statfs(path, &fs) != 0) {
      DEBFAIL(ACCESS_FAIL, path);
      return;
   }

   device->free      = (uint64_t)fs.f_bfree  * (uint64_t)fs.f_bsize;
   device->size      = (uint64_t)fs.f_blocks * (uint64_t)fs.f_bsize;
   device->available = (uint64_t)fs.f_bavail * (uint64_t)fs.f_bsize;
#elif _WIN32
   char szName[1024];
   char szDrive[3] = { ' ', ':', '\0' };
   szDrive[0] = path[0];
   ULARGE_INTEGER bytes_available, bytes_size, bytes_free;

   memset(szName, 0, sizeof(szName));
   if (!QueryDosDevice(szDrive, szName, sizeof(szName)-1)) {
      DEBFAIL(DEVICE_ROOT_FAIL, szName);
      return;
   }

   if (!GetDiskFreeSpaceEx(szDrive,
        &bytes_available, &bytes_size, &bytes_free)) {
      DEBFAIL(ACCESS_FAIL, szDrive);
      return;
   }

   device->free      = bytes_free.QuadPart;
   device->size      = bytes_size.QuadPart;
   device->available = bytes_available.QuadPart;
#endif
}

/* INTERNAL */

/* \brief check's devices structure and returns appdata, free it*/
char* _pndman_device_get_appdata(pndman_device *device)
{
   assert(device);

   /* check that all needed dirs exist */
   if (_pndman_device_check_pnd_tree(device) != RETURN_OK)
      return NULL;

   if (!device->appdata) return NULL;
   return strdup(device->appdata);
}

/* \brief get appdata, free it */
char* _pndman_device_get_appdata_no_create(pndman_device *device)
{
   char *appdata = NULL;
   assert(device);

   if (device->appdata && access(device->appdata, F_OK) == 0) {
      if (!(appdata = strdup(device->appdata))) goto fail;
   } else {
      int size = snprintf(NULL, 0, "%s/pandora/appdata/%s", device->mount, PNDMAN_APPDATA)+1;
      if (!(appdata = malloc(size))) goto fail;
      sprintf(appdata, "%s/pandora/appdata/%s", device->mount, PNDMAN_APPDATA);
   }

   if (access(appdata, F_OK) != 0) goto fail;
   return appdata;

fail:
   IFDO(free, appdata);
   return NULL;
}

/* API */

/* \brief add all found devices */
PNDMANAPI pndman_device* pndman_device_detect(pndman_device *device)
{
   pndman_device *d, *d2;
   if ((d = _pndman_device_detect(device)))
      for (d2 = d; d2; d2 = d2->next) {
       /* removal of tmp files on device add
       * disabled for now at least.
       *
       * might be useful if some libpndman
       * application crashes and you can
       * continue download. */
#if 0
         _pndman_remove_tmp_files(d2);
#endif
         char *appdata;
         if ((appdata = _pndman_device_get_appdata_no_create(d2))) {
            _pndman_device_set_appdata(d2, appdata);
            free(appdata);
         }
      }
   return d;
}

/* \brief add new device */
PNDMANAPI pndman_device* pndman_device_add(const char *path, pndman_device *device)
{
   pndman_device *d;
   CHECKUSEP(path);

   if ((d = _pndman_device_add(path, device))) {
      /* removal of tmp files on device add
       * disabled for now at least.
       *
       * might be useful if some libpndman
       * application crashes and you can
       * continue download. */
#if 0
      _pndman_remove_tmp_files(d);
#endif
      char *appdata;
      if ((appdata = _pndman_device_get_appdata_no_create(d))) {
         _pndman_device_set_appdata(d, appdata);
         free(appdata);
      }
   }
   return d;
}

/* \brief update space information of device */
PNDMANAPI void pndman_device_update(pndman_device *device)
{
   CHECKUSEV(device);
   _pndman_device_update(device);
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
