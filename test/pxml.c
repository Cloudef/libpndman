#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#  include <dirent.h>
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
#ifdef __linux__
   DIR *dp;
   struct dirent *ep;
#else
   WIN32_FIND_DATA dp;
   HANDLE hFind = NULL;
#endif
   char path1[PATH_MAX];
   char path2[PATH_MAX];
   char *cwd;
   size_t count = 0;

   cwd = test_device();
   if (!cwd) err("failed to get virtual device path");

   puts("This test, tests various pxml operations within libpndman.");
   puts("To get some pnds, run handle(.exe) first.");
   puts("");

   if (pndman_init() == -1)
      err("pndman init failed");

#ifdef __linux__
   /* copy path */
   strncpy(path1, cwd, PATH_MAX-1);
   strncat(path1, "/pandora/menu/", PATH_MAX-1);

   dp = opendir(path1);
   if (!dp)
      return EXIT_FAILURE;

   while (ep = readdir (dp)) {
      if (strstr(ep->d_name, ".pnd")) {
         strncpy(path2, path1,      PATH_MAX-1);
         strncat(path2, ep->d_name, PATH_MAX-1);
         puts(path2);
         pnd_do_something(path2);
         ++count;
      }
   }

   closedir(dp);
#elif __WIN32__
   sprintf(path1, "%s/pandora/menu/*.pnd", cwd);

   if ((hFind = FindFirstFile(path1, &dp)) == INVALID_HANDLE_VALUE)
      err("could not find any pnd files");

   do {
      sprintf(path2, "%s/pandora/menu/%s", cwd, dp.cFileName);
      puts(path2);
      pnd_do_something(path2);

      ++count;
   } while (FindNextFile(hFind, &dp));
   FindClose(hFind);
#endif

   free(cwd);
   if (pndman_quit() == -1)
      err("pndman quit failed");

   puts("");
   printf("%zu PNDs\n", count);

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
