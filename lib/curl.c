#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "pndman.h"
#include "curl.h"

/* INTERNAL API */

/* \brief write to file */
size_t curl_write_file(void *data, size_t size, size_t nmemb, FILE *file)
{
   size_t written = fwrite(data, size, nmemb, file);
   return written;
}

/* \brief progressbar */
int curl_progress_func(void* data, double TotalToDownload, double NowDownloaded, double TotalToUpload, double NowUploaded)
{
   return 0;
}

/* \brief write request */
size_t curl_write_request(void *data, size_t size, size_t nmemb, curl_request *request)
{
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
   if (request->curl)   curl_easy_reset(request->curl);
   else request->curl = curl_easy_init();
   if (!request->curl) return RETURN_FAIL;

   request->result.data = malloc(MAX_REQUEST);
   if (!request->result.data) {
      curl_free_request(request);
      return RETURN_FAIL;
   }
   return RETURN_OK;
}

/* \brief free internal curl request */
void curl_free_request(curl_request *request)
{
   if (request->curl)         curl_easy_cleanup(request->curl);
   if (request->result.data)  free(request->result.data);
}

/* vim: set ts=8 sw=3 tw=0 :*/
