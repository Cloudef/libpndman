#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pndman.h"

/* create these paths to test, local db writing */
#if __linux__
#  define TEST_ABSOLUTE "/tmp/libpndman"
#elif __WIN32__
#  define TEST_ABSOLUTE "C:/libpndman"
#else
#  error "No support yet"
#endif

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

int main()
{
   pndman_device device;
   pndman_repository repository, *repo;
   pndman_handle handle[H_COUNT+1];
   int i;
   int again         = 0;
   size_t dcount     = 0;

   puts("This test, tests various curl operations within libpndman.");
   puts("");

   if (pndman_init() == -1)
      err("pndman_init failed");

   pndman_device_init(&device);
   if (pndman_device_add(TEST_ABSOLUTE, &device) == -1)
      err("failed to add device "TEST_ABSOLUTE", check that it exists");

   pndman_repository_init(&repository);
   if (pndman_repository_add(REPO_URL, &repository) == -1)
      err("failed to add repository "REPO_URL", :/");

   /* the repository we added, is the next one.
    * first repository is always the local repository */
   repo = repository.next;

   /* read from device, then sync the repository we added */
   pndman_read_from_device(repo, &device);
   pndman_sync_request(repo);
   while (pndman_sync() > 0);

   /* check that we actually got pnd's */
   if (!repo->pnd)
      err("no PND's retivied from "REPO_URL", maybe it's down?");

   /* print some info */
   puts(repo->name);
   puts(repo->pnd->id);
   puts(repo->pnd->url);

   i = 0;
   for (; i != H_COUNT; ++i) {
      pndman_handle_init("DOWNLOAD", &handle[i]);
      handle[i].pnd    = repo->pnd; /* lets download the first pnd from repository */
      handle[i].device = &device;
      handle[i].flags  = PNDMAN_HANDLE_INSTALL;
      if (pndman_handle_perform(&handle[i]) == -1)
         err("failed to perform handle");
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

   if (pndman_quit() == -1)
      err("pndman_quit failed");

   puts("");
   printf("Download Count: %zu\n", dcount);

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
