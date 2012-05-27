#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef __common_h__
#define __common_h__

#ifdef __WIN32__
#  define getcwd _getcwd
#elif __linux__
#  include <sys/stat.h>
#  include <dirent.h>
#endif

#define REPOSITORY_URL "http://repo.openpandora.org/client/masterlist?com=true"

/* common error function */
static void err(const char *str)
{
   puts(str);
   exit(EXIT_FAILURE);
}

/* get last device from list */
static pndman_device* common_get_last_device(pndman_device* d)
{
   pndman_device* last = d;
   while(last->next) last = last->next;
   return last;
}

/* returns path to test device */
static char* common_get_path_to_fake_device()
{
   char *cwd = malloc(PATH_MAX);
   if (!cwd) return NULL;
   getcwd(cwd, PATH_MAX);
   strncat(cwd, "/SD", PATH_MAX-1);
   if (access(cwd, F_OK) != 0)
#ifdef __WIN32__
      if (mkdir(cwd) == -1) {
#else
      if (mkdir(cwd, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
#endif
         free(cwd);
         return NULL;
      }

   if (!cwd) err("failed to get virtual device path");
   printf("-!- %s\n", cwd);
   return cwd;
}

/* read repositories from devices */
static void common_read_repositories_from_device(
      pndman_repository *rl, pndman_device *dl)
{
   pndman_repository *r = rl;
   pndman_device *d;
   for (; r; r = r->next)
      for (d = dl; d; d = d->next)
         pndman_device_read_repository(r, d);
}

/* create and perform sync handles */
static void common_create_sync_handles(pndman_sync_handle *handle,
      size_t num, pndman_repository *list, pndman_callback cb,
      unsigned int flags)
{
   size_t i;
   pndman_repository *r = list->next;
   for (i = 0; r; r = r->next) {
      if (i > num) break;
      if (pndman_sync_handle_init(&handle[i]) != 0)
         err("pndman_sync_handle_init failed");
      handle[i].repository = r;
      handle[i].callback   = cb;
      handle[i].flags      = flags;
      pndman_sync_handle_perform(&handle[i]);
   }
}

/* create package handles */
static void common_create_package_handles(pndman_package_handle *handle,
      size_t num, pndman_device *device, pndman_repository *repo, pndman_callback cb)
{
   size_t i;
   for (i = 0; i != num; ++i) {
      if (pndman_package_handle_init("noname", &handle[i]) != 0)
         err("pndman_package_handle_init failed");
      handle[i].repository = repo;
      handle[i].device     = device;
      handle[i].callback   = cb;
   }
}

/* perform package handles */
static void common_perform_package_handles(pndman_package_handle *handle,
      size_t num, unsigned int flags)
{
   size_t i;
   for (i = 0; i != num; ++i) {
      handle[i].flags = flags;
      pndman_package_handle_perform(&handle[i]);
   }
}

/* auto commit all repositories */
static void common_commit_repositories_to_device(
      pndman_repository *rl, pndman_device *dl)
{
   pndman_device *d = dl;
   for (; d; d = d->next)
      pndman_repository_commit_all(rl, d);
}

/* common sync callback */
static void common_sync_cb(pndman_curl_code code, void *data)
{
   pndman_sync_handle *handle = (pndman_sync_handle*)data;

   if (code == PNDMAN_CURL_DONE ||
       code == PNDMAN_CURL_FAIL) {
      printf("%s : %s!\n", handle->repository->name,
            code==PNDMAN_CURL_DONE?"DONE":handle->error);
      pndman_sync_handle_free(handle);
   } else if (code == PNDMAN_CURL_PROGRESS) {
      printf("%s [%.2f/%.2f]%s", handle->repository->url,
            handle->progress.download/1048576, handle->progress.total_to_download/1048576,
            handle->progress.done?"\n":"\r"); fflush(stdout);
   }
}

/* common package callback */
static void common_package_cb(pndman_curl_code code, void *data)
{
   pndman_package_handle *handle = (pndman_package_handle*)data;

   if (code == PNDMAN_CURL_DONE ||
       code == PNDMAN_CURL_FAIL) {
      printf("%s : %s!\n", handle->pnd->id,
            code==PNDMAN_CURL_DONE?"DONE":handle->error);
      if (code == PNDMAN_CURL_DONE) {
         if (pndman_package_handle_commit(handle, handle->repository) != 0)
            printf("commit failed for: %s\n", handle->pnd->id);
      }
      pndman_package_handle_free(handle);
   } else if (code == PNDMAN_CURL_PROGRESS) {
      printf("%s [%.2f/%.2f]%s", handle->pnd->id,
            handle->progress.download/1048576, handle->progress.total_to_download/1048576,
            handle->progress.done?"\n":"\r"); fflush(stdout);
   }
}

#endif /* __common_h__ */
