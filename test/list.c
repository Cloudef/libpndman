#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "pndman.h"

int main()
{
   pndman_device device, *d;
   pndman_repository repo, *r;

   pndman_device_init(&device);
   pndman_device_add("/tmp", &device);
   pndman_device_detect(&device);

   /* great for checking what's going on */
#if 0
   /* print the structure */
   puts("");
   for (d = &device; d; d = d->next) {
      printf("%s : %s\n", d->device, d->mount);
      printf("next: %s\n", d->next ? d->next->device : "NULL");
      printf("prev: %s\n", d->prev ? d->prev->device : "NULL");
   }
   puts("");
#endif

   /* 1 for random order */
#if 1
   srand(time(0));
   while (device.exist) {
      for (d = &device; (rand() % 2) && d && d->next; d = d->next);
      printf("free: %s\n", d->device);
      pndman_device_free(d);
   }
#else
   for (d = &device; d; d = d->next) {
      printf("free: %s\n", d->device);
      pndman_device_free(d);
   }
#endif

   /* maybe make more intelligence in future
    * to prevent adding same place like the first 2 here */
   pndman_repository_init(&repo);
   pndman_repository_add("http://cloudef.eu", &repo);
   pndman_repository_add("http://cloudef.eu/", &repo);
   pndman_repository_add("http://cloudef.eu/asd", &repo);

   /* 1 for random order */
#if 1
   srand(time(0));
   while (repo.next) { /* local repo will always exist! */
      for (r = &repo; (rand() % 2) && r && r->next; r = r->next);
      printf("free: %s\n", r->url);
      pndman_repository_free(r);
   }
#else
   for (r = &repo; r; r = r->next) {
      printf("free: %s\n", r->device);
      pndman_repository_free(r);
   }
#endif

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
