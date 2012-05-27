#include "pndman.h"
#include "common.h"

#define HANDLE_COUNT 4
int main(int argc, char **argv)
{
   pndman_device *device, *d;
   pndman_package *pnd;
   pndman_repository *repository, *repo;
   pndman_package_handle phandle[HANDLE_COUNT+1];
   pndman_sync_handle shandle[1];
   size_t i;
   char *cwd;

   puts("-!- TEST update");
   puts("");

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);
   cwd = common_get_path_to_fake_device();
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   repository = pndman_repository_init();
   if (!(repo = pndman_repository_add(REPOSITORY_URL, repository)))
      err("failed to add repository "REPOSITORY_URL", :/");

   common_read_repositories_from_device(repository, device);
   common_create_sync_handles(shandle, 1, repository, common_sync_cb, 0);

   puts("");
   while (pndman_curl_process() > 0);
   puts("");

   if (!repo->pnd)
      err("no PND's retivied from "REPOSITORY_URL", maybe it's down?");

   for (d = device; d; d = d->next) pndman_package_crawl(0, d, repository);
   pndman_repository_check_local(repository);
   if (!pndman_repository_check_updates(repository))
      err("no updates avaivable, this test will do nothing.");

   for (pnd = repo->pnd; pnd; pnd = pnd->next) if (pnd->update) break;
   if (!pnd->update)
      err("wut? could not find the pnd with update.");

   common_create_package_handles(phandle, HANDLE_COUNT, device, repo, common_package_cb);
   for (i = 0; i != HANDLE_COUNT; ++i) { phandle[i].pnd = pnd; if (!(pnd = pnd->next)) break; }
   common_perform_package_handles(phandle, HANDLE_COUNT, PNDMAN_PACKAGE_INSTALL);

   puts("");
   while (pndman_curl_process() > 0);
   puts("");

   for (d = device; d; d = d->next) pndman_package_crawl(0, d, repository);
   pndman_repository_check_local(repository);
   pndman_repository_check_updates(repository);

   common_commit_repositories_to_device(repository, device);
   pndman_repository_free_all(repository);
   pndman_device_free_all(device);
   free(cwd);

   puts("");
   puts("-!- DONE");
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
