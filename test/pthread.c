#include "pndman.h"
#include "common.h"
#include <pthread.h>

/* pthread example,
 * NOTE: libpndman itself isn't thread safe.
 * However it's possible to run pndman_curl_process();
 * function in different thread for example,
 * as long as you guard the process and handle calls.
 *
 * Following calls will give race condition without
 * blocking:
 * pndman_curl_process();
 * pndman_*_handle_free();
 * pndman_*_handle_perform();
 *
 * Any other calls should be ok.
 * Generally you should not thread
 * any other call than pndman_curl_process();
 */

/* quit mutex */
static int PROCESS_THREAD_RUNNING = 1;
static int DOWNLOADS              = 0;
static pthread_mutex_t quit_mutex =
PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pndman_mutex =
PTHREAD_MUTEX_INITIALIZER;

static void* process_thread(void *ptr) {
   int running = 1;
   while (running) {
      pthread_mutex_lock(&pndman_mutex);
      DOWNLOADS = pndman_curl_process();
      pthread_mutex_unlock(&pndman_mutex);
      pthread_mutex_lock(&quit_mutex);
      running = PROCESS_THREAD_RUNNING;
      pthread_mutex_unlock(&quit_mutex);
      usleep(1000);
   }
   return NULL;
}

static void package_cb(pndman_curl_code code, pndman_package_handle *handle)
{
   if (code == PNDMAN_CURL_DONE ||
       code == PNDMAN_CURL_FAIL) {
      printf("%s : %s!\n", handle->pnd->id,
            code==PNDMAN_CURL_DONE?"DONE":handle->error);
      if (code == PNDMAN_CURL_DONE) {
         if (pndman_package_handle_commit(handle, handle->repository) != 0)
            printf("commit failed for: %s\n", handle->pnd->id);
      }
      pndman_package_handle_free(handle);
   } else if (code == PNDMAN_CURL_PROGRESS) {
      printf("%s [%.2f/%.2f]%s", handle->pnd->id,
            handle->progress.download/1048576, handle->progress.total_to_download/1048576,
            handle->progress.done?"\n":"\r"); fflush(stdout);
   }
}

#define HANDLE_COUNT    4
int main(int argc, char **argv)
{
   pthread_t pndman_thread;
   pndman_device *device;
   pndman_package *pnd;
   pndman_repository *repository, *repo;
   pndman_package_handle phandle[HANDLE_COUNT+1];
   pndman_sync_handle shandle[1];
   size_t i;
   int count;
   char *cwd;

   puts("-!- TEST pthread");
   puts("");

   pthread_create(&pndman_thread, NULL, (void*)&process_thread, NULL);

   pndman_set_verbose(PNDMAN_LEVEL_CRAP);
   cwd = common_get_path_to_fake_device();
   if (!(device = pndman_device_add(cwd, NULL)))
      err("failed to add device, check that it exists");

   repository = pndman_repository_init();
   if (!(repo = pndman_repository_add(REPOSITORY_URL, repository)))
      err("failed to add repository "REPOSITORY_URL", :/");

   pthread_mutex_lock(&pndman_mutex);
   common_create_sync_handles(shandle, 1, repository, common_sync_cb,
         PNDMAN_SYNC_FULL);
   pthread_mutex_unlock(&pndman_mutex);

   count = 0;

   /* wait for download to register */
   while (!count) {
      pthread_mutex_lock(&pndman_mutex);
      count = DOWNLOADS;
      pthread_mutex_unlock(&pndman_mutex);
   }

   /* wait until sync done */
   while (count) {
      pthread_mutex_lock(&pndman_mutex);
      count = DOWNLOADS;
      pthread_mutex_unlock(&pndman_mutex);
   }

   /* check that we actually got pnd's */
   if (!(pnd = repo->pnd))
      err("no PND's retivied from "REPOSITORY_URL", maybe it's down?");

   pthread_mutex_lock(&pndman_mutex);
   common_create_package_handles(phandle, HANDLE_COUNT, device, repo, package_cb);
   for (i = 0; i != HANDLE_COUNT; ++i) { phandle[i].pnd = pnd; if (!(pnd = pnd->next)) break; }
   common_perform_package_handles(phandle, HANDLE_COUNT,
         PNDMAN_PACKAGE_INSTALL | PNDMAN_PACKAGE_INSTALL_MENU);
   pthread_mutex_unlock(&pndman_mutex);

   count = 0;

   /* wait for download to register */
   while (!count) {
      pthread_mutex_lock(&pndman_mutex);
      count = DOWNLOADS;
      pthread_mutex_unlock(&pndman_mutex);
   }

   /* run endlesly */
   while (count) {
      pthread_mutex_lock(&pndman_mutex);
      count = DOWNLOADS;
      pthread_mutex_unlock(&pndman_mutex);

      /* cancel outside of thread */
      if (count > 2) {
         pthread_mutex_lock(&pndman_mutex);
         pndman_package_handle_free(&phandle[2]);
         pthread_mutex_unlock(&pndman_mutex);
      }
   }

   common_commit_repositories_to_device(repository, device);
   pndman_repository_free_all(repository);
   pndman_device_free_all(device);
   free(cwd);

   pthread_mutex_lock(&quit_mutex);
   PROCESS_THREAD_RUNNING = 0;
   pthread_mutex_unlock(&quit_mutex);
   pthread_join(pndman_thread, NULL);

   puts("");
   puts("-!- DONE");
   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
