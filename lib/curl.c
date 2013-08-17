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
   if (!handle || handle->free) return 0;
   return fwrite(data, size, nmemb, handle->file);
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
   (void)total_to_upload;
   (void)upload;
   if (!handle || handle->free) return 1;
   handle->progress->download           = handle->resume + download;
   handle->progress->total_to_download  = handle->resume + total_to_download;
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

   /* unlink on free
    * commented since we allow download resumes! */
#if 0
   if (strlen(handle->path)) unlink(handle->path);
#endif
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
   handle->progress = NULL;
   handle->callback = NULL;

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
      /* this temporary file exists, lets do resume! */
      if ((handle->file = fopen(handle->path, "rb"))) {
         fseek(handle->file, 0L, SEEK_END);
         handle->resume = ftell(handle->file);
         fclose(handle->file);
      }
      if (!(handle->file = fopen(handle->path,
                  handle->resume?"a+b":"w+b")))
         goto open_fail;
   }

   /* print url */
   DEBUG(PNDMAN_LEVEL_CRAP, "url: %s", handle->url);

   /* set file options */
   curl_easy_setopt(handle->curl, CURLOPT_URL, handle->url);
   if (strlen(handle->post)) {
         curl_easy_setopt(handle->curl, CURLOPT_POSTFIELDS, handle->post);
      DEBUG(PNDMAN_LEVEL_CRAP, "POST: %s", handle->post);
   }

   curl_easy_setopt(handle->curl, CURLOPT_HEADERFUNCTION, _pndman_curl_write_header);
   curl_easy_setopt(handle->curl, CURLOPT_CONNECTTIMEOUT, pndman_get_curl_timeout());
   curl_easy_setopt(handle->curl, CURLOPT_NOPROGRESS, handle->progress?0:1);
   curl_easy_setopt(handle->curl, CURLOPT_PROGRESSFUNCTION, _pndman_curl_progress_func);
   curl_easy_setopt(handle->curl, CURLOPT_HEADERDATA, handle);
   curl_easy_setopt(handle->curl, CURLOPT_PROGRESSDATA, handle);
   curl_easy_setopt(handle->curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(handle->curl, CURLOPT_PRIVATE, handle);
   curl_easy_setopt(handle->curl, CURLOPT_WRITEFUNCTION, _pndman_curl_write_file);
   curl_easy_setopt(handle->curl, CURLOPT_WRITEDATA, handle);
   curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_LIMIT, 10240L);
   curl_easy_setopt(handle->curl, CURLOPT_LOW_SPEED_TIME, 300L);
   if (handle->resume && strlen(handle->path)) {
      curl_easy_setopt(handle->curl, CURLOPT_RESUME_FROM, handle->resume);
      DEBUG(PNDMAN_LEVEL_CRAP, "Handle resume: %zu", handle->resume);
   }

   if (!_pndman_curlm && !(_pndman_curlm = curl_multi_init()))
      goto curlm_fail;

   /* init progress if needed */
   if (handle->progress)
      _pndman_curl_init_progress(handle->progress);

   if (curl_multi_add_handle(_pndman_curlm, handle->curl) != CURLM_OK)
      goto add_fail;

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
   goto fail;
add_fail:
   DEBFAIL("curl_multi_add_handle failed");
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

/* \brief handle curl message */
static void _pndman_curl_msg(int result, pndman_curl_handle *handle)
{
   if (!handle || handle->free)
      return;

   if (handle->progress) handle->progress->done = 1;

   if (pndman_get_verbose() >= PNDMAN_LEVEL_CRAP) {
      if (handle->header.size)
         DEBUG(PNDMAN_LEVEL_CRAP, "%s", (char*)handle->header.data);

      char buffer[256];
      memset(buffer, 0, sizeof(buffer));
      fseek(handle->file, 0L, SEEK_SET);
      if (fgets(buffer, sizeof(buffer)-1, handle->file) && strlen(buffer) > 5)
         DEBUG(PNDMAN_LEVEL_CRAP, "%s", buffer);
      fseek(handle->file, 0L, SEEK_SET);
   }

   if (result != CURLE_OK) {
      /* is http 1.1 resume supported? */
      if (result == CURLE_RANGE_ERROR ||
          result == CURLE_BAD_DOWNLOAD_RESUME)
         handle->retry = PNDMAN_CURL_MAX_RETRY;

      /* try http 1.1 download resume */
      if ((result == CURLE_PARTIAL_FILE ||
           result == CURLE_WRITE_ERROR  ||
           result == CURLE_OPERATION_TIMEDOUT ||
           result == CURLE_ABORTED_BY_CALLBACK) &&
            handle->retry < PNDMAN_CURL_MAX_RETRY) {
         /* retry */
         DEBUG(PNDMAN_LEVEL_CRAP, "%s", curl_easy_strerror(result));
         if (_pndman_curl_handle_perform(handle) == RETURN_OK)
            handle->retry++;
         else handle->retry = PNDMAN_CURL_MAX_RETRY;
      } else handle->retry = PNDMAN_CURL_MAX_RETRY;

      /* fail if max retries exceeded */
      if (handle->retry >= PNDMAN_CURL_MAX_RETRY) {
         handle->resume = 0;
         handle->retry  = 0;
         handle->callback(PNDMAN_CURL_FAIL, handle->data,
               curl_easy_strerror(result), handle);
      }
   } else {
      handle->resume = 0;
      handle->retry  = 0;
      fflush(handle->file);
      handle->callback(PNDMAN_CURL_DONE, handle->data, NULL, handle);
   }
}

/* \brief perform curl operation */
static int _pndman_curl_perform(unsigned long tv_sec, unsigned long tv_usec)
{
   int still_running;
   int maxfd = -1;
   struct timeval timeout;
   fd_set fdread;
   fd_set fdwrite;
   fd_set fdexcep;
   long curl_timeout = -1;

   CURLMcode ret;
   CURLMsg *msg;
   int msgs_left;

   pndman_curl_handle *handle;

   /* perform download */
   while ((ret = curl_multi_perform(_pndman_curlm, &still_running)) == CURLM_CALL_MULTI_PERFORM);

   /* check error */
   if (ret != CURLM_OK)
      goto fail;

   /* run multi_timeout, if still running */
   if (still_running) {
      /* zero file descriptions */
      FD_ZERO(&fdread);
      FD_ZERO(&fdwrite);
      FD_ZERO(&fdexcep);

      /* set a suitable timeout to play around with */
      memset(&timeout, 0, sizeof(timeout));
      timeout.tv_sec = tv_sec;
      timeout.tv_usec = tv_usec;

      /* timeout */
      ret = curl_multi_timeout(_pndman_curlm, &curl_timeout);
      if (ret != CURLM_OK) goto fail;

      if (curl_timeout >= 0) {
         timeout.tv_sec  = curl_timeout / 1000;
         timeout.tv_usec = 0;
         if (timeout.tv_sec > 60) {
            timeout.tv_sec = 60;
         } else if (timeout.tv_sec == 0) {
            timeout.tv_usec = curl_timeout * 1000;
         }
      }

      /* get file descriptors from the transfers */
      ret = curl_multi_fdset(_pndman_curlm, &fdread, &fdwrite, &fdexcep, &maxfd);
      if (ret != CURLM_OK || maxfd < -1) goto fail;
      select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
   }

   /* update status of curl handles */
   while ((msg = curl_multi_info_read(_pndman_curlm, &msgs_left))) {
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &handle);
      if (msg->msg == CURLMSG_DONE) { /* DONE */
         _pndman_curl_msg(msg->data.result, handle);

         /* free handle if requested */
         if (handle && handle->free)
            _pndman_curl_handle_free_real(handle);

         /* if we got messages and still running == 0
          * make it back 1, since callbacks might restart
          * curl requests */
         if (!still_running) still_running = 1;
      }
   }

   return still_running;

fail:
   DEBFAIL("%s", ret);
   return -1;
}

/* PUBLIC API */

/* \brief process all internal curl requests */
PNDMANAPI int pndman_curl_process(unsigned long tv_sec, unsigned long tv_usec)
{
   int still_running;

   /* we are done :) */
   if (!_pndman_curlm) return 0;

   /* perform sync */
   if ((still_running = _pndman_curl_perform(tv_sec, tv_usec)) == -1)
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
