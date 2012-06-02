#include "pndman.h"
#include "common.h"

static void comment_pull_cb(void *user_data,
      pndman_package *pnd, pndman_version *version, time_t date,
      const char *username, const char *comment)
{
   printf("%s [%zu] (%s.%s.%s.%s) // %s: %s\n", pnd->id, date,
         version->major, version->minor, version->release, version->build,
         username, comment);
}

static void history_cb(void *user_data,
      const char *id, pndman_version *version, time_t date)
{
   printf("%s [%zu] (%s.%s.%s.%s)\n", id, date,
         version->major, version->minor, version->release, version->build);
}

static void archive_cb(void *user_data, pndman_package *pnd)
{
   pndman_package *p;
   printf("got archive data for: %s\n", pnd->id);
   for (p = pnd->next_installed; p; p = p->next_installed)
      printf("%s [%zu] (%s.%s.%s.%s)\n", p->id, p->modified_time,
            p->version.major,   p->version.minor,
            p->version.release, p->version.build);
}

int main(int argc, char **argv)
{
   pndman_device *device;
   pndman_package *pnd;
   pndman_repository *repository, *repo;
   pndman_package_handle phandle[1];
   pndman_sync_handle shandle[1];
   char *cwd;

   puts("-!- TEST repo_api");
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

   pndman_repository_set_credentials(repo,
         "use", "key", 0);

   if (!repo->pnd)
      err("no PND's retivied from "REPOSITORY_URL", maybe it's down?");

   int c=0; /* hack to skip to second commercial PND in list
               (haven't bought the other one :P) */
   for (pnd = repo->pnd; pnd; pnd = pnd->next)
      if (pnd->commercial) if (c) break; else c++;
   if (!pnd) puts("commercial pnd not found, skipping test");

   if (pnd) {
      common_create_package_handles(phandle, 1, device, repo, common_package_cb);
      phandle[0].pnd = pnd;
      common_perform_package_handles(phandle, 1,
            PNDMAN_PACKAGE_INSTALL | PNDMAN_PACKAGE_INSTALL_MENU);

      puts("");
      while (pndman_curl_process() > 0);
      puts("");
   }

   for (pnd = repo->pnd; pnd && strcmp(pnd->id, "milkyhelper"); pnd = pnd->next);
   if (!pnd) puts("no milkyhelper pnd found, skipping test");

   if (pnd) {
      // pndman_api_comment_pnd(pnd, repo, "test comment from libpndman");
      // pndman_api_rate_pnd(pnd, repo, 100);
      pndman_api_comment_pnd_pull(NULL, pnd, repo, comment_pull_cb);
      pndman_api_download_history(NULL, repo, history_cb);
      pndman_api_archived_pnd(NULL, pnd, repo, archive_cb);

      puts("");
      while (pndman_curl_process() > 0);
      puts("");
   }

   pndman_repository_free_all(repository);
   pndman_device_free_all(device);
   free(cwd);

   puts("");
   puts("-!- DONE");
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
