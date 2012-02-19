#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "pndman.h"

pndman_device* getLast(pndman_device* d)
{
   pndman_device* last = d;
   while(last->next) last = last->next;
   return last;
}

/* note we won't check errors here, so nasty things can happen */
int main()
{
   pndman_device *device, *d;
   pndman_repository *repo, *r;

   device = pndman_device_add("/tmp", NULL);
   pndman_device *d2 = pndman_device_add("/media/Storage", device);
   pndman_device *d3 = pndman_device_add("/media/Anime", device);

   pndman_device_free(device);
   pndman_device_free(d2);
   pndman_device_free(d3);

   device = pndman_device_add("/tmp", NULL);
   pndman_device_detect(device);

   /* great for checking what's going on */
#if 0
   /* print the structure */
   puts("");
   for (d = device; d; d = d->next) {
      printf("%s : %s\n", d->device, d->mount);
      printf("next: %s\n", d->next ? d->next->device : "NULL");
      printf("prev: %s\n", d->prev ? d->prev->device : "NULL");
   }
   puts("");
#endif

   /* 1 for random order */
#if 1
   srand(time(0));
   while (device) {
      for (d = device; (rand() % 2) && d && d->next; d = d->next);
      printf("free: %s\n", d->device);
      device = pndman_device_free(d);
   }
#else
   for (d = device; d; d = d->next) {
      printf("free: %s\n", d->device);
      device = pndman_device_free(d);
   }
#endif

   /* maybe make more intelligence in future
    * to prevent adding same place like the first 2 here */
   repo = pndman_repository_init();
   pndman_repository_add("http://cloudef.eu", repo);
   pndman_repository_add("http://cloudef.eu/", repo);
   pndman_repository_add("http://cloudef.eu/asd", repo);


   /* great for checking what's going on */
#if 0
   /* print the structure */
   puts("");
   for (r = repo; r; r = r->next) {
      printf("%s\n",strlen(r->name) ? r->name : r->url);
      printf("next: %s\n", r->next ? r->next->url : "NULL");
      printf("prev: %s\n", r->prev ? r->prev->url : "NULL");
   }
   puts("");
#endif

   /* 1 for random order */
#if 1
   srand(time(0));
   while (repo) {
      for (r = repo; (rand() % 2) && r && r->next; r = r->next);
      printf("free: %s\n", strlen(r->name) ? r->name : r->url);
      repo = pndman_repository_free(r);
   }
#else
   for (r = repo; r; r = r->next) {
      printf("free: %s\n", strlen(r->name) ? r->name : r->url);
      repo = pndman_repository_free(r);
   }
#endif

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
