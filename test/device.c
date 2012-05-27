#include "pndman.h"
#include "common.h"

int main(int argc, char **argv)
{
   pndman_device *device, *d;
   int device_count;
   int i;
   char *cwd;

   puts("-!- TEST device");
   puts("");

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);
   cwd = common_get_path_to_fake_device();

   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, does it exist?");
   pndman_device_detect(device);
   if (pndman_device_add(cwd, device))
      err("second device add should fail!");

   puts("");
   device_count = 0;
   for (d = device; d; d = d->next) {
      printf("%12s : %24s : %zu : %zu : %zu\n",
            d->device, d->mount, d->size, d->free, d->available);
      device_count++;
   }
   puts("");

   if (device_count > 1) {
      d = device;
      for (i = 0; i < device_count / 2; ++i)
         d = d->next;
      device = pndman_device_free(d);

      puts("");
      for(d = device; d; d = d->next)
         printf("%12s : %24s : %zu : %zu : %zu\n",
               d->device, d->mount, d->size, d->free, d->available);
      puts("");
   }

   device = pndman_device_free(device);

   puts("");
   for (d = device; d; d = d->next)
      printf("%12s : %24s : %zu : %zu : %zu\n",
            d->device, d->mount, d->size, d->free, d->available);
   puts("");

   pndman_device_free_all(device);
   free(cwd);

   puts("");
   puts("-!- DONE");
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
