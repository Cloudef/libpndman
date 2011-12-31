#include <stdlib.h>
#include <stdio.h>
#include "pndman.h"

int main()
{
   pndman_repository repository, *r;

   puts("This test, tests various repository operations within libpndman.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   if (pndman_init() == -1)
      return EXIT_FAILURE;

   /* add repository */
   pndman_repository_init(&repository);

   /* free everything */
   pndman_repository_free_all(&repository);

   if (pndman_quit() == -1)
      return EXIT_FAILURE;

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
