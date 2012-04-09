#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pndman.h"

static void sample_hook(const char *function, int verbose_level, const char *str)
{
   printf("[%d] %s :: %s\n", verbose_level, function, str);
}

int main()
{
   pndman_repository *repo, *r;
   int c = 0;

   /* pndman_set_verbose(3); */ /* use the hook below instead */
   pndman_set_debug_hook(sample_hook);

   repo = pndman_repository_init();
   pndman_repository_add("http://repo.openpandora.org/includes/get_data.php", repo);

   r = repo;
   for (; r; r = r->next) ++c;
   pndman_sync_handle handle[c]; r = repo;
   for (c = 0; r; r = r->next) pndman_sync_request(&handle[c++], PNDMAN_SYNC_FULL, r);

   puts("");
   while (pndman_sync() > 0) {
      r = repo;
      for (c = 0; r; r = r->next) {
         printf("%s [%.2f/%.2f]%s", handle[c].repository->url,
                handle[c].progress.download/1048576, handle[c].progress.total_to_download/1048576,
                handle[c].progress.done?"\n":"\r"); fflush(stdout);
         if (handle[c].progress.done) {
            printf("%s : DONE!\n", handle[c].repository->name);
            pndman_sync_request_free(&handle[c]);
         }
         c++;
      }
   }
   puts("");

   r = repo;
   for (c = 0; r; r = r->next) pndman_sync_request_free(&handle[c++]);
   pndman_repository_free_all(repo);
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
