#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#ifdef __linux__
#  include <mntent.h>
#  include <sys/vfs.h>
#  include <sys/stat.h>
#  include <sys/statvfs.h>
#  define LINUX_MTAB "/etc/mtab"
#endif

#include "pndman.h"
#include "package.h"
#include "repository.h"
#include "device.h"

/* used also by internal functions */
int pndman_device_init(pndman_device *device);

/* \brief Find first device */
static inline pndman_device* _pndman_device_first(pndman_device *device)
{
   /* find first */
   for(; device->prev; device = device->prev);
   return device;
}

/* \brief Find last device */
static inline pndman_device* _pndman_device_last(pndman_device *device)
{
   /* find last */
   for(; device->next; device = device->next);
   return device;
}

static int _check_create_tree_dir(char *path)
{
   if (access(path, F_OK) != 0) {
      if (errno == EACCES)
         return RETURN_FAIL;
#ifdef __WIN32__
      if (mkdir(path) == -1)
#else
      if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
#endif
         return RETURN_FAIL;
   }
   return RETURN_OK;
}

static int _pndman_device_check_pnd_tree(pndman_device *device)
{
   char tmp[PATH_MAX];
   char tmp2[PATH_MAX];
   assert(device);

   /* <device>/pandora */
   strncpy(tmp, device->mount, PATH_MAX-1);
   strncat(tmp, "/pandora", PATH_MAX-1);
   if (_check_create_tree_dir(tmp) == -1)
      return RETURN_FAIL;

   /* <device>/apps */
   memcpy(tmp2, tmp, PATH_MAX);
   strncat(tmp2, "/apps", PATH_MAX-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/menu */
   memcpy(tmp2, tmp, PATH_MAX);
   strncat(tmp2, "/menu", PATH_MAX-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/desktop */
   memcpy(tmp2, tmp, PATH_MAX);
   strncat(tmp2, "/desktop", PATH_MAX-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/appdata */
   memcpy(tmp2, tmp, PATH_MAX);
   strncat(tmp2, "/appdata", PATH_MAX-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   /* <device>/appdata/<LIBPNDMAN_APPDATA_FOLDER> */
   strncat(tmp2, "/"PNDMAN_APPDATA, PATH_MAX-1);
   if (_check_create_tree_dir(tmp2) == -1)
      return RETURN_FAIL;

   memcpy(device->appdata, tmp2, PATH_MAX);
   return RETURN_OK;
}

/* \brief Allocate next device */
static int _pndman_device_new(pndman_device **device)
{
   pndman_device *new;
   assert(device);

   /* find last device */
   *device = _pndman_device_last(*device);
   new = calloc(1, sizeof(pndman_device));
   if (!new) return RETURN_FAIL;

   /* set defaults */
   pndman_device_init(new);
   new->prev       = *device;
   (*device)->next = new;
   *device         = new;
   return RETURN_OK;
}

/* \brief Allocate new if exists */
static int _pndman_device_new_if_exist(pndman_device **device, char *check_existing)
{
   pndman_device *d;

   if (check_existing) {
      d = _pndman_device_first(*device);
      for(; d && d->exist; d = d->next) {
         // DEBUGP("%s == %s\n", d->mount, check_existing);
         if (!strcmp(d->mount, check_existing)) return RETURN_FAIL;
      }
   }

   *device = _pndman_device_last(*device);
   if (!(*device)->exist) return RETURN_OK;
   return _pndman_device_new(device);
}

/* \brief Free one device */
static int _pndman_device_free(pndman_device *device)
{
   pndman_device *deleted;
   assert(device);

   /* avoid freeing the first device */
   if (device->prev) {
      /* set previous device point to the next device */
      device->prev->next    = device->next;

      /* set next point back to the previous device */
      if (device->next)
         device->next->prev = device->prev;

      free(device);
   }
   else {
      if (device->next) {
         /* copy next to this and free the next */
         strcpy(device->device, device->next->device);
         strcpy(device->mount,  device->next->mount);
         device->size       = device->next->size;
         device->free       = device->next->free;
         device->available  = device->next->available;
         device->exist      = device->next->exist;

         deleted = device->next;
         device->next       = deleted->next;

         if (device->next)
            device->next->prev = device;

         free(deleted);
      }
      else pndman_device_init(device);
   }

   return RETURN_OK;
}

/* \brief Free all devices */
static int _pndman_device_free_all(pndman_device *device)
{
   pndman_device *prev;
   assert(device);

   /* find the last device */
   device = _pndman_device_last(device);

   /* free everything */
   for(; device->prev; device = prev) {
      prev = device->prev;
      free(device);
   }
   pndman_device_init(device);

   return RETURN_OK;
}

/* \brief Stat absolute path, and fill the device struct according to that. */
static int _pndman_device_add_absolute(char *path, pndman_device *device)
{
#ifdef __linux__
   struct stat st;
   struct statvfs fs;

   if (stat(path, &st) != 0) return RETURN_FAIL;
   if (!S_ISDIR(st.st_mode)) {
      DEBUGP("%s: is not a directory\n", path);
      return RETURN_FAIL;
   }

   /* check for permissions */
   if (access(path, R_OK | W_OK) != 0)
      return RETURN_FAIL;

   if (statvfs(path, &fs) != 0)
      return RETURN_FAIL;

   /* create new if needed */
   if (_pndman_device_new_if_exist(&device, path) != RETURN_OK)
      return RETURN_FAIL;

   /* fill device struct */
   strncpy(device->mount,  path, PATH_MAX-1);
   if (_pndman_device_check_pnd_tree(device) != RETURN_OK) {
      _pndman_device_free(device);
      return RETURN_FAIL;
   }

   strncpy(device->device, path, PATH_MAX-1);
   device->free      = fs.f_bfree  * fs.f_bsize;
   device->size      = fs.f_blocks * fs.f_bsize;
   device->available = fs.f_bavail * fs.f_bsize;
   device->exist     = 1;
   _strip_slash(device->device);
   _strip_slash(device->mount);
#endif

   return RETURN_OK;
}

/* \brief
 * Check that path is a correct device, mount point or absolute path,
 * and fill the details to device struct according to that.
 */
static int _pndman_device_add(char *path, pndman_device *device)
{
#ifdef __linux__
   FILE *mtab;
   struct mntent *m;
   struct mntent mnt;
   struct statfs fs;
   char strings[4096];

   if (!path) return RETURN_FAIL;
   mtab = setmntent(LINUX_MTAB, "r");
   memset(strings, 0, 4096);
   while ((m = getmntent_r(mtab, &mnt, strings, sizeof(strings)))) {
      if (mnt.mnt_dir != NULL) {
         if (strcmp(mnt.mnt_fsname, path) == 0 ||
             strcmp(mnt.mnt_dir,    path) == 0  )
             break;
      }
      m = NULL;
   }
   endmntent(mtab);

   if (!m)
      return _pndman_device_add_absolute(path, device);

   /* check for read && write permissions */
   if (access(mnt.mnt_dir, R_OK | W_OK) != 0)
      return RETURN_FAIL;

   if (statfs(mnt.mnt_dir, &fs) != 0)
      return RETURN_FAIL;

   /* create new if needed */
   if (_pndman_device_new_if_exist(&device, mnt.mnt_dir) != RETURN_OK)
      return RETURN_FAIL;

   /* fill device struct */
   strncpy(device->mount,  mnt.mnt_dir,    PATH_MAX-1);
   strncpy(device->device, mnt.mnt_fsname, PATH_MAX-1);
   device->free      = fs.f_bfree  * fs.f_bsize;
   device->size      = fs.f_blocks * fs.f_bsize;
   device->available = fs.f_bavail * fs.f_bsize;
   device->exist     = 1;
   _strip_slash(device->device);
   _strip_slash(device->mount);
#elif __WIN32__
   char szName[PATH_MAX];
   char szDrive[3] = { ' ', ':', '\0' };
   szDrive[0] = path[0];
   ULARGE_INTEGER bytes_available, bytes_size, bytes_free;

   memset(szName, 0, PATH_MAX);
   if (!QueryDosDevice(szDrive, szName, PATH_MAX-1))
      return RETURN_FAIL;

   /* check for read && write perms */
   if (access(path, R_OK | W_OK) == -1)
      return RETURN_FAIL;

   if (!GetDiskFreeSpaceEx(szDrive,
        &bytes_available, &bytes_size, &bytes_free))
      return RETURN_FAIL;

   /* create new if needed */
   if (_pndman_device_new_if_exist(&device, szDrive) != RETURN_OK)
      return RETURN_FAIL;

   /* fill device struct */
   if (strlen(path) > 3 && strcmp(szDrive, path))
      strncpy(device->mount, path, PATH_MAX-1);
   else
      strncpy(device->mount,  szDrive, PATH_MAX-1);

   if (_pndman_device_check_pnd_tree(device) != RETURN_OK) {
      _pndman_device_free(device);
      return RETURN_FAIL;
   }

   strncpy(device->device, szName, PATH_MAX-1);
   device->free      = bytes_free.QuadPart;
   device->size      = bytes_size.QuadPart;
   device->available = bytes_available.QuadPart;
   device->exist     = 1;
   _strip_slash(device->device);
   _strip_slash(device->mount);
#endif

   return RETURN_OK;
}

/* \brief Detect devices and fill them automatically. */
static int _pndman_device_detect(pndman_device *device)
{
   pndman_device *old_device;
#ifdef __linux__
   FILE *mtab;
   struct mntent *m;
   struct mntent mnt;
   struct statfs fs;
   char strings[4096];
   int ret;

   ret = RETURN_OK;
   mtab = setmntent(LINUX_MTAB, "r");
   memset(strings, 0, 4096);
   while ((m = getmntent_r(mtab, &mnt, strings, sizeof(strings)))) {
      if ((mnt.mnt_dir != NULL) && (statfs(mnt.mnt_dir, &fs) == 0)) {
         if(strstr(mnt.mnt_fsname, "/dev/")	        != 0 &&
            strcmp(mnt.mnt_dir, "/")		        != 0 &&
            strcmp(mnt.mnt_dir, "/home")		!= 0 &&
            strcmp(mnt.mnt_dir, "/boot")		!= 0 )
         {
            /* check for read && write perms */
            if (access(mnt.mnt_dir, R_OK | W_OK) == -1)
               continue;

            old_device = device;
            if ((ret = _pndman_device_new_if_exist(&device, mnt.mnt_dir)) != RETURN_OK)
               break;

            DEBUGP("%s: %d\n", mnt.mnt_dir, device->exist);

            strncpy(device->mount,  mnt.mnt_dir,    PATH_MAX-1);
            if (_pndman_device_check_pnd_tree(device) == RETURN_OK) {
               strncpy(device->device, mnt.mnt_fsname, PATH_MAX-1);
               device->free      = fs.f_bfree  * fs.f_bsize;
               device->size      = fs.f_blocks * fs.f_bsize;
               device->available = fs.f_bavail * fs.f_bsize;
               device->exist     = 1;
               _strip_slash(device->device);
               _strip_slash(device->mount);
            } else {
               _pndman_device_free(device);
               device = old_device;
            }
         }
      }
      m = NULL;
   }
   endmntent(mtab);
#elif __WIN32__
   char szTemp[512];
   char szName[PATH_MAX-1];
   char szDrive[3] = { ' ', ':', '\0' };
   char *p;
   ULARGE_INTEGER bytes_free, bytes_available, bytes_size;
   int ret;

   memset(szTemp, 0, 512);
   if (!GetLogicalDriveStrings(511, szTemp))
      return RETURN_FAIL;

   ret = RETURN_OK; p = szTemp;
   while (*p) {
      *szDrive = *p;

      if (QueryDosDevice(szDrive, szName, PATH_MAX-1)) {
         /* check for read && write perms */
         if (access(szDrive, R_OK | W_OK) == -1)
         { while (*p++);  continue; }

         if (!GetDiskFreeSpaceEx(szDrive,
            &bytes_available, &bytes_size, &bytes_free))
         { while (*p++); continue; }

         old_device = device;
         if ((ret = _pndman_device_new_if_exist(&device, szDrive)) != RETURN_OK)
            break;

         DEBUGP("%s : %s\n", szDrive, szName);

         strncpy(device->mount,  szDrive, PATH_MAX-1);
         if (_pndman_device_check_pnd_tree(device) == RETURN_OK) {
            strncpy(device->device, szName,  PATH_MAX-1);
            device->free      = bytes_free.QuadPart;
            device->size      = bytes_size.QuadPart;
            device->available = bytes_available.QuadPart;
            device->exist     = 1;
            _strip_slash(device->device);
            _strip_slash(device->mount);
         } else {
            _pndman_device_free(device);
            device = old_device;
         }
      }

      /* go to the next NULL character. */
      while (*p++);
   }
#else
#  error "No device support for your OS"
#endif

   return ret;
}

/* API */

/* \brief Initialize device list, call this only once after declaring pndman_device */
int pndman_device_init(pndman_device *device)
{
   DEBUG("pndman device init");
   if (!device) return RETURN_FAIL;

   memset(device->device, 0, PATH_MAX);
   memset(device->mount,  0, PATH_MAX);
   memset(device->appdata,0, PATH_MAX);
   device->size      = 0;
   device->free      = 0;
   device->available = 0;
   device->exist     = 0;
   device->next = NULL;
   device->prev = NULL;

   return RETURN_OK;
}

/* \brief Add all found devices */
int pndman_device_detect(pndman_device *device)
{
   DEBUG("pndman device detect");
   if (!device) return RETURN_FAIL;
   return _pndman_device_detect(device);
}

/* \brief Add new device */
int pndman_device_add(char *path, pndman_device *device)
{
   DEBUG("pndman device add");
   if (!device) return RETURN_FAIL;
   return _pndman_device_add(path, device);
}

/* \brief Free one device */
int pndman_device_free(pndman_device *device)
{
   DEBUG("pndman device free");
   if (!device) return RETURN_FAIL;
   return _pndman_device_free(device);
}

/* \brief Free all devices */
int pndman_device_free_all(pndman_device *device)
{
   DEBUG("pndman device free all");
   if (!device) return RETURN_FAIL;
   return _pndman_device_free_all(device);
}

/* vim: set ts=8 sw=3 tw=0 :*/
