#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#define API_HANDSHAKE   "handshake"
#define API_RATE        "rate"
#define API_COMMENT     "comment"
#define NONCE_LEN 33

/* forward declaration */
struct pndman_api_request;

/* \brief api callback function */
typedef void (*pndman_api_callback)(struct pndman_api_request *request);

/* \brief api request type enum */
typedef enum pndman_api_request_type
{
   PNDMAN_API_NONCE,
   PNDMAN_API_HANDSHAKE,
} pndman_api_request_type;

/* \brief handshake packet */
typedef struct pndman_handshake_packet
{
   char username[PNDMAN_SHRT_STR];
   char key[PNDMAN_STR];
} pndman_handshake_packet;

/* \brief comment packet */
typedef struct pndman_comment_packet
{
   char comment[PNDMAN_API_COMMENT_LEN];
   pndman_package *pnd;
   pndman_repository *repository;
   pndman_api_comment_callback callback;
} pndman_comment_packet;

/* \brief rate packet */
typedef struct pndman_rate_packet
{
   int rate;
   pndman_package *pnd;
   pndman_repository *repository;
} pndman_rate_packet;

/* \brief download packet */
typedef struct pndman_download_packet
{
   pndman_package_handle *handle;
} pndman_download_packet;

/* \brief internal api request */
typedef struct pndman_api_request
{
   pndman_api_request_type type;
   pndman_api_status status;
   pndman_curl_handle *handle;
   pndman_api_callback callback;
   pndman_handshake_packet *handshake;
   void *data;
} pndman_api_request;

/* \brief internal allocation of internal api request */
static pndman_api_request* _pndman_api_request_new(
      pndman_api_request_type type, pndman_curl_handle *handle)
{
   pndman_api_request *request;
   if (!(request = malloc(sizeof(pndman_api_request))))
      goto fail;
   memset(request, 0, sizeof(pndman_api_request));
   request->type = type;
   request->handle = handle;

   return request;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_api_request");
   return NULL;
  }

/* \brief internal free of internal api request */
static void _pndman_api_request_free(
      pndman_api_request *request)
{
   IFDO(free, request->handshake);
   IFDO(free, request->data);
   IFDO(_pndman_curl_handle_free, request->handle);
   free(request);
}

/* \brief common api callback */
static void _pndman_api_common_cb(pndman_curl_code code, void *data,
      const char *info, pndman_curl_handle *chandle)
{
   if (code == PNDMAN_CURL_FAIL)
      DEBFAIL("api request fail: %s", info);
   else if (code == PNDMAN_CURL_DONE)
      DEBUG(PNDMAN_LEVEL_CRAP, "%s was success!", (char*)data);

   /* free curl handle */
   _pndman_curl_handle_free(chandle);
}

/* \brief comment pull callback */
static void _pndman_api_comment_pull_cb(pndman_curl_code code,
      void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_comment_packet *packet = (pndman_comment_packet*)data;

   if (code != PNDMAN_CURL_DONE &&
       code != PNDMAN_CURL_FAIL)
      return;

   _pndman_json_comment_pull(packet->callback, packet->pnd, chandle->file);
   DEBUG(PNDMAN_LEVEL_CRAP, "comment pull: %s",
         code==PNDMAN_CURL_DONE?"OK":info);

   free(packet);
   _pndman_curl_handle_free(chandle);
}

/* \brief comment callback */
static void _pndman_api_comment_cb(pndman_api_request *request)
{
   char url[PNDMAN_URL];
   char buffer[PNDMAN_POST];
   pndman_comment_packet *packet = (pndman_comment_packet*)request->data;

   request->handle->callback = _pndman_api_common_cb;
   request->handle->data     = API_COMMENT;

   snprintf(url, PNDMAN_URL-1, "%s/%s", packet->repository->api.root, API_COMMENT);
   snprintf(buffer, PNDMAN_POST-1, "id=%s&c=%s", packet->pnd->id, packet->comment);
   _pndman_curl_handle_set_url(request->handle, url);
   _pndman_curl_handle_set_post(request->handle, buffer);
   if (_pndman_curl_handle_perform(request->handle) == RETURN_OK)
      request->handle = NULL;

   /* free request */
   _pndman_api_request_free(request);
}

/* \brief rate callback */
static void _pndman_api_rate_cb(pndman_api_request *request)
{
   char url[PNDMAN_URL];
   char buffer[PNDMAN_POST];
   pndman_rate_packet *packet = (pndman_rate_packet*)request->data;

   request->handle->callback = _pndman_api_common_cb;
   request->handle->data     = API_RATE;

   snprintf(url, PNDMAN_URL-1, "%s/%s", packet->repository->api.root, API_RATE);
   snprintf(buffer, PNDMAN_POST-1, "id=%s&r=%d", packet->pnd->id, packet->rate);
   _pndman_curl_handle_set_url(request->handle, url);
   _pndman_curl_handle_set_post(request->handle, buffer);
   if (_pndman_curl_handle_perform(request->handle) == RETURN_OK)
      request->handle = NULL;

   /* free request */
   _pndman_api_request_free(request);
}

/* \brief authenticated download callback */
static void _pndman_api_download_cb(pndman_api_request *request)
{
   char url[PNDMAN_URL];
   pndman_download_packet *packet = (pndman_download_packet*)request->data;
   pndman_package_handle *handle  = packet->handle;

   /* reuse the curl handle and forward to handle.c :) */
   request->handle->callback = _pndman_package_handle_done;
   request->handle->data     = handle;

   snprintf(url, PNDMAN_URL-1, "%s&a=false", handle->pnd->url);
   _pndman_curl_handle_set_url(request->handle, url);
   _pndman_curl_handle_set_post(request->handle, "");
   if (_pndman_curl_handle_perform(request->handle) == RETURN_OK)
      request->handle = NULL;

   /* free reuquest */
   _pndman_api_request_free(request);
}

/* \brief create new handshake packet */
static pndman_handshake_packet* _pndman_api_handshake_packet(
      const char *key, const char *username)
{
   pndman_handshake_packet *packet;
   if (!(packet = malloc(sizeof(pndman_handshake_packet))))
      goto fail;
   memset(packet, 0, sizeof(pndman_handshake_packet));
   strcpy(packet->username, username);
   strcpy(packet->key, key);

   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_handshake_packet");
   return NULL;
}

/* \brief create new comment packet */
static pndman_download_packet* _pndman_api_download_packet(
      pndman_package_handle *handle)
{
   pndman_download_packet *packet;
   if (!(packet = malloc(sizeof(pndman_download_packet))))
      goto fail;
   memset(packet, 0, sizeof(pndman_download_packet));
   packet->handle = handle;

   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_download_packet");
   return NULL;
}

/* \brief create new comment packet */
static pndman_comment_packet* _pndman_api_comment_packet(
      pndman_repository *repo, pndman_package *pnd, const char *comment)
{
   pndman_comment_packet *packet;
   if (!(packet = malloc(sizeof(pndman_comment_packet))))
      goto fail;
   memset(packet, 0, sizeof(pndman_comment_packet));
   packet->pnd = pnd;
   packet->repository = repo;

   if (comment)
      strncpy(packet->comment, comment, PNDMAN_API_COMMENT_LEN-1);

   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_comment_packet");
   return NULL;
}

/* \brief create new rate packet */
static pndman_rate_packet* _pndman_api_rate_packet(
      pndman_repository *repo, pndman_package *pnd, int rate)
{
   pndman_rate_packet *packet;
   if (!(packet = malloc(sizeof(pndman_rate_packet))))
      goto fail;
   memset(packet, 0, sizeof(pndman_rate_packet));
   packet->pnd = pnd;
   packet->repository = repo;
   packet->rate = rate;

   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_rate_packet");
   return NULL;
}

/* \brief generate nonce */
static int _pndman_api_generate_nonce(char *buffer)
{
   unsigned int i;
   char *md5 = NULL;

   srand(time(0));
   for (i = 0; i != 10; ++i)
      buffer[i] = rand() % 255;

   if (!(md5 = _pndman_md5_buf(buffer, 10)))
      goto fail;

   /* cpy and free */
   strcpy(buffer, md5);
   free(md5);
   return RETURN_OK;

fail:
   DEBFAIL(API_NONCE_FAIL);
   return RETURN_OK;
}

/* \brief check if handshake was success
 * Final stage of handshaking, check the output from repo.
 * On failure request is freed
 *
 * Otherwise if there is callback, the request is carried over to the
 * callback for future handling. */
static void _pndman_api_handshake_check(pndman_api_request *request)
{
   pndman_api_status status;
   pndman_curl_handle *handle = request->handle;

   /* check return value */
   if (_pndman_json_client_api_return(handle->file,
               &status) != RETURN_OK)
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "Handshake success!");

   /* call callback, if needed */
   if (request->callback) request->callback(request);
   else _pndman_api_request_free(request);
   return;

fail:
   DEBFAIL(API_FAIL, status.number, status.text);
   _pndman_api_request_free(request);
}

/* \brief handshake perform
 * Send nonce hashed key and plain text username.
 * On failure request is freed */
static void _pndman_api_handshake_perform(pndman_api_request *request)
{
   char cnonce[NONCE_LEN];
   char buffer[PNDMAN_POST], *hash = NULL;
   char nonce[1024];

   pndman_handshake_packet *handshake = request->handshake;
   pndman_curl_handle *handle = request->handle;

   if (_pndman_api_generate_nonce(cnonce) != RETURN_OK)
      goto fail;

   if (_pndman_json_get_value("nonce", nonce, sizeof(nonce),
            handle->file) != RETURN_OK)
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "%s", nonce);
   sprintf(buffer, "%s%s%s", nonce, cnonce, handshake->key);
   if (!(hash = _pndman_md5_buf(buffer, strlen(buffer))))
      goto hash_fail;

   request->type = PNDMAN_API_HANDSHAKE;
   snprintf(buffer, PNDMAN_POST-1,
         "stage=2&cnonce=%s&user=%s&hash=%s", cnonce, handshake->username, hash);
   free(hash);

   /* callback should be _pndman_api_handshake_cb */
   _pndman_curl_handle_set_post(handle, buffer);
   if (_pndman_curl_handle_perform(handle) != RETURN_OK)
      goto fail;

   return;

hash_fail:
   DEBFAIL(API_HASH_FAIL);
fail:
   IFDO(free, hash);
   _pndman_api_request_free(request);
}

/* \brief api request callback
 * Internal callback for handshake.
 * On failure request is freed. */
static void _pndman_api_handshake_cb(pndman_curl_code code,
      void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_api_request *handle = (pndman_api_request*)data;
   if (code == PNDMAN_CURL_DONE) {
      if (handle->type == PNDMAN_API_NONCE)
         _pndman_api_handshake_perform(handle);
      else if (handle->type == PNDMAN_API_HANDSHAKE)
         _pndman_api_handshake_check(handle);
   } else if (code == PNDMAN_CURL_FAIL) {
      DEBFAIL("handshake failed : %s", info);
      _pndman_api_request_free(handle);
   }
}

/* \brief handshake with repository
 * Creates api request and reuses passed curl handle.
 * Request will be freed on failures, on success the callback will handle freeing. */
static int _pndman_api_handshake(pndman_curl_handle *handle,
      pndman_repository *repo, pndman_api_callback callback,
      void *data)
{
   char url[PNDMAN_URL];
   pndman_api_request *request;
   assert(handle && repo && callback && data);

   if (!(request = _pndman_api_request_new(PNDMAN_API_NONCE, handle)))
      goto fail;

   if (!(request->handshake = _pndman_api_handshake_packet(repo->api.key, repo->api.username)))
      goto fail;

   snprintf(url, PNDMAN_URL-1, "%s/%s", repo->api.root, API_HANDSHAKE);

   request->data     = data;
   request->callback = callback;
   handle->callback  = _pndman_api_handshake_cb;
   handle->data      = request;
   _pndman_curl_handle_set_url(request->handle, url);
   _pndman_curl_handle_set_post(request->handle, "stage=1");
   if (_pndman_curl_handle_perform(handle) != RETURN_OK)
      goto fail;

   return RETURN_OK;

fail:
   IFDO(_pndman_api_request_free, request);
   return RETURN_FAIL;
}

/* INTERNAL API */

/* \brief handshake for commercial download */
int _pndman_api_commercial_download(pndman_curl_handle *handle,
      pndman_package_handle *package)
{
   pndman_download_packet *packet;
   assert(package->repository);

   if (!(packet = _pndman_api_download_packet(package)))
      goto fail;

   return _pndman_api_handshake(handle, package->repository,
         _pndman_api_download_cb, packet);

fail:
   return RETURN_FAIL;
}

/* \brief rate pnd */
int _pndman_rate_pnd(pndman_package *pnd, pndman_repository *repository, int rate)
{
   assert(pnd && repository);

   pndman_rate_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(pnd && repository);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, NULL, NULL)))
      goto fail;

   if (!(packet = _pndman_api_rate_packet(repository, pnd, rate)))
      goto fail;

   return _pndman_api_handshake(handle, repository,
         _pndman_api_rate_cb, packet);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   return RETURN_FAIL;
}

/* \brief comment pnd */
int _pndman_comment_pnd(pndman_package *pnd, pndman_repository *repository,
      const char *comment)
{
   pndman_comment_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(pnd && repository && comment);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, NULL, NULL)))
      goto fail;

   if (!(packet = _pndman_api_comment_packet(repository, pnd, comment)))
      goto fail;

   return _pndman_api_handshake(handle, repository,
         _pndman_api_comment_cb, packet);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   return RETURN_FAIL;
}

/* \brief get comments for pnd */
int _pndman_comment_pnd_pull(pndman_package *pnd, pndman_repository *repository,
      pndman_api_comment_callback callback)
{
   char url[PNDMAN_URL];
   char buffer[PNDMAN_POST];
   pndman_comment_packet *packet = NULL;
   pndman_curl_handle *handle;

   if (!(handle = _pndman_curl_handle_new(NULL, NULL,
               _pndman_api_comment_pull_cb, NULL)))
      goto fail;

   if (!(packet = _pndman_api_comment_packet(repository, pnd, NULL)))
      goto fail;

   snprintf(url, PNDMAN_URL-1, "%s/%s", repository->api.root, API_COMMENT);
   snprintf(buffer, sizeof(buffer)-1, "id=%s&pull=true", pnd->id);

   handle->data = packet;
   packet->callback = callback;
   _pndman_curl_handle_set_url(handle, url);
   _pndman_curl_handle_set_post(handle, buffer);
   return _pndman_curl_handle_perform(handle);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   return RETURN_FAIL;
}
