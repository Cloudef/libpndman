#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "pndman.h"

/* \brief curl_multi handle for curl.c */
static CURLM *_pndman_curlm         = NULL;

/* \brief pndman_handle struct */
typedef struct pndman_handle
{
   char           name[25];
   char           error[LINE_MAX];
   char           url[LINE_MAX];
   unsigned int   flags;

   /* info */
   int            done;
   CURL           *curl;
} pndman_handle;

/* \brief Init curl's multi interface
 * NOTE: Does checkup if it's already initialized*/
static int _pndman_curl_init(void)
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
static int _pndman_curl_free(void)
{
   DEBUG("pndman_curl_free");

   if (!_pndman_curlm)
      return RETURN_FAIL;

   /* free multi curl handle */
   curl_multi_cleanup(_pndman_curlm);
   _pndman_curlm = NULL;

   return RETURN_OK;
}

/* \brief Allocate new pndman_handle for transfer */
int pndman_handle_init(char *name, pndman_handle *handle)
{
   DEBUG("pndman_handle_init");

   if (!handle)
      return RETURN_FAIL;

   if (_pndman_curl_init() != RETURN_OK)
      return RETURN_FAIL;

   handle->curl = NULL;
   strncpy(handle->name, name, 24);
   memset(handle->error, 0, LINE_MAX-1);
   handle->flags = 0;
   handle->done  = 0;

   return RETURN_OK;
}

/* \brief Free pndman_handle */
int pndman_handle_free(pndman_handle *handle)
{
   DEBUG("pndman_handle_free");

   if (!handle)
      return RETURN_FAIL;

   if (!handle->curl)
      return RETURN_FAIL;

   /* free curl handle */
   if (_pndman_curlm) curl_multi_remove_handle(_pndman_curlm, handle->curl);
   curl_easy_cleanup(handle->curl);
   handle->curl = NULL;

   return RETURN_OK;
}

/* \brief Perform pndman_handle */
int pndman_handle_perform(pndman_handle *handle)
{
   DEBUG("pndman_handle_perform");

   if (!handle)
      return RETURN_FAIL;

   /* get curl handle */
   handle->curl  = curl_easy_init();
   if (!handle->curl)
      return RETURN_FAIL;

   /* set download URL */
   curl_easy_setopt(handle->curl, CURLOPT_URL,     handle->url);
   curl_easy_setopt(handle->curl, CURLOPT_PRIVATE, handle);

   /* add to multi interface */
   curl_multi_add_handle(_pndman_curlm, handle->curl);

   return RETURN_OK;
}

/* \brief Perform download on all handles */
int pndman_download(int *still_running)
{
   int maxfd = -1;
   struct timeval timeout;
   fd_set fdread;
   fd_set fdwrite;
   fd_set fdexcep;
   long curl_timeo = -1;

   CURLMsg *msg;
   int msgs_left;
   pndman_handle *handle;

   /* perform download */
   curl_multi_perform(_pndman_curlm, still_running);

   /* zero file descriptions */
   FD_ZERO(&fdread);
   FD_ZERO(&fdwrite);
   FD_ZERO(&fdexcep);

   /* set a suitable timeout to play around with */
   timeout.tv_sec = 1;
   timeout.tv_usec = 0;

   /* timeout */
   curl_multi_timeout(_pndman_curlm, &curl_timeo);
   if(curl_timeo >= 0) {
      timeout.tv_sec = curl_timeo / 1000;
      if(timeout.tv_sec > 1) timeout.tv_sec = 1;
      else timeout.tv_usec = (curl_timeo % 1000) * 1000;
   }

   /* get file descriptors from the transfers */
   curl_multi_fdset(_pndman_curlm, &fdread, &fdwrite, &fdexcep, &maxfd);

   /* check that everything is still okay */
   if (select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout) == -1)
      return RETURN_FAIL;

   /* update status of curl handles */
   while ((msg = curl_multi_info_read(_pndman_curlm, &msgs_left)))
   {
      curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &handle);
      if (msg->msg == CURLMSG_DONE)
      {
         handle->done = 1;
      }
   }

   /* it's ok to get rid of this */
   if (!*still_running)
      _pndman_curl_free();

   return RETURN_OK;
}
