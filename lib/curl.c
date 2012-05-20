#include "internal.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* INTERNAL API */

/* \brief write to file */
size_t curl_write_file(void *data, size_t size, size_t nmemb, FILE *file)
{
   assert(file);
   size_t written = fwrite(data, size, nmemb, file);
   return written;
}

/* \brief progressbar */
int curl_progress_func(curl_progress *ptr, double total_to_download, double download, double total_to_upload, double upload)
{
   ptr->download           = download;
   ptr->total_to_download  = total_to_download;
   return 0;
}

/* \brief write request */
size_t curl_write_request(void *data, size_t size, size_t nmemb, curl_request *request)
{
   void *new;
   assert(request);

   /* if we have more data than we can hold */
   if (request->result.pos + size * nmemb >= request->result.size - 1) {
      if (!(new = realloc(request->result.data,
                  request->result.size + CURL_CHUNK))) {
         if (!(new = malloc(request->result.size + CURL_CHUNK)))
            return 0;
         memcpy(new, request->result.data, request->result.size);
      }
      request->result.data  = new;
      request->result.size += CURL_CHUNK;
   }

   /* memcpy new data */
   memcpy(request->result.data + request->result.pos, data, size * nmemb);
   request->result.pos += size * nmemb;
   return size * nmemb;
}

/* \brief init internal curl request */
void curl_init_request(curl_request *request)
{
   assert(request);
   memset(request, 0, sizeof(curl_request));
   curl_reset_request(request);
}

/* \brief reset internal curl request */
int curl_reset_request(curl_request *request)
{
   assert(request);

   request->result.pos = 0;
   if (!request->result.data) {
      request->result.data = malloc(CURL_CHUNK);
      request->result.size = CURL_CHUNK;
   }
   if (!request->result.data) goto fail;
   memset(request->result.data, 0, request->result.size);

   if (request->curl)   curl_easy_reset(request->curl);
   else request->curl = curl_easy_init();
   if (!request->curl) goto fail;

   return RETURN_OK;

fail:
   curl_free_request(request);
   return RETURN_FAIL;
}

/* \brief free internal curl request */
void curl_free_request(curl_request *request)
{
   assert(request);
   if (request->result.data)  free(request->result.data);
   if (request->curl)         curl_easy_cleanup(request->curl);
   memset(request, 0, sizeof(curl_request));
}

/* \brief initialize the progress struct */
void curl_init_progress(curl_progress *progress)
{
   assert(progress);
   memset(progress, 0, sizeof(curl_progress));
}

/* vim: set ts=8 sw=3 tw=0 :*/
