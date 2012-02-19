#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "pndman.h"

int main()
{
   pndman_device device, *d;

   pndman_device_init(&device);
   pndman_device_add("/tmp", &device);
   pndman_device_detect(&device);

   for (d = &device; d; d = d->next)
      pndman_device_free(d);

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
