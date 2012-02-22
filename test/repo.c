#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "pndman.h"

#define REPO_URL "http://repo.openpandora.org/includes/get_data.php"

static void err(char *str)
{
   puts(str);
   exit(EXIT_FAILURE);
}

#ifdef __WIN32__
#  define getcwd _getcwd
#elif __linux__
#  include <sys/stat.h>
#endif
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

int main()
{
   pndman_repository *repository, *r;
   pndman_device     *device, *d;
   pndman_package    *pnd;
   pndman_sync_handle handle[1]; /* we should have only one remote repository */
   unsigned int x;
   char *cwd;

   cwd = test_device();
   if (!cwd) err("failed to get virtual device path");

   puts("This test, tests various repository operations within libpndman.");
   puts("We'll first try to add "REPO_URL" two times, but since it already exists, second add should fail.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   if (pndman_init() != 0)
      err("pndman_init failed");

   /* add device */
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   /* add repository */
   repository = pndman_repository_init();
   if (!repository)
      err("failed to allocate repository list");

   if (!pndman_repository_add(REPO_URL, repository))
      err("failed to add "REPO_URL", :/");
   if (pndman_repository_add(REPO_URL, repository))
      err("second repo add should fail!");

   r = repository; x = 0;
   for (; r; r = r->next) {
      d = device;
      for (; d; d = d->next) pndman_read_from_device(r, d);

      /* skip the local repository */
      if (x) {
         if (pndman_sync_request(&handle[x-1], 0, r) != 0)
            err("pndman_sync_request failed");
      }
      ++x;
   }

   /* sync repositories
    * in real use should check error (-1) */
   puts("");
   while (pndman_sync() > 0) {
      for (x = 0; x != 1; ++x) {
         printf("%s [%.2f/%.2f]%s", handle[x].repository->url,
                handle[x].progress.download/1048576, handle[x].progress.total_to_download/1048576,
                handle[x].progress.done?"\n":"\r"); fflush(stdout);
         if (handle[x].progress.done) {
            printf("%s : DONE!\n", handle[x].repository->name);
            pndman_sync_request_free(&handle[x]);
         }
      }
   }
   puts("");

   /* make sure all sync handles are freed */
   for (x = 0; x != 1; ++x)
      pndman_sync_request_free(&handle[x]);

   puts("");
   r = repository;
   for (; r; r = r->next)
   {
      printf("%s :\n", r->name);
      printf("   UPD: %s\n", r->updates);
      printf("   URL: %s\n", r->url);
      printf("   VER: %s\n", r->version);
      printf("   TIM: %lu\n", r->timestamp);
      puts("");

      /* print only local repo packages if any,
       * run handle exe to get some :) */
      if (!r->prev) {
         for (pnd = r->pnd; pnd; pnd = pnd->next) {
            printf("ID:    %s\n", pnd->id);
            printf("ICON:  %s\n", pnd->icon);
            printf("MD5:   %s\n", pnd->md5);
            printf("URL:   %s\n", pnd->url);
            puts("");
         }
      }
   }
   puts("");

   /* commit all repositories to every device */
   d = device;
   for (; d; d = d->next)
      pndman_commit_all(repository, d);

   /* free everything */
   pndman_repository_free_all(repository);
   pndman_device_free_all(device);

   free(cwd);
   if (pndman_quit() == -1)
      err("pndman_quit failed");

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
