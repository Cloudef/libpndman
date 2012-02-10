#include <stdlib.h>
#include <stdio.h>
#include "pndman.h"

/* create these paths to test, local db writing */
#if __linux__
#  define TEST_ABSOLUTE "/tmp/pndman"
#elif __WIN32__
#  define TEST_ABSOLUTE "C:/pndman"
#else
#  error "No support yet"
#endif

#define REPO_URL "http://repo.openpandora.org/includes/get_data.php"

int main()
{
   pndman_repository repository, *r;
   pndman_device     device;
   pndman_package   *pnd;

   puts("This test, tests various repository operations within libpndman.");
   puts("We'll first try to add "REPO_URL" two times, but since it already exists, second add should fail.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   if (pndman_init() == -1)
      return EXIT_FAILURE;

   /* add device */
   pndman_device_init(&device);
   pndman_device_add(TEST_ABSOLUTE, &device);

   /* add repository */
   pndman_repository_init(&repository);
   pndman_repository_add(REPO_URL, &repository);
   pndman_repository_add(REPO_URL, &repository);

   /* sync repositories */
   while (pndman_repository_sync(&repository, &device) == 1);

   puts("");
   r = &repository;
   for (; r; r = r->next)
   {
      printf("%s :\n", r->name);
      printf("   UPD: %s\n", r->updates);
      printf("   URL: %s\n", r->url);
      printf("   VER: %f\n", r->version);
      puts("");
      /*pnd = r->pnd;
      for (; pnd; pnd = pnd->next) {
         puts(pnd->id);
      }*/
   }
   puts("");

   /* commit, needs devices! */
   pndman_commit_database(&repository, &device);

   /* free everything */
   pndman_repository_free_all(&repository);
   pndman_device_free_all(&device);

   if (pndman_quit() == -1)
      return EXIT_FAILURE;

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
