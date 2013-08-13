#include "pndman.h"
#include "common.h"

static void rate_cb(pndman_curl_code code, pndman_api_rate_packet *p)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   printf("USER_DATA: %s\n", (char*)p->user_data);

   if (code == PNDMAN_CURL_FAIL) {
      printf("%s\n", p->error);
      return;
   }

   if (p->total_rating)
      printf("new rating for %s is %d\n", p->pnd->id, p->total_rating);
   else
      printf("%d\n", p->rating);
}

static void comment_pull_cb(pndman_curl_code code, pndman_api_comment_packet *p)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   printf("USER_DATA: %s\n", (char*)p->user_data);

   if (code == PNDMAN_CURL_FAIL) {
      puts(p->error);
      return;
   }

   if (!p->pnd) {
      printf("no comments\n");
      return;
   }

   printf("%s [%lu] (%s.%s.%s.%s) // %s: %s\n",
         p->pnd->id, p->date,
         p->version->major, p->version->minor,
         p->version->release, p->version->build,
         p->username, p->comment);
}

static void history_cb(pndman_curl_code code, pndman_api_history_packet *p)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   printf("USER_DATA: %s\n", (char*)p->user_data);

   if (code == PNDMAN_CURL_FAIL) {
      puts(p->error);
      return;
   }

   if (!strlen(p->id)) {
      printf("no history\n");
      return;
   }

   printf("%s [%lu] (%s.%s.%s.%s)\n", p->id, p->download_date,
         p->version->major,p->version->minor,
         p->version->release, p->version->build);
}

static void archive_cb(pndman_curl_code code, pndman_api_archived_packet *p)
{
   pndman_package *pp;

   if (code == PNDMAN_CURL_PROGRESS)
      return;

   printf("USER_DATA: %s\n", (char*)p->user_data);

   if (code == PNDMAN_CURL_FAIL) {
      puts(p->error);
      return;
   }

   for (pp = p->pnd->next_installed; pp; pp = pp->next_installed)
      printf("%s [%lu] (%s.%s.%s.%s)\n", pp->id, pp->modified_time,
            pp->version.major,   pp->version.minor,
            pp->version.release, pp->version.build);
   if (!p->pnd->next_installed)
      printf("no archieved pnds\n");
}

static void generic_cb(pndman_curl_code code, const char *info, void *user_data)
{
   printf("USER_DATA: %s\n", (char*)user_data);
   if (code == PNDMAN_CURL_FAIL)
      puts(info);
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

#if 0
   pndman_device_read_repository(repo, device);
#else
   common_create_sync_handles(shandle, 1, repository, common_sync_cb,
         PNDMAN_SYNC_FULL);

   puts("");
   while (pndman_curl_process() > 0);
   puts("");
#endif

   pndman_repository_set_credentials(repo,
         "user", "key", 0);

   if (!repo->pnd)
      err("no PND's retivied from "REPOSITORY_URL", maybe it's down?");

   int c=0; /* hack to skip to second commercial PND in list
               (haven't bought the other one :P) */
   for (pnd = repo->pnd; pnd; pnd = pnd->next)
      if (pnd->commercial) { if (c) break; else c++; }
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
      pndman_api_comment_pnd("USERDATA", pnd, "test comment from libpndman", generic_cb);
      pndman_api_rate_pnd("USERDATA", pnd, 5, rate_cb);
      pndman_api_get_own_rate_pnd("USERDATA", pnd, rate_cb);
      pndman_api_comment_pnd_pull("USERDATA", pnd, comment_pull_cb);
      pndman_api_comment_pnd_delete("USERDATA", pnd, 1, generic_cb);
      pndman_api_download_history("USERDATA", repo, history_cb);
      pndman_api_archived_pnd("USERDATA", pnd, archive_cb);

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
