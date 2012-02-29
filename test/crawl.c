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
   pndman_repository *repository, *r;
   pndman_device     *device;
   pndman_package    *pnd;
   pndman_translated *t;
   char *cwd;

   cwd = test_device();
   if (!cwd) err("failed to get virtual device path");

   puts("This test, tests crawl operation inside libpndman");
   puts("");

   if (pndman_init() != 0)
      err("pndman_init failed");

   /* add device */
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   /* add repository */
   repository = pndman_repository_init();
   if (!repository)
      err("allocating repo list failed");

   /* crawl pnds to local repository */
   if (pndman_crawl(0, device, repository) == -1)
      err("crawling failed");

   puts("");
   r = repository;
   for (; r; r = r->next)
   {
      printf("%s :\n", r->name);
      printf("   UPD: %s\n", r->updates);
      printf("   URL: %s\n", r->url);
      printf("   VER: %s\n", r->version);
      puts("");

      /* print only local repo packages if any,
       * run handle exe to get some :) */
      if (!r->prev) {
         for (pnd = r->pnd; pnd; pnd = pnd->next) {
            printf("ID:    %s\n", pnd->id);
            printf("ICON:  %s\n", pnd->icon);
            printf("MD5:   %s\n", pnd->md5);
            printf("URL:   %s\n", pnd->url);
            puts("\nTitles:");
            for (t = pnd->title; t; t = t->next)
               puts(t->string);
            puts("");
         }
      }
   }
   puts("");

   /* free everything */
   pndman_repository_free_all(repository);
   pndman_device_free_all(device);

   free(cwd);
   if (pndman_quit() == -1)
      err("pndman_quit failed");

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
