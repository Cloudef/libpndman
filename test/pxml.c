#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pndman.h"

#if __linux__
#  include <dirent.h>
#  define PND_DIR "/media/Storage/pandora/menu"
#elif __WIN32__
#  define PND_DIR "D:/pandora/menu"
#else
#  error "No support yet"
#endif

int main()
{
   DIR *dp;
   struct dirent *ep;
   char path1[PATH_MAX];
   char path2[PATH_MAX];

   puts("This test, tests various pxml operations within libpndman.");
   puts("");

   /* copy path */
   strncpy(path1, PND_DIR, PATH_MAX-1);
   strncat(path1, "/",     PATH_MAX-1);

   if (pndman_init() == -1)
      return EXIT_FAILURE;

#if 1
#ifdef __linux__
   dp = opendir(PND_DIR);
   if (!dp)
      return EXIT_FAILURE;

   while (ep = readdir (dp))
   {
      if (strstr(ep->d_name, ".pnd"))
      {
         strncpy(path2, path1,      PATH_MAX-1);
         strncat(path2, ep->d_name, PATH_MAX-1);
         puts(path2);
         pnd_do_something(path2);
      }
   }

   closedir(dp);
#endif
#else
   pnd_do_something("/media/Storage/pandora/menu/shootet.mrz.pnd");
#endif

   if (pndman_quit() == -1)
      return EXIT_FAILURE;

   return EXIT_SUCCESS;
}
