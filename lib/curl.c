#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "pndman.h"
#include "curl.h"

#define BUFFER_SIZE (1024 * 1024) /* 1024 KB */

/* \brief curl_multi handle for curl.c */
static CURLM *_pndman_curlm         = NULL;

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

/* \brief write response */
size_t curl_write_response(void *data, size_t size, size_t nmemb, curl_write_result *result)
{
   if (result->pos + size * nmemb >= BUFFER_SIZE - 1)
      return 0;

   memcpy(result->data + result->pos, data, size * nmemb);
   result->pos += size * nmemb;

   return size * nmemb;
}

/* \brief Init curl's multi interface
 * NOTE: Does checkup if it's already initialized */
int _pndman_curl_init(void)
{
   DEBUG("pndman_curl_free");

   if (_pndman_curlm)
      return RETURN_OK;

   /* get multi curl handle */
   _pndman_curlm = curl_multi_init();
   if (!_pndman_curlm)
      return RETURN_FAIL;

   return RETURN_OK;
}

/* \brief Free curl's multi interface */
int _pndman_curl_free(void)
{
   DEBUG("pndman_curl_free");

   if (!_pndman_curlm)
      return RETURN_FAIL;

   /* free multi curl handle */
   curl_multi_cleanup(_pndman_curlm);
   _pndman_curlm = NULL;

   return RETURN_OK;
}

/* \brief Add easy handle to multi handle */
int _pndman_curl_add_handle(CURL *curl)
{
   if (!_pndman_curlm)
      return RETURN_FAIL;

   curl_multi_add_handle(_pndman_curlm, curl);
   return RETURN_OK;
}

/* \brief Remove easy handle from multi handle */
int _pndman_curl_remove_handle(CURL *curl)
{
   if (!_pndman_curlm)
      return RETURN_FAIL;

   curl_multi_remove_handle(_pndman_curlm, curl);
   return RETURN_OK;
}

/* \brief Internal perform of curl's multi handle */
int _pndman_curl_perform(int *still_running)
{
   if (!_pndman_curlm)
      return RETURN_FAIL;

   curl_multi_perform(_pndman_curlm, still_running);
   return RETURN_OK;
}

/* \brief Internal timeout of curl's multi handle */
int _pndman_curl_timeout(long *timeout)
{
   if (!_pndman_curlm)
      return RETURN_FAIL;

   curl_multi_timeout(_pndman_curlm, timeout);
   return RETURN_OK;
}

/* \brief Internal fdset of curl's multi handle */
int _pndman_curl_fdset(fd_set *read, fd_set *write, fd_set *expect, int *max)
{
   if (!_pndman_curlm)
      return RETURN_FAIL;

   curl_multi_fdset(_pndman_curlm, read, write, expect, max);
   return RETURN_OK;
}

CURLMsg* _pndman_curl_info_read(int *msgs_left)
{
   if (!_pndman_curlm)
   {
      msgs_left = 0;
      return NULL;
   }

   return curl_multi_info_read(_pndman_curlm, msgs_left);
}

/* vim: set ts=8 sw=3 tw=0 :*/
