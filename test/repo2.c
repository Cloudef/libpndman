#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "pndman.h"
int main()
{
   pndman_repository repo, *r;
   int c = 0;

   pndman_repository_init(&repo);
   pndman_repository_add("http://repo.openpandora.org/includes/get_data.php", &repo);

   r = &repo;
   for (; r; r = r->next) ++c;
   pndman_sync_handle handle[c]; r = &repo;
   for (c = 0; r; r = r->next) pndman_sync_request(&handle[c++], r);
   while (pndman_sync() > 0) {
      for (c = 0; r; r = r->next)
         if (handle[c++].done) {
            printf("%s : DONE!\n", handle[c].repository->name);
            pndman_sync_request_free(&handle[c]);
         }
   }

   pndman_repository_free_all(&repo);
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
