#include <stdlib.h>
#include <stdio.h>
#include "pndman.h"

#if __linux__
#  define TEST_PND "/media/Storage/pandora/menu/milkyhelper.pnd"
#elif __WIN32__
#  define TEST_PND "D:/pandora/menu/milkyhelper.pnd"
#else
#  error "No support yet"
#endif

int main()
{
   puts("This test, tests various pxml operations within libpndman.");
   puts("");

   if (pndman_init() == -1)
      return EXIT_FAILURE;

   /* test function */
   pnd_do_something(TEST_PND);

   if (pndman_quit() == -1)
      return EXIT_FAILURE;

   return EXIT_SUCCESS;
}
