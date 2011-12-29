#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pndman.h"

#define TEST_URL "http://cloudef.eu/armpit/harem_animes.txt"
#define H_COUNT 5 /* handle count */
#define D_AGAIN 2 /* how many times to download again?
                     tests adding new downloads to the same queue, while others are active */

/* by default, download count should be 7 */

int main()
{
   pndman_handle handle[H_COUNT+1];
   int i;
   int still_running = 1;
   int again         = 0;
   size_t dcount     = 0;

   puts("This test, tests various curl operations within libpndman.");
   puts("");

   if (pndman_init() == -1)
      return EXIT_FAILURE;

   i = 0;
   for (; i != H_COUNT; ++i)
   {
      pndman_handle_init("download", &handle[i]);
      strcpy(handle[i].url, TEST_URL);
      pndman_handle_perform(&handle[i]);
   }

   /* download while running */
   while (still_running)
   {
      pndman_download(&still_running);

      /* check status */
      i = 0;
      for (; i != H_COUNT; ++i)
      {
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
