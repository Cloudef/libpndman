#include <stdlib.h>
#include <stdio.h>
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

static void err(char *str)
{
   puts(str);
   exit(EXIT_FAILURE);
}

int main()
{
   pndman_repository repository, *r;
   pndman_device     device, *d;
   pndman_package   *pnd;

   puts("This test, tests various repository operations within libpndman.");
   puts("We'll first try to add "REPO_URL" two times, but since it already exists, second add should fail.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   if (pndman_init() != 0)
      err("pndman_init failed");

   /* add device */
   pndman_device_init(&device);
   if (pndman_device_add(TEST_ABSOLUTE, &device) == -1)
      err("failed to add device "TEST_ABSOLUTE", check that it exists");

   /* add repository */
   pndman_repository_init(&repository);
   if (pndman_repository_add(REPO_URL, &repository) == -1)
      err("failed to add "REPO_URL", :/");
   if (pndman_repository_add(REPO_URL, &repository) == 0)
      err("second repo add should fail!");

   r = &repository;
   for (; r; r = r->next) {
      d = &device;
      for (; d; d = d->next) pndman_read_from_device(r, d);
      pndman_sync_request(r);
   }

   /* sync repositories
    * in real use should check error (-1) */
   while (pndman_sync() > 0);

   puts("");
   r = &repository;
   for (; r; r = r->next)
   {
      printf("%s :\n", r->name);
      printf("   UPD: %s\n", r->updates);
      printf("   URL: %s\n", r->url);
      printf("   VER: %s\n", r->version);
      puts("");
      pnd = r->pnd;
      for (; pnd; pnd = pnd->next) {
         printf("ID:    %s\n", pnd->id);
         printf("ICON:  %s\n", pnd->icon);
         printf("MD5:   %s\n", pnd->md5);
         printf("URL:   %s\n", pnd->url);
      }
   }
   puts("");

   /* commit all repositories to every device */
   d = &device;
   for (; d; d = d->next)
      pndman_commit(&repository, d);

   /* free everything */
   pndman_repository_free_all(&repository);
   pndman_device_free_all(&device);

   if (pndman_quit() == -1)
      err("pndman_quit failed");

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
