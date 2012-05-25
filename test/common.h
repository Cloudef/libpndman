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
#endif

/* common error function */
static void err(const char *str)
{
   puts(str);
   exit(EXIT_FAILURE);
}

/* returns path to test device */
static char* test_device()
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
   return cwd;
}

/* read repositories from devices */
static void repo_auto_read(pndman_repository *rl, pndman_device *dl)
{
   pndman_repository *r = rl;
   pndman_device *d;
   for (; r; r = r->next)
      for (d = dl; d; d = d->next)
         pndman_read_from_device(r, d);
}

/* create and perform sync handles */
static void sync_auto_create(pndman_sync_handle *handle,
      size_t num, pndman_repository *list, pndman_callback cb)
{
   size_t i;
   pndman_repository *r = list->next;
   for (i = 0; r; r = r->next) {
      if (i > num) break;
      if (!(pndman_sync_handle_init(&handle[i])))
         err("pndman_sync_handle_init failed");
      handle[i].repository = r;
      handle[i].callback   = cb;
      pndman_sync_handle_perform(&handle[i]);
   }
}

/* auto commit all repositories */
static void repo_auto_commit(pndman_repository *rl, pndman_device *dl)
{
   pndman_device *d = dl;
   for (; d; d = d->next)
      pndman_commit_all(rl, d);
}

/* common curl loop */
static void curl_loop(void)
{
   puts("");
   while (pndman_curl_process() > 0) {
      for (x = 0; x != 1; ++x) {
         printf("%s [%.2f/%.2f]%s", handle[x].repository->url,
                handle[x].progress.download/1048576, handle[x].progress.total_to_download/1048576,
                handle[x].progress.done?"\n":"\r"); fflush(stdout);
      }
   }
   puts("");
}

#endif /* __common_h__
