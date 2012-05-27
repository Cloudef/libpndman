#include "pndman.h"
#include "common.h"

#define HANDLE_COUNT    4
#define REPOSITORY_URL "http://repo.openpandora.org/includes/get_data.php"
int main(int argc, char **argv)
{
   pndman_device *device;
   pndman_package *pnd;
   pndman_repository *repository, *repo;
   pndman_package_handle phandle[HANDLE_COUNT+1];
   pndman_sync_handle shandle[1];
   size_t i;
   char *cwd;

   puts("-!- TEST handle");
   puts("");

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);
   cwd = common_get_path_to_fake_device();
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   repository = pndman_repository_init();
   if (!(repo = pndman_repository_add(REPOSITORY_URL, repository)))
      err("failed to add repository "REPOSITORY_URL", :/");

   common_create_sync_handles(shandle, 1, repository, common_sync_cb,
         PNDMAN_SYNC_FULL);

   puts("");
   while (pndman_curl_process() > 0);
   puts("");

   /* check that we actually got pnd's */
   if (!(pnd = repo->pnd))
      err("no PND's retivied from "REPOSITORY_URL", maybe it's down?");

   common_create_package_handles(phandle, HANDLE_COUNT, device, repo, common_package_cb);
   for (i = 0; i != HANDLE_COUNT; ++i) { phandle[i].pnd = pnd; if (!(pnd = pnd->next)) break; }
   common_perform_package_handles(phandle, HANDLE_COUNT,
         PNDMAN_PACKAGE_INSTALL | PNDMAN_PACKAGE_INSTALL_MENU);

   puts("");
   while (pndman_curl_process() > 0);
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
