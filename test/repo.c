#include <stdlib.h>
#include <stdio.h>
#include "pndman.h"

#define REPO_URL "http://repo.openpandora.org/includes/get_data.php"

int main()
{
   pndman_repository repository, *r;

   puts("This test, tests various repository operations within libpndman.");
   puts("We'll first try to add "REPO_URL" two times, but since it already exists, second add should fail.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   if (pndman_init() == -1)
      return EXIT_FAILURE;

   /* add repository */
   pndman_repository_init(&repository);
   pndman_repository_add(REPO_URL, &repository);
   pndman_repository_add(REPO_URL, &repository);

   /* sync repositories */
   pndman_repository_sync_all(&repository, NULL);

   puts("");
   r = &repository;
   for (; r; r = r->next)
      printf("%s : %s\n", r->url, r->name);
   puts("");

   /* free everything */
   pndman_repository_free_all(&repository);

   if (pndman_quit() == -1)
      return EXIT_FAILURE;

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
