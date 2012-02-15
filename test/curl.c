#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pndman.h"

/* create these paths to test, local db writing */
#if __linux__
#  define TEST_ABSOLUTE "/tmp/libpndman"
#elif __WIN32__
#  define TEST_ABSOLUTE "C:/libpndman"
#else
#  error "No support yet"
#endif

#define TEST_URL "http://cloudef.eu/armpit/harem_animes.txt"
#define H_COUNT 5 /* handle count */
#define D_AGAIN 2 /* how many times to download again?
                     tests adding new downloads to the same queue, while others are active */

/* by default, download count should be 7 */

/* we need unique download identifiers, for now at least (might change) */
char *UNIQID[H_COUNT+1] = { "d1", "d2", "d3", "d4", "d5", "d6" };

int main()
{
   pndman_device device;
   pndman_handle handle[H_COUNT+1];
   int i;
   int again         = 0;
   size_t dcount     = 0;

   puts("This test, tests various curl operations within libpndman.");
   puts("");

   if (pndman_init() == -1)
      return EXIT_FAILURE;

   pndman_device_init(&device);
   pndman_device_add(TEST_ABSOLUTE, &device);

   i = 0;
   for (; i != H_COUNT; ++i) {
      pndman_handle_init(UNIQID[i], &handle[i]);
      strcpy(handle[i].url, TEST_URL);
      handle[i].device = &device;
      pndman_handle_perform(&handle[i]);
   }

   /* download while running,
    * in real use should check error (-1) */
   while (pndman_download() > 0) {
      /* check status */
      i = 0;
      for (; i != H_COUNT; ++i) {
         if (!handle[i].done) continue;
         puts("");
         printf("DONE: %s\n", handle[i].name);

         /* reset done and free */
         handle[i].done = 0;
         ++dcount;

         if (++again <= D_AGAIN)
            pndman_handle_perform(&handle[i]);
         else
            pndman_handle_free(&handle[i]);
      }
   }

   /* make sure that our handles are freed */
   i = 0;
   for (; i != H_COUNT; ++i)
      pndman_handle_free(&handle[i]);

   if (pndman_quit() == -1)
      return EXIT_FAILURE;

   puts("");
   printf("Download Count: %zu\n", dcount);

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
