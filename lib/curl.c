#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "pndman.h"
#include "curl.h"

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
   assert(request);
   if (request->result.pos + size * nmemb >= MAX_REQUEST - 1)
      return 0;

   memcpy(request->result.data + request->result.pos, data, size * nmemb);
   request->result.pos += size * nmemb;

   return size * nmemb;
}

/* \brief init internal curl request
 * NOTE: you must manually set curl pointer to NULL */
int curl_init_request(curl_request *request)
{
   assert(request);
   if (request->curl)   curl_easy_reset(request->curl);
   else request->curl = curl_easy_init();
   if (!request->curl) return RETURN_FAIL;

   request->result.data = malloc(MAX_REQUEST);
   if (!request->result.data) {
      curl_free_request(request);
      return RETURN_FAIL;
   }
   memset(request->result.data, 0, MAX_REQUEST);
   request->result.pos = 0;

   return RETURN_OK;
}

/* \brief free internal curl request */
void curl_free_request(curl_request *request)
{
   assert(request);
   if (request->curl)         curl_easy_cleanup(request->curl);
   if (request->result.data)  free(request->result.data);
   request->curl        = NULL;
   request->result.data = NULL;
   request->result.pos  = 0;
}

/* \brief initialize the progress struct */
void curl_init_progress(curl_progress *progress)
{
   assert(progress);
   progress->download = 0;
   progress->total_to_download = 0;
   progress->done = 0;
}

/* vim: set ts=8 sw=3 tw=0 :*/
