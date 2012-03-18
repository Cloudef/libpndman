#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pndman.h"

#define REPO_URL "http://repo.openpandora.org/includes/get_data.php"
#define H_COUNT 1 /* handle count */
#define D_AGAIN 0 /* how many times to download again?
                     tests adding new downloads to the same queue, while others are active */

/* by default, download count should be 7 */
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
   pndman_device *device, *d;
   pndman_package *pnd;
   pndman_repository *repository, *repo;
   pndman_handle handle[H_COUNT+1];
   pndman_sync_handle sync_handle;
   int i;
   int again         = 0;
   size_t dcount     = 0;
   char *cwd;

   pndman_set_verbose(3);

   cwd = test_device();
   if (!cwd) err("failed to get virtual device path");

   puts("This test, tests various update operation.");
   printf("%s, is treated as virtual device.", cwd);
   puts("");

   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   repository = pndman_repository_init();
   if (!(repo = pndman_repository_add(REPO_URL, repository)))
      err("failed to add repository "REPO_URL", :/");

   /* read from device, then sync the repository we added */
   pndman_read_from_device(repo, device);
   pndman_sync_request(&sync_handle, PNDMAN_SYNC_FULL, repo);

   puts("");
   while (pndman_sync() > 0) {
      printf("%s [%.2f/%.2f]%s", sync_handle.repository->url,
             sync_handle.progress.download/1048576, sync_handle.progress.total_to_download/1048576,
             sync_handle.progress.done?"\n":"\r"); fflush(stdout);
      if (sync_handle.progress.done) {
         printf("%s : DONE!\n", sync_handle.repository->name);
         pndman_sync_request_free(&sync_handle);
      }
   }
   puts("");

   /* make sure it's freed */
   pndman_sync_request_free(&sync_handle);

   /* check that we actually got pnd's */
   if (!repo->pnd)
      err("no PND's retivied from "REPO_URL", maybe it's down?");

   for (d = device; d; d = d->next)
      pndman_crawl(0, d, repository);
   pndman_repository_check_local(repository);
   if (!pndman_check_updates(repository))
      err("no updates avaivable, this test will do nothing.");

   for (pnd = repo->pnd; pnd; pnd = pnd->next)
      if (pnd->update) break;
   if (!pnd->update)
      err("wut? could not find the pnd with update.");

   i = 0;
   for (; i != H_COUNT; ++i) {
      pndman_handle_init((char*)pnd->id, &handle[i]);
      handle[i].pnd    = pnd; /* lets download the first pnd from repository */
      handle[i].device = device;
      handle[i].flags  = PNDMAN_HANDLE_INSTALL;
      if (pndman_handle_perform(&handle[i]) == -1)
         err("failed to perform handle");
      pnd = pnd->next;
   }

   /* download while running,
    * in real use should check error (-1) */
   puts("");
   while (pndman_download() > 0) {
      /* check status */
      i = 0;
      for (; i != H_COUNT; ++i) {
         printf("%s [%.2f/%.2f]%s", handle[i].name,
               handle[i].progress.download/1048576, handle[i].progress.total_to_download/1048576,
               handle[i].progress.done?"\n":"\r"); fflush(stdout);
         if (!handle[i].progress.done) continue;
         printf("DONE: %s\n", handle[i].name);

         /* handle is downloaded, "commit" it, (installs) */
         if (pndman_handle_commit(&handle[i], repository) != 0)
            printf("commit failed for: %s\n", handle[i].name);

         ++dcount;
         if (++again <= D_AGAIN)
            pndman_handle_perform(&handle[i]);
         else
            pndman_handle_free(&handle[i]);
      }
   }
   puts("");

   for (d = device; d; d = d->next)
      pndman_crawl(0, d, repository);
   pndman_repository_check_local(repository);
   pndman_check_updates(repository);

   /* make sure that our handles are freed */
   i = 0;
   for (; i != H_COUNT; ++i)
      pndman_handle_free(&handle[i]);

   /* commit all repositories to every device */
   for (d = device; d; d = d->next)
      pndman_commit_all(repository, d);

   pndman_repository_free_all(repository);
   pndman_device_free_all(device);

   free(cwd);
   puts("");
   printf("Download Count: %zu\n", dcount);
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
