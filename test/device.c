#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "pndman.h"

static void err(char *str)
{
   puts(str);
   exit(EXIT_FAILURE);
}

#ifdef __WIN32__
#  define getcwd _getcwd
#elif __linux__
#  include <sys/stat.h>
#endif
static char* test_device()
{
   char *cwd = malloc(PATH_MAX);
   if (!cwd) return NULL;
   getcwd(cwd, PATH_MAX);
   strncat(cwd, "/SD", PATH_MAX-1);
   if (access(cwd, F_OK) != 0)
#ifdef __WIN32__
      if (mkdir(cwd) == -1) {
#else
      if (mkdir(cwd, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
#endif
         free(cwd);
         return NULL;
      }
   return cwd;
}

int main()
{
   pndman_device device, *d;
   int device_count;
   int i;
   char *cwd;

   puts("This test, tests various device operations within libpndman.");
   puts("It should find all devices and mount points which are writable, within your system.");
   puts("Additionally it tries to add virtual device path two times, where second time will fail.");
   puts("");
   puts("So if there exists more than one virtual device path, or additional devices which are not either writable, or do not exist on your system, this test fails.");
   puts("");
   puts("Additionally it does some free trickery, so there should be no memory leaks or segmentation faults either.");
   puts("");

   cwd = test_device();
   if (!cwd) err("failed to get virutal device path");

   if (pndman_init() == -1)
      err("pndman_init failed");

   /* add some devices */
   pndman_device_init(&device);
   if (pndman_device_add(cwd, &device) == -1)
      err("failed to add device, does it exist?");
   pndman_device_detect(&device);
   if (pndman_device_add(cwd, &device) == 0)
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
