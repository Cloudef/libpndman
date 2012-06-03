#include "internal.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>

/* internal multi curl handle */
static CURLM *_pndman_curlm = NULL;

/* \brief init internal curl header */
static void _pndman_curl_header_init(pndman_curl_header *header)
{
   memset(header, 0, sizeof(pndman_curl_header));
   if (!(header->data = malloc(PNDMAN_CURL_CHUNK)))
         return;
   memset(header->data, 0, PNDMAN_CURL_CHUNK);
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
static size_t _pndman_curl_write_file(void *data, size_t size, size_t nmemb, pndman_curl_handle *handle)
{
   size_t written;
   if (!handle || handle->free) return 0;
   written= fwrite(data, size, nmemb, handle->file);
   if (written) handle->resume += written;
   return written;
}

/* \brief write to header */
static size_t _pndman_curl_write_header(void *data, size_t size, size_t nmemb, pndman_curl_handle *handle)
{
   void *new;
   pndman_curl_header *header;

   if (!handle || handle->free) return 0;
   header = &handle->header;

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
      memset(header->data+header->size, 0,
             header->size + PNDMAN_CURL_CHUNK);
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
   if (!handle || handle->free) return 1;
   handle->progress->download           = download;
   handle->progress->total_to_download  = total_to_download;
   handle->callback(PNDMAN_CURL_PROGRESS, handle->data, NULL, handle);
   return 0;
}

static void _pndman_curl_handle_free_real(pndman_curl_handle *handle)
{
   assert(handle);

   /* we were not requested to do this */
   if (!handle->free) return;

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

/* INTERNAL API */

/* \brief free curl handle */
void _pndman_curl_handle_free(pndman_curl_handle *handle)
{
   assert(handle);
   handle->free = 1;

   /* can we free immediatly? */
   if (!_pndman_curlm)
      _pndman_curl_handle_free_real(handle);
}

/* \brief create new curl handle */
pndman_curl_handle* _pndman_curl_handle_new(void *data,
      pndman_curl_progress *progress, pndman_curl_callback callback,
      const char *path)
{
   pndman_curl_handle *handle;

   if (!(handle = malloc(sizeof(pndman_curl_handle))))
      goto handle_fail;
   memset(handle, 0, sizeof(pndman_curl_handle));
   memset(&handle->header, 0, sizeof(pndman_curl_header));

   if (!(handle->curl = curl_easy_init()))
      goto curl_fail;

   /* set defaults */
   if (path) memcpy(handle->path, path, PNDMAN_PATH);
   handle->data      = data;
   handle->callback  = callback;
   handle->progress  = progress;
   return handle;

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

/* \brief set curl handle's url */
void _pndman_curl_handle_set_url(pndman_curl_handle *handle, const char *url)
{
   assert(handle);
   if (!url) return;
   memset(handle->url, 0, PNDMAN_URL-1);
   strncpy(handle->url, url, PNDMAN_URL-1);
}

/* \brief set curl handle's post field */
void _pndman_curl_handle_set_post(pndman_curl_handle *handle, const char *post)
{
   assert(handle);
   if (!post) return;
   memset(handle->post, 0, PNDMAN_POST-1);
   strncpy(handle->post, post, PNDMAN_POST-1);
}

/* \brief perform curl operation */
int _pndman_curl_handle_perform(pndman_curl_handle *handle)
{
   assert(handle);

   /* no callback or data, we will fail */
   if (!handle->data || !handle->callback)
      goto no_data_or_callback;

   /* no url, fail */
   if (!strlen(handle->url))
      goto no_url;

   /* reopen handle if needed */
   if (handle->file) {
      curl_multi_remove_handle(_pndman_curlm, handle->curl);
      curl_easy_reset(handle->curl);
      fclose(handle->file);
      handle->file = NULL;
      _pndman_curl_header_free(&handle->header);
      memset(&handle->header, 0, sizeof(pndman_curl_header));
      DEBUG(PNDMAN_LEVEL_CRAP, "CURL REOPEN");
   }

   if (!strlen(handle->path)) {
      if (!(handle->file = _pndman_get_tmp_file()))
         goto fail;
   } else {
      if (!(handle->file = fopen(handle->path,
                  handle->resume?"a+b":"w+b")))
         goto open_fail;
   }

   /* print url */
   DEBUG(PNDMAN_LEVEL_CRAP, handle->url);

   /* set file options */
   curl_easy_setopt(handle->curl, CURLOPT_URL, handle->url);
   if (strlen(handle->post)) {
         curl_easy_setopt(handle->curl, CURLOPT_POSTFIELDS, handle->post);
      DEBUG(PNDMAN_LEVEL_CRAP, "POST: %s", handle->post);
   }

   curl_easy_setopt(handle->curl, CURLOPT_HEADERFUNCTION, _pndman_curl_write_header);
   curl_easy_setopt(handle->curl, CURLOPT_CONNECTTIMEOUT, PNDMAN_CURL_TIMEOUT);
   curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, handle->progress?0:1);
   curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, _pndman_curl_progress_func);
   curl_easy_setopt(handle->curl, CURLOPT_HEADERDATA, handle);
   curl_easy_setopt(handle->curl, CURLOPT_PROGRESSDATA, handle);
   curl_easy_setopt(handle->curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(handle->curl, CURLOPT_PRIVATE, handle);
   curl_easy_setopt(handle->curl, CURLOPT_WRITEFUNCTION, _pndman_curl_write_file);
   curl_easy_setopt(handle->curl, CURLOPT_WRITEDATA, handle);
   curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_LIMIT, 10240L);
   curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_TIME, 10L);
   if (handle->resume && strlen(handle->path)) {
      curl_easy_setopt(handle->curl, CURLOPT_RESUME_FROM, handle->resume);
      DEBUG(PNDMAN_LEVEL_CRAP, "Handle resume: %zu", handle->resume);
   }

   if (!_pndman_curlm && !(_pndman_curlm = curl_multi_init()))
      goto curlm_fail;

   /* init progress if needed */
   if (handle->progress)
      _pndman_curl_init_progress(handle->progress);

   curl_multi_add_handle(_pndman_curlm, handle->curl);
   return RETURN_OK;

no_data_or_callback:
   DEBFAIL(CURL_NO_DATA_OR_CALLBACK);
   goto fail;
no_url:
   DEBFAIL(CURL_NO_URL);
   goto fail;
open_fail:
   DEBFAIL(ACCESS_FAIL, handle->path);
   goto fail;
curlm_fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "CURLM");
fail:
   IFDO(fclose, handle->file);
   if (strlen(handle->path)) unlink(handle->path);
   return RETURN_FAIL;
}

/* \brief query cleanup */
static void _pndman_curl_cleanup(void)
{
   if (!_pndman_curlm) return;
   curl_multi_cleanup(_pndman_curlm);
   curl_global_cleanup();
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
         if (handle->progress)
            handle->progress->done = msg->data.result==CURLE_OK?1:0;

         char buffer[1024];
         memset(buffer, 0, sizeof(buffer));
         if (handle->header.size)
            DEBUG(PNDMAN_LEVEL_CRAP, (char*)handle->header.data);
         fseek(handle->file, 0L, SEEK_SET);
         if (fgets(buffer, sizeof(buffer)-1, handle->file) &&
             strlen(buffer) > 5)
            DEBUG(PNDMAN_LEVEL_CRAP, buffer);
         fseek(handle->file, 0L, SEEK_SET);

         if (msg->data.result != CURLE_OK) {
            /* is http 1.1 resume supported? */
            if (msg->data.result == CURLE_RANGE_ERROR ||
                msg->data.result == CURLE_BAD_DOWNLOAD_RESUME)
               handle->retry = PNDMAN_CURL_MAX_RETRY;

            /* try http 1.1 download resume */
            if ((msg->data.result == CURLE_PARTIAL_FILE ||
                 msg->data.result == CURLE_WRITE_ERROR  ||
                 msg->data.result == CURLE_OPERATION_TIMEDOUT ||
                 msg->data.result == CURLE_ABORTED_BY_CALLBACK) &&
                  handle->retry < PNDMAN_CURL_MAX_RETRY) {
               /* retry */
               if (_pndman_curl_handle_perform(handle) == RETURN_OK)
                  handle->retry++;
               else handle->retry = PNDMAN_CURL_MAX_RETRY;
            } else handle->retry = PNDMAN_CURL_MAX_RETRY;

            /* fail if max retries exceeded */
            if (handle->retry >= PNDMAN_CURL_MAX_RETRY) {
               handle->resume = 0;
               handle->retry  = 0;
               handle->callback(PNDMAN_CURL_FAIL, handle->data,
                     curl_easy_strerror(msg->data.result), handle);
            }
         } else {
            handle->resume = 0;
            handle->retry  = 0;
            fflush(handle->file);
            handle->callback(PNDMAN_CURL_DONE, handle->data, NULL, handle);
         }

         /* free handle if requested */
         if (handle->free)
            _pndman_curl_handle_free_real(handle);

         /* if we got messages and still running == 0
          * make it back 1, since callbacks might restart
          * curl requests */
         if (!still_running) still_running = 1;
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
