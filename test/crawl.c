#include "pndman.h"
#include "common.h"

int main(int argc, char **argv)
{
   pndman_repository *repository, *r;
   pndman_device     *device;
   pndman_package    *pnd;
   pndman_translated *t;
   char *cwd;

   puts("-!- TEST crawl");
   puts("");

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);

   cwd = common_get_path_to_fake_device();
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   repository = pndman_repository_init();
   if (!repository) err("allocating repo list failed");

   if (pndman_package_crawl(0, device, repository) == -1)
      err("crawling failed");

   puts("");
   for (r = repository; r; r = r->next) {
      printf("%s :\n", r->name);
      printf("   UPD: %s\n", r->updates);
      printf("   URL: %s\n", r->url);
      printf("   VER: %s\n", r->version);
      puts("");

      if (!r->prev) {
         for (pnd = r->pnd; pnd; pnd = pnd->next) {
            printf("ID:    %s\n", pnd->id);
            printf("ICON:  %s\n", pnd->icon);
            printf("MD5:   %s\n", pnd->md5);
            printf("URL:   %s\n", pnd->url);
            puts("\nTitles:");
            for (t = pnd->title; t; t = t->next)
               puts(t->string);
            puts("");
         }
      }
   }
   puts("");

   pndman_repository_free_all(repository);
   pndman_device_free_all(device);
   free(cwd);

   puts("");
   puts("-!- DONE");
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
