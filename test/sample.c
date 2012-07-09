/* sample.c,
 * this is not really a test,
 * it's actualy example of showing
 * how to use libpndman itself,
 * by showcasing all the functions.
 *
 * this sample asumes you are
 * running linux with /tmp folder,
 * however the code is simple so
 * you can use it to extend to other
 * platforms as well.
 *
 * incase you wonder why the text
 * is so short and hard to read.
 * it's for apocalypse reading
 * when you have only access to
 * so-so wide terminal and have
 * to use this crappy library.
 */

#include "pndman.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sample  debug hook, use this to catch error
 * messages etc */
static void sample_hook(const char *file, int line,
      const char *function, int verbose_level,
      const char *str)
{
   /* lets ignore all PNDMAN_LEVEL_CRAP
    * messages */
   if (verbose_level == PNDMAN_LEVEL_CRAP) return;
   printf("[%d] %s :: %s\n", verbose_level, function, str);
}

/* callback for our sync handles */
static void sync_done_cb(pndman_curl_code code,
      pndman_sync_handle *handle)
{
   /* this callback also gives
    * progression data, but we
    * don't cover that here.
    *
    * check the pndman_curl_progress
    * struct in pndman.h.
    *
    * this struct is member of
    * pndman_sync_handle. */

   /* check if this is either
    * DONE or FAIL message */
   if (code == PNDMAN_CURL_DONE ||
       code == PNDMAN_CURL_FAIL) {
      /* print the repository name
       * and either success or error message.
       *
       * NOTE: name is empty if repository isn't synced. */
      printf("%s : %s!\n", handle->repository->name,
            code == PNDMAN_CURL_DONE?"DONE":handle->error);

      /* free the sync handle now,
       * we are done with it. */
      pndman_sync_handle_free(handle);
   }
}

/* callback for our package handles */
static void pkg_done_cb(pndman_curl_code code,
      pndman_package_handle *handle)
{
   /* this callback also gives
    * progression data, but we
    * don't cover that here.
    *
    * check the pndman_curl_progress
    * struct in pndman.h.
    *
    * this struct is member of
    * pndman_package_handle. */

   /* check if this is either
    * DONE or FAIL message */
   if (code == PNDMAN_CURL_DONE ||
       code == PNDMAN_CURL_FAIL) {
      /* print the package handle name
       * and either success or error message. */
      printf("%s : %s!\n", handle->name,
            code == PNDMAN_CURL_DONE?"DONE":handle->error);
   }
}

/* callback for our download history api call */
static void dl_history_cb(pndman_curl_code code,
      pndman_api_history_packet *p)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      puts(p->error);
      return;
   }

   if (!strlen(p->id)) {
      printf("no history\n");
      return;
   }

   puts("");
   printf("-- Download History --\n");
   printf("%s [%lu] (%s.%s.%s.%s)\n", p->id, p->download_date,
         p->version->major,p->version->minor,
         p->version->release, p->version->build);
}

/* callback for our comment list api call */
static void comment_cb(pndman_curl_code code,
      pndman_api_comment_packet *p)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      puts(p->error);
      return;
   }

   if (!p->pnd) {
      printf("no comments\n");
      return;
   }

   puts("");
   printf("-- Comment List --\n");
   printf("%s [%lu] (%s.%s.%s.%s) // %s: %s\n",
         p->pnd->id, p->date,
         p->version->major, p->version->minor,
         p->version->release, p->version->build,
         p->username, p->comment);
}

/* callback for our archived packages api call */
static void archived_cb(pndman_curl_code code,
      pndman_api_archived_packet *p)
{
   pndman_package *pp;

   if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      puts(p->error);
      return;
   }

   puts("");
   printf("-- Archieved Packages --\n");
   for (pp = p->pnd->next_installed; pp; pp = pp->next_installed)
      printf("%s [%lu] (%s.%s.%s.%s)\n", pp->id, pp->modified_time,
            pp->version.major,   pp->version.minor,
            pp->version.release, pp->version.build);
   if (!p->pnd->next_installed)
      printf("no archieved packages\n");
}

/* callback for generic api requests */
static void generic_cb(pndman_curl_code code,
      const char *info, void *user_data) {
   if (code == PNDMAN_CURL_FAIL)
      puts(info);
}

/* main */
int main(int argc, char **argv)
{
   pndman_repository *repo, *r;
   pndman_device *device, *d;
   pndman_package *pnd, *p;
   int c = 0;

   printf("NOTE: This sample touches your /tmp "
          "you can remove the /tmp/pandora safely\n\n");

#if _WIN32
   printf("This sample will not work correctly on windows\n"
          "However, it should not crash either.\n");
#endif

   /* sets verbose level of pndman,
    * this is unrelevant if you use own
    * debug hook, since the verbose level
    * of debug message is given you as
    * argument */
   /* pndman_set_verbose(3); */

   /* we can set custom debug hook callback,
    * so we can trap all libpndman warnings
    * and failures and handle them ourself. */
   pndman_set_debug_hook(sample_hook);

   /* init repository list. this creates
    * the local repository, which is
    * a special repository, that we will
    * use to hold all locally installed
    * packages from different devices
    * and such. */
   repo = pndman_repository_init();

   /* from the _init(); command we got
    * a repository list where we can
    * add more repositories by calling
    * _add(); function.
    *
    * this will return pointer to
    * the added repository or NULL
    * on failure.
    *
    * we will play dirty and
    * ignore the return values here. */
   pndman_repository_add(
         "http://repo.openpandora.org/client/masterlist", repo);

   /* create sync handle for each repository.
    * note first repository is always local.
    * we can just ignore that, hence r = repo->next */
   for (r = repo->next; r; r = r->next) ++c;
   pndman_sync_handle shandle[c];
   for (c = 0, r = repo->next; r; r = r->next) {
      /* this basically memsets the
       * pndman_sync_handle to 0 */
      pndman_sync_handle_init(&shandle[c]);

      /* set properties of sync handle
       * by default libpndman does delta
       * synchorizations and only gets changes
       * from repository. Lets force full sync. */
      shandle[c].flags = PNDMAN_SYNC_FULL;

      /* repository we want to synchorize */
      shandle[c].repository = r;

      /* call back that gets called,
       * for progression and failure */
      shandle[c].callback = sync_done_cb;

      /* this triggers internal curl system,
       * when we call pndman_curl_process();
       * libpndman will start downloading
       * data over network.
       *
       * to cancel handle, you can simply
       * call pndman_sync_handle_free(),
       * which also frees the handle.
       *
       * The internal curl transfer is
       * canceled and freen within
       * pndman_curl_process loop,
       * when libpndman allows it. */
      pndman_sync_handle_perform(&shandle[c++]);
   }

   /* tell libpndman to process all our
    * curl requests (handle_perform)
    *
    * this should be ran until it returns
    * either 0 downloads or failure (-1)
    *
    * It's save to run in loop or
    * beetwen certain intervals for
    * example in GUI programs,
    * since it won't do anything if
    * not needed. */
   while (pndman_curl_process() > 0);

   /* lets make sure every sync handle is freed,
    * normally this check is not needed,
    * if everything was done proberly */
   for (c = 0, r = repo->next; r; r = r->next)
      pndman_sync_handle_free(&shandle[c++]);

   /* so, we have synced our repository
    * with the server.
    *
    * what to do now? maybe check, if
    * we have new updates for any
    * packages. below function,
    * takes the whole repository list
    * as argument since, it needs to be
    * able to check against local repository. */
   pndman_repository_check_updates(repo);

   /* if you want the repository
    * client api functions to work,
    * you need to set your credentials
    * here.
    *
    * you can obtain api key for eg.
    * milkshake's repository from
    * the repository site's account
    * settings page.
    *
    * NOTE: api functions need
    * synchorized repository, since
    * it gets the api root from the
    * repository metadata. */
   pndman_repository_set_credentials(
         repo->next, "username", "key", 0);

   /* this is AFAIK, the only repository
    * api command you can call without
    * pndman_package passed to it.
    *
    * so lets do it now.
    * this gets user download history from
    * the repository. the history data
    * will be given you through specified
    * callback function. */
   pndman_api_download_history(NULL, repo->next,
         dl_history_cb);


   /* don't forget to call the magic
    * pndman_curl_process(); for api
    * requests as well. */
   while (pndman_curl_process() > 0);

   /* OK, we covered all the neat
    * repository related things you
    * can do above.
    *
    * next some device functions. */

   /* with devices, we don't need to call
    * any pndman_device_init(); function.
    *
    * instead if you pass NULL to
    * pndman_device_add's second argument,
    * it will create new empty device
    * list for you. */
   device = pndman_device_add("/tmp", NULL);

   /* if you want to add another device
    * to the device list, you would
    * simply pass the list as second
    * argument.
    *
    * the return value for add function
    * is same as in repository. it
    * returns the added device or
    * NULL in failure. */
   pndman_device_add("/tmp", device);

   /* ^ the above function fails,
    * however since you can't add
    * duplicate devices. Same is
    * true for repositories as well. */

   /* if you want to detect devices
    * and mount points, you would
    * call function pndman_device_detect()
    * the argument holds same meaning here
    * as second argument in _add(); function.
    *
    * however the function returns pointer
    * to the first detected device. */
   pndman_device_detect(device);

   /* so, lets print all the devices
    * we have so far */
   puts("");
   printf("-- Device List --\n");
   for (d = device; d; d = d->next)
      printf("dev: %s mount: %s\n",
            d->device, d->mount);
   puts("");

   /* to free single device from list,
    * use the pndman_device_free function.
    * for example, lets free the first
    * device in the list.
    *
    * the function will return first
    * item in the list or NULL if,
    * list is empty after freeing.
    *
    * repositories have a similar
    * function. */
   device = pndman_device_free(device);

   /* ^ I assigned the return value,
    * to the device pointer since,
    * we are freeing the first item. */

   /* now lets see how our device list
    * changed. */
   printf("-- Device List --\n");
   for (d = device; d; d = d->next)
      printf("dev: %s mount: %s\n",
            d->device, d->mount);
   puts("");

   /* lets free every device and
    * add back the /tmp device
    * as we do our things to it. */
   if (device) {
      pndman_device_free_all(device);
      device = NULL;
   }
   device = pndman_device_add("/tmp", device);

   /* so that's for all the  device
    * functions.
    *
    * let's see what we can do
    * if we mix them with repositories. */

   /* if you want to read all the stored
    * database information from device
    * for certain repository, you would
    * use pndman_device_read_repository
    * function as follows. */
   pndman_device_read_repository(
         repo, device);

   /* above, we read the local repository
    * from the first device in the list.
    * you can do the same thing for
    * cached remote repository data,
    * so you can still "search" remote
    * repositories without internet
    * connection.
    *
    * the above function fails if there
    * is no data for given repository
    * on the device.
    *
    * anyways, lets do the same
    * for the remote repository as well. */
   pndman_device_read_repository(
         repo->next, device);

   /* so what can we done with the
    * readed repositories?
    * well for example we can
    * check if every package is
    * still as we expect them to
    * be on the physical disk
    * for local repository. */
   pndman_repository_check_local(
         repo);

   /* the above function checks
    * if any of the packages
    * do not exist on filesystem
    * anymore and frees them
    * from ram, if that's so.
    *
    * they are removed from
    * database as well,
    * next time we commit. */

   /* ok, any other things to do?
    * lets try crawling the device
    * for any packages user has
    * and we don't know about.
    *
    * this function returns
    * number of packages crawled and
    * adds them to local repository.
    *
    * the first argument is boolean
    * for full crawl. full crawl means
    * crawling everything from the package
    * which includes application data.
    *
    * if you set full_crawl to 0,
    * application data will not be read,
    * and thus saving some ram. */
   pndman_package_crawl(0,
         device, repo);

   /* OK, now how do we access the
    * packages in repositories?
    *
    * like this: */
   printf("-- Remote Packages [showing only 5 first] --\n");
   for (p = repo->next->pnd, c = 0;
        p; p = p->next) {
      printf("%s\n", p->id);
      if (++c>=5) break;
   }

   /* want application data for single
    * pnd? */
   if (repo->pnd)
      pndman_package_crawl_single_package(
            1, repo->pnd);

   /* get md5 for every package in
    * local repository that doesn't
    * have one. */
   for (p = repo->pnd; p; p = p->next)
      pndman_package_fill_md5(p);

   pnd = repo->next->pnd;
   /* so how about repository api
    * with certain package?
    *
    * like this, let's get comments
    * for package. */
   if (pnd)
      pndman_api_comment_pnd_pull(
            NULL, pnd, comment_cb);

   /* get archeived packages from
    * repository for package. */
   if (pnd)
      pndman_api_archived_pnd(
            NULL, pnd, archived_cb);

   /* you can also send comment
    * or rate pnd with
    *
    * pndman_api_comment_pnd();
    * pndman_api_rate_pnd();
    *
    * functions, but they are not
    * covered here. */

   /* don't forget to process the
    * api requests with the magic
    * pndman_curl_process(); */
   while (pndman_curl_process() > 0);

   /* want to download a package?
    * here we go. */
   if (pnd) {
      pndman_package_handle phandle;
      pndman_package_handle_init(
         pnd->id, &phandle);

      /* device where the package
       * will be installed/removed */
      phandle.device = device;

      /* package we want to
       * install/update/remove */
      phandle.pnd = pnd;

      /* flags for handle.
       * we only cover installing here,
       * refer to pndman.h for more. */
      phandle.flags = PNDMAN_PACKAGE_INSTALL |
                      PNDMAN_PACKAGE_INSTALL_APPS;

      /* callback that gets called
       * on progression and failure */
      phandle.callback = pkg_done_cb;

      /* perform our request */
      pndman_package_handle_perform(&phandle);

      /* call the magic function,
       * you know the thing already. */
      while (pndman_curl_process() > 0);

      /* normally this is something
       * you do in the package callback,
       * but since this is sample we do
       * this here.
       *
       * this is how you commit the
       * changes of package handle to
       * local repository.
       *
       * NOTE: database on disk stay intact.
       */
      if (phandle.progress.done)
         pndman_package_handle_commit(
               &phandle, repo);

      /* we are done with the handle,
       * free it now. */
      pndman_package_handle_free(&phandle);

      /* want the changes to disk?
       * OK. */
      pndman_repository_commit_all(
            repo, device);
   }

   /* below are for cleanups */

   /* free every repository
    * in the repository list */
   pndman_repository_free_all(repo);

   /* free every device
    * in the device list */
   pndman_device_free_all(device);

   /* we can exit now */
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
