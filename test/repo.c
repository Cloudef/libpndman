#include "pndman.h"
#include "common.h"

#define REPO_URL "http://repo.openpandora.org/includes/get_data.php"

static void sync_done_cb(pndman_curl_code code, void *data)
{
   pndman_sync_handle *handle = (pndman_sync_handle*)data;
   printf("%s : DONE!\n", handle->repository->name);
   pndman_sync_handle_free(handle);
}

int main()
{
   pndman_repository *repository, *r;
   pndman_device     *device, *d;
   pndman_package    *pnd;
   pndman_sync_handle handle[1]; /* we should have only one remote repository */
   unsigned int x;
   char *cwd;

   puts("This test, tests various repository operations within libpndman.");
   puts("We'll first try to add "REPO_URL" two times, but since it already exists, second add should fail.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   /* set verbose level to max */
   pndman_set_verbose(PNDMAN_LEVEL_CRAP);

   /* get test device */
   if (!(cwd = test_device()))
      err("failed to get virtual device path");

   /* add device */
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   /* add repository */
   repository = pndman_repository_init();
   if (!repository)
      err("failed to allocate repository list");

   /* test duplicates */
   if (!pndman_repository_add(REPO_URL, repository))
      err("failed to add "REPO_URL", :/");
   if (pndman_repository_add(REPO_URL, repository))
      err("second repo add should fail!");

   /* read all repositories locally */
   repo_auto_read(repository, device);

   /* create and perform sync handles */
   sync_auto_create(handle, 1, repository, sync_done_cb);

   /* wait for repositories to sync */
   curl_loop();

   /* output information about repositories */
   puts("");
   r = repository;
   for (; r; r = r->next)
   {
      printf("%s :\n", r->name);
      printf("   UPD: %s\n", r->updates);
      printf("   URL: %s\n", r->url);
      printf("   VER: %s\n", r->version);
      printf("   API: %s\n", r->api.root);
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
   repo_auto_commit(repository, device);

   /* cleanup */
   pndman_repository_free_all(repository);
   pndman_device_free_all(device);
   free(cwd);

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
