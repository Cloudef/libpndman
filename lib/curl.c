#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "pndman.h"
#include "curl.h"

#define BUFFER_SIZE (1024 * 1024) /* 1024 KB */

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
   if (request->result.pos + size * nmemb >= BUFFER_SIZE - 1)
      return 0;

   memcpy(request->result.data + request->result.pos, data, size * nmemb);
   request->result.pos += size * nmemb;

   return size * nmemb;
}

/* vim: set ts=8 sw=3 tw=0 :*/
