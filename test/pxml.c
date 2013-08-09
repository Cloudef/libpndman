#include "pndman.h"
#include "common.h"

int main(int argc, char **argv)
{
#ifndef _WIN32
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

   puts("-!- TEST pxml");
   puts("");

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);
   cwd = common_get_path_to_fake_device();
#ifndef _WIN32
   /* copy path */
   strncpy(path1, cwd, PATH_MAX-1);
   strncat(path1, "/pandora/menu/", PATH_MAX-1);

   dp = opendir(path1);
   if (!dp)
      return EXIT_FAILURE;

   while ((ep = readdir (dp))) {
      if (strstr(ep->d_name, ".pnd")) {
         strncpy(path2, path1,      PATH_MAX-1);
         strncat(path2, ep->d_name, PATH_MAX-1);
         puts(path2);
         pndman_pxml_test(path2);
         ++count;
      }
   }

   closedir(dp);
#else
   sprintf(path1, "%s/pandora/menu/*.pnd", cwd);

   if ((hFind = FindFirstFile(path1, &dp)) == INVALID_HANDLE_VALUE)
      err("could not find any pnd files");

   do {
      sprintf(path2, "%s/pandora/menu/%s", cwd, dp.cFileName);
      puts(path2);
      pndman_pxml_test(path2);

      ++count;
   } while (FindNextFile(hFind, &dp));
   FindClose(hFind);
#endif

   free(cwd);
   puts("");
   printf("%zu PNDs\n", count);

   puts("");
   puts("-!- DONE");
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
