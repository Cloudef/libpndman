#include "internal.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* internal multi curl handle */
static CURLM *_pndman_curlm = NULL;

/* \brief init internal curl header */
static void _pndman_curl_header_init(pndman_curl_header *header)
{
   memset(header, 0, sizeof(pndman_curl_header));
   if (!(header->data = malloc(PNDMAN_CURL_CHUNK)))
         return;
   header->size = PNDMAN_CURL_CHUNK;
}

/* \brief free internal curl header */
static void _pndman_curl_header_free(pndman_curl_header *header)
{
   IFDO(free, header->data);
   memset(header, 0, sizeof(pndman_curl_header));
}

/* \brief initialize the progress struct */
static void _pndman_curl_init_progress(pndman_curl_progress *progress)
{
   memset(progress, 0, sizeof(pndman_curl_progress));
}

/* \brief write to file */
static size_t _pndman_curl_write_file(void *data, size_t size, size_t nmemb, FILE *file)
{
   assert(file);
   size_t written = fwrite(data, size, nmemb, file);
   return written;
}

/* \brief write to header */
static size_t _pndman_curl_write_header(void *data, size_t size, size_t nmemb, pndman_curl_header *header)
{
   void *new;
   assert(header);

   /* init header if needed */
   if (!header->size)
      _pndman_curl_header_init(header);

   /* if we have more data than we can hold */
   if (header->pos + size * nmemb >= header->size - 1) {
      if (!(new = realloc(header->data,
                  header->size + PNDMAN_CURL_CHUNK))) {
         if (!(new = malloc(header->size + PNDMAN_CURL_CHUNK)))
               return 0;
         memcpy(new, header->data, header->size);
      }
      header->data  = new;
      header->size += PNDMAN_CURL_CHUNK;
   }

   /* memcpy new data */
   memcpy(header->data + header->pos, data, size * nmemb);
   header->pos += size * nmemb;
   return size * nmemb;
}

/* \brief progressbar */
static int _pndman_curl_progress_func(pndman_curl_handle *handle,
      double total_to_download, double download, double total_to_upload, double upload)
{
   handle->progress->download           = download;
   handle->progress->total_to_download  = total_to_download;
   handle->callback(PNDMAN_CURL_PROGRESS, handle->data, NULL);
   return 0;
}

/* INTERNAL API */

/* \brief free curl handle */
void _pndman_curl_handle_free(pndman_curl_handle *handle)
{
   assert(handle);

   /* cleanup */
   if (handle->curl) {
      if (_pndman_curlm) curl_multi_remove_handle(_pndman_curlm, handle->curl);
      curl_easy_cleanup(handle->curl);
   }
   _pndman_curl_header_free(&handle->header);
   IFDO(fclose, handle->file);
   if (strlen(handle->path)) unlink(handle->path);
   memset(handle, 0, sizeof(pndman_curl_handle));

   /* free handle */
   free(handle);
}

/* \brief create new curl handle */
pndman_curl_handle* _pndman_curl_handle_new(void *data,
      pndman_curl_progress *progress, pndman_curl_callback callback,
      const char *path)
{
   pndman_curl_handle *handle;
   assert(data && progress && callback);

   if (!(handle = malloc(sizeof(pndman_curl_handle))))
      goto handle_fail;
   memset(handle, 0, sizeof(pndman_curl_handle));
   memset(&handle->header, 0, sizeof(pndman_curl_header));

   if (!path) {
      if (!(handle->file = _pndman_get_tmp_file()))
         goto fail;
   } else {
      if (!(handle->file = fopen(path, "w+b")))
         goto open_fail;
      memcpy(handle->path, path, PNDMAN_PATH);
   }

   if (!(handle->curl = curl_easy_init()))
      goto curl_fail;

   /* set defaults */
   handle->data      = data;
   handle->callback  = callback;
   handle->progress  = progress;
   curl_easy_setopt(handle->curl, CURLOPT_WRITEFUNCTION, _pndman_curl_write_file);
   curl_easy_setopt(handle->curl, CURLOPT_HEADERFUNCTION, _pndman_curl_write_header);
   curl_easy_setopt(handle->curl, CURLOPT_CONNECTTIMEOUT, PNDMAN_CURL_TIMEOUT);
   curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, 0);
   curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, _pndman_curl_progress_func);
   curl_easy_setopt(handle->curl, CURLOPT_WRITEDATA, handle->file);
   curl_easy_setopt(handle->curl, CURLOPT_HEADERDATA, &handle->header);
   curl_easy_setopt(handle->curl, CURLOPT_PROGRESSDATA, handle);
   curl_easy_setopt(handle->curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(handle->curl, CURLOPT_PRIVATE, handle);

   return handle;

open_fail:
   DEBFAIL(ACCESS_FAIL, path);
   goto fail;
handle_fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_curl_handle");
   goto fail;
curl_fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "CURL");
fail:
   IFDO(fclose, handle->file);
   IFDO(unlink, path);
   IFDO(_pndman_curl_handle_free, handle);
   return NULL;
}

/* \brief perform curl operation */
int _pndman_curl_handle_perform(pndman_curl_handle *handle)
{
   if (!_pndman_curlm) {
#if 0
      // if (curl_global_init(CURL_GLOBAL_ALL) != 0)
      //   goto fail;
#endif
      if (!(_pndman_curlm = curl_multi_init()))
         goto fail;
   }

   _pndman_curl_init_progress(handle->progress);
   curl_multi_add_handle(_pndman_curlm, handle->curl);
   return RETURN_OK;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "CURLM");
   return RETURN_FAIL;
}

/* \brief query cleanup */
static void _pndman_curl_cleanup(void)
{
   if (_pndman_curlm) {
      curl_multi_cleanup(_pndman_curlm);
      curl_global_cleanup();
   }
   _pndman_curlm = NULL;
}

/* \brief perform curl operation */
static int _pndman_curl_perform(void)
{
   int still_running;
   int maxfd = -1;
   struct timeval timeout;
   fd_set fdread;
   fd_set fdwrite;
   fd_set fdexcep;
   long curl_timeout = -1;

   CURLMsg *msg;
   int msgs_left;

   pndman_curl_handle *handle;

   /* perform download */
   curl_multi_perform(_pndman_curlm, &still_running);

   /* zero file descriptions */
   FD_ZERO(&fdread);
   FD_ZERO(&fdwrite);
   FD_ZERO(&fdexcep);

   /* set a suitable timeout to play around with */
   timeout.tv_sec = 1;
   timeout.tv_usec = 0;

   /* timeout */
   curl_multi_timeout(_pndman_curlm, &curl_timeout);
   if(curl_timeout >= 0) {
      timeout.tv_sec = curl_timeout / 1000;
      if(timeout.tv_sec > 1) timeout.tv_sec = 1;
      else timeout.tv_usec = (curl_timeout % 1000) * 1000;
   }

   /* get file descriptors from the transfers */
   curl_multi_fdset(_pndman_curlm, &fdread, &fdwrite, &fdexcep, &maxfd);
   if (maxfd < -1) return RETURN_FAIL;
   select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

   /* update status of curl handles */
   while ((msg = curl_multi_info_read(_pndman_curlm, &msgs_left))) {
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &handle);
      if (msg->msg == CURLMSG_DONE) { /* DONE */
         handle->progress->done = msg->data.result==CURLE_OK?1:0;
         if (msg->data.result != CURLE_OK)
            handle->callback(PNDMAN_CURL_FAIL, handle->data,
                  curl_easy_strerror(msg->data.result));
         else {
            fflush(handle->file);
            handle->callback(PNDMAN_CURL_DONE, handle->data, NULL);
         }
      }
   }

   return still_running;
}

/* PUBLIC API */

/* \brief process all internal curl requests */
PNDMANAPI int pndman_curl_process(void)
{
   int still_running;

   /* we are done :) */
   if (!_pndman_curlm) return 0;

   /* perform sync */
   if ((still_running = _pndman_curl_perform()) == -1)
      goto fail;

   /* destoroy curlm when done,
    * and return exit code */
   if (!still_running) {
      _pndman_curl_cleanup();

      /* fake so that we are still running, why?
       * this lets user to catch the final completed handles
       *
       * However, in recent version callbacks
       * are the better method, but this doesn't hurt anything */
      still_running = 1;
   }

   return still_running;

fail:
   _pndman_curl_cleanup();
   return RETURN_FAIL;
}

/* vim: set ts=8 sw=3 tw=0 :*/
