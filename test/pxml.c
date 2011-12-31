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
#ifdef __linux__
   DIR *dp;
   struct dirent *ep;
#else
   WIN32_FIND_DATA dp;
   HANDLE hFind = NULL;
#endif
   char path1[PATH_MAX];
   char path2[PATH_MAX];
   size_t count = 0;

   puts("This test, tests various pxml operations within libpndman.");
   puts("");

   if (pndman_init() == -1)
      return EXIT_FAILURE;

#ifdef __linux__
   /* copy path */
   strncpy(path1, PND_DIR, PATH_MAX-1);
   strncat(path1, "/",     PATH_MAX-1);

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

         ++count;
      }
   }

   closedir(dp);
#elif __WIN32__
    sprintf(path1, "%s/*.pnd", PND_DIR);

    if ((hFind = FindFirstFile(path1, &dp)) == INVALID_HANDLE_VALUE)
       return EXIT_FAILURE;

    do
    {
       sprintf(path2, "%s/%s", PND_DIR, dp.cFileName);
       puts(path2);
       pnd_do_something(path2);

       ++count;
    } while (FindNextFile(hFind, &dp));
    FindClose(hFind);
#endif

   if (pndman_quit() == -1)
      return EXIT_FAILURE;

   puts("");
   printf("%zu PNDs\n", count);

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
