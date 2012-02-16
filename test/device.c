#include <stdlib.h>
#include <stdio.h>
#include "pndman.h"

#if __linux__
#  define TEST_ABSOLUTE "/tmp/libpndman"
#elif __WIN32__
#  define TEST_ABSOLUTE "C:/libpndman"
#else
#  error "No support yet"
#endif

static void err(char *str)
{
   puts(str);
   exit(EXIT_FAILURE);
}

int main()
{
   pndman_device device, *d;
   int device_count;
   int i;

   puts("This test, tests various device operations within libpndman.");
   puts("It should find all devices and mount points which are writable, within your system.");
   puts("Additionally it tries to add "TEST_ABSOLUTE" path two times, where second time will fail.");
   puts("");
   puts("So if there exists more than one "TEST_ABSOLUTE", or additional devices which are not either writable, or do not exist on your system, this test fails.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   if (pndman_init() == -1)
      err("pndman_init failed");

   /* add some devices */
   pndman_device_init(&device);
   if (pndman_device_add(TEST_ABSOLUTE, &device) == -1)
      err("failed to add device "TEST_ABSOLUTE", does it exist?");
   pndman_device_detect(&device);
   if (pndman_device_add(TEST_ABSOLUTE, &device) == 0)
      err("second device add should fail!");

   /* print devices */
   puts("");
   device_count = 0;
   for(d = &device; d; d = d->next) {
      printf("%12s : %24s : %zu : %zu : %zu\n",
            d->device, d->mount, d->size, d->free, d->available);
      device_count++;
   }
   puts("");

   /* if count is more than one, do the next step */
   if (device_count > 1) {
      /* go to the middle device and free it */
      i = 0;
      d = &device;
      for(; i < device_count / 2; ++i)
         d = d->next;
      pndman_device_free(d);

      /* print devices */
      puts("");
      for(d = &device; d; d = d->next)
         printf("%12s : %24s : %zu : %zu : %zu\n",
               d->device, d->mount, d->size, d->free, d->available);
      puts("");
   }

   /* free first device */
   pndman_device_free(&device);

   /* print devices */
   puts("");
   for(d = &device; d; d = d->next)
      printf("%12s : %24s : %zu : %zu : %zu\n",
            d->device, d->mount, d->size, d->free, d->available);
   puts("");

   /* free everything */
   pndman_device_free_all(&device);

   if (pndman_quit() == -1)
      err("pndman_quit failed");

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
