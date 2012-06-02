#include "pndman.h"
#include "common.h"

/* NOTE: we won't check errors here, so nasty things can happen */
int main(int argc, char **argv)
{
   pndman_device *device, *d;
   pndman_repository *repo, *r;

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);

#ifndef _WIN32
   device = pndman_device_add("/tmp", NULL);
   pndman_device *d2 = pndman_device_add("/media", device);
   pndman_device *d3 = pndman_device_add("/home", device);

   pndman_device_free(device);
   pndman_device_free(d2);
   pndman_device_free(d3);

   device = pndman_device_add("/tmp", NULL);
   pndman_device_detect(device);

#if 0
   puts("");
   for (d = device; d; d = d->next) {
      printf("%s : %s\n", d->device, d->mount);
      printf("next: %s\n", d->next ? d->next->device : "NULL");
      printf("prev: %s\n", d->prev ? d->prev->device : "NULL");
   }
   puts("");
#endif

#if 1 /* 1 for random order */
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
#endif /* _WIN32 */

   /* maybe make more intelligence in future
    * to prevent adding same place like the first 2 here */
   repo = pndman_repository_init();
   pndman_repository_add("http://cloudef.eu", repo);
   pndman_repository_add("http://cloudef.eu/", repo);
   pndman_repository_add("http://cloudef.eu/asd", repo);

#if 0
   puts("");
   for (r = repo; r; r = r->next) {
      printf("%s\n",strlen(r->name) ? r->name : r->url);
      printf("next: %s\n", r->next ? r->next->url : "NULL");
      printf("prev: %s\n", r->prev ? r->prev->url : "NULL");
   }
   puts("");
#endif

#if 1 /* 1 for random order */
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
