#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pndman.h"

#define REPO_URL "http://repo.openpandora.org/includes/get_data.php"
#define H_COUNT 5 /* handle count */
#define D_AGAIN 2 /* how many times to download again?
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
   pndman_device device;
   pndman_package *pnd;
   pndman_repository repository, *repo;
   pndman_handle handle[H_COUNT+1];
   pndman_sync_handle sync_handle;
   int i;
   int again         = 0;
   size_t dcount     = 0;
   char *cwd;

   cwd = test_device();
   if (!cwd) err("failed to get virtual device path");

   puts("This test, tests various handle operations within libpndman.");
   printf("%s, is treated as virtual device.", cwd);
   puts("");

   if (pndman_init() == -1)
      err("pndman_init failed");

   pndman_device_init(&device);
   if (pndman_device_add(cwd, &device) == -1)
      err("failed to add device, check that it exists");

   pndman_repository_init(&repository);
   if (pndman_repository_add(REPO_URL, &repository) == -1)
      err("failed to add repository "REPO_URL", :/");

   /* the repository we added, is the next one.
    * first repository is always the local repository */
   repo = repository.next;

   /* read from device, then sync the repository we added */
   pndman_read_from_device(repo, &device);
   pndman_sync_request(&sync_handle, repo);
   while (pndman_sync() > 0) {
      if (sync_handle.done) {
          printf("%s : DONE!\n", sync_handle.repository->name);
         pndman_sync_request_free(&sync_handle);
      }
   }

   /* make sure it's freed */
   pndman_sync_request_free(&sync_handle);

   /* check that we actually got pnd's */
   if (!(pnd = repo->pnd))
      err("no PND's retivied from "REPO_URL", maybe it's down?");

   i = 0;
   for (; i != H_COUNT; ++i) {
      pndman_handle_init((char*)pnd->id, &handle[i]);
      handle[i].pnd    = pnd; /* lets download the first pnd from repository */
      handle[i].device = &device;
      handle[i].flags  = PNDMAN_HANDLE_INSTALL | PNDMAN_HANDLE_INSTALL_MENU;
      if (pndman_handle_perform(&handle[i]) == -1)
         err("failed to perform handle");
      pnd = pnd->next;
   }

   /* download while running,
    * in real use should check error (-1) */
   while (pndman_download() > 0) {
      /* check status */
      i = 0;
      for (; i != H_COUNT; ++i) {
         if (!handle[i].done) continue;
         puts("");
         printf("DONE: %s\n", handle[i].name);

         /* handle is downloaded, "commit" it, (installs) */
         if (pndman_handle_commit(&handle[i]) != 0)
            printf("commit failed for: %s\n", handle[i].name);

         /* reset done and free */
         handle[i].done = 0;
         ++dcount;

         if (++again <= D_AGAIN)
            pndman_handle_perform(&handle[i]);
         else
            pndman_handle_free(&handle[i]);
      }
   }

   /* make sure that our handles are freed */
   i = 0;
   for (; i != H_COUNT; ++i)
      pndman_handle_free(&handle[i]);

   pndman_repository_free_all(&repository);
   pndman_device_free_all(&device);

   free(cwd);
   if (pndman_quit() == -1)
      err("pndman_quit failed");

   puts("");
   printf("Download Count: %zu\n", dcount);

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
