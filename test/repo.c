#include "pndman.h"
#include "common.h"

int main(int argc, char **argv)
{
   pndman_repository *repository, *r;
   pndman_device     *device;
   pndman_package    *pnd;
   pndman_sync_handle handle[1];
   char *cwd;

   puts("-!- TEST repo");
   puts("");

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);
   cwd = common_get_path_to_fake_device();
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   repository = pndman_repository_init();
   if (!repository)
      err("failed to allocate repository list");

   if (!pndman_repository_add(REPOSITORY_URL, repository))
      err("failed to add "REPOSITORY_URL", :/");
   if (pndman_repository_add(REPOSITORY_URL, repository))
      err("second repo add should fail!");

   common_read_repositories_from_device(repository, device);
   common_create_sync_handles(handle, 1, repository, common_sync_cb, 0);

   puts("");
   while (pndman_curl_process() > 0);
   puts("");

   puts("");
   for (r = repository; r; r = r->next) {
      printf("%s :\n", r->name);
      printf("   UPD: %s\n", r->updates);
      printf("   URL: %s\n", r->url);
      printf("   VER: %s\n", r->version);
      printf("   API: %s\n", r->api.root);
      printf("   TIM: %lu\n", r->timestamp);
      puts("");

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

   common_commit_repositories_to_device(repository, device);

   pndman_repository_free_all(repository);
   pndman_device_free_all(device);
   free(cwd);

   puts("");
   puts("-!- DONE");
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
