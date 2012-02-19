#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "pndman.h"

int main()
{
   pndman_device device, *d;

   pndman_device_init(&device);
   pndman_device_add("/tmp", &device);
   pndman_device_detect(&device);

   /* print the structure */
   puts("");
   for (d = &device; d; d = d->next) {
      printf("%s : %s\n", d->device, d->mount);
      printf("next: %s\n", d->next ? d->next->device : "NULL");
      printf("prev: %s\n", d->prev ? d->prev->device : "NULL");
   }
   puts("");

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

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
