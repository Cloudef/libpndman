#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#define API_HANDSHAKE         "handshake"
#define API_RATE              "rate"
#define API_COMMENT           "comment"
#define API_DOWNLOAD_HISTORY  "history"
#define API_ARCHIVED          "archived"
#define NONCE_LEN 33

/* helper macro for request failuers
 * p = packet, cb = callback, ud = userdata, st = status */
#define PNDMAN_API_FAIL(p, cb, ud, st) \
   IFDO(free, p.error);  \
   p.error = st; \
   p.user_data = ud; cb(PNDMAN_CURL_FAIL, &p);

/* forward declaration */
struct pndman_api_request;

/* \brief api callback function */
typedef void (*pndman_api_callback)(
      const char *info, struct pndman_api_request *request);

/* \brief api request type enum */
typedef enum pndman_api_request_type
{
   PNDMAN_API_NONCE,
   PNDMAN_API_HANDSHAKE,
} pndman_api_request_type;

/* \brief handshake packet */
typedef struct pndman_handshake_packet
{
   char *username;
   char *key;
} pndman_handshake_packet;

/* \brief comment packet */
typedef struct pndman_comment_packet
{
   pndman_api_generic_callback callback; /* for send comment */
   void *user_data;

   char *comment;
   pndman_package *pnd;
   pndman_repository *repository;
   pndman_api_comment_callback pull_callback; /* for comment pull */
   time_t timestamp; /* for removal */
} pndman_comment_packet;

/* \brief rate packet */
typedef struct pndman_rate_packet
{
   pndman_api_rate_callback callback;
   void *user_data;

   int rate;
   pndman_package *pnd;
   pndman_repository *repository;
} pndman_rate_packet;

/* \brief download history packet */
typedef struct pndman_history_packet
{
   pndman_api_history_callback callback;
   void *user_data;

   pndman_repository *repository;
} pndman_history_packet;

/* \brief archived pnd packet */
typedef struct pndman_archived_packet
{
   pndman_api_archived_callback callback;
   void *user_data;

   pndman_package *pnd;
   pndman_repository *repository;
} pndman_archived_packet;

/* \brief download packet */
typedef struct pndman_download_packet
{
   char *download_path;
   pndman_package_handle *handle;
} pndman_download_packet;

/* \brief generic packet
 * NOTE: this struct is just for recasting the
 * above packets, so the order of data above must be same.
 * And only for struct which have
 * pndman_api_generic_callback member */
typedef struct pndman_generic_packet
{
   pndman_api_generic_callback callback;
   void *user_data;
} pndman_generic_packet;

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
static pndman_api_request* _pndman_api_request_new(pndman_api_request_type type, pndman_curl_handle *handle)
{
   pndman_api_request *request;
   if (!(request = calloc(1, sizeof(pndman_api_request))))
      goto fail;

   request->type = type;
   request->handle = handle;
   return request;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_api_request");
   return NULL;
}

/* \brief internal free of internal api request */
static void _pndman_api_request_free(pndman_api_request *request)
{
   if (request->handshake) {
      IFDO(free, request->handshake->username);
      IFDO(free, request->handshake->key);
   }
   IFDO(free, request->handshake);
   IFDO(free, request->data);
   IFDO(_pndman_curl_handle_free, request->handle);
   free(request);
}

/* \brief common api callback */
static void _pndman_api_common_cb(pndman_curl_code code, void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_api_status status;
   pndman_generic_packet *packet = (pndman_generic_packet*)data;

   if (code != PNDMAN_CURL_DONE &&
       code != PNDMAN_CURL_FAIL)
      return;

   /* error checking, etc.. */
   if (code == PNDMAN_CURL_FAIL) {
      packet->callback(PNDMAN_CURL_FAIL, info, packet->user_data);
   } else if (chandle && _pndman_json_client_api_return(chandle->file, &status) != RETURN_OK && status.text) {
      packet->callback(PNDMAN_CURL_FAIL, status.text, packet->user_data);
      free(status.text);
   } else { /* success */
      packet->callback(PNDMAN_CURL_DONE, NULL, packet->user_data);
   }

   /* dealloc */
   IFDO(_pndman_curl_handle_free, chandle);
   IFDO(free, packet);
}

/* \brief rating api callback */
static void _pndman_api_rating_cb(pndman_curl_code code, void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_api_status status;
   pndman_api_rate_packet rpacket;
   pndman_rate_packet *packet = (pndman_rate_packet*)data;

   if (code != PNDMAN_CURL_DONE &&
       code != PNDMAN_CURL_FAIL)
      return;

   if (code == PNDMAN_CURL_DONE && !chandle)
      code = PNDMAN_CURL_FAIL;

   /* init */
   memset(&rpacket, 0, sizeof(pndman_api_rate_packet));
   rpacket.pnd       = packet->pnd;
   rpacket.user_data = packet->user_data;
   rpacket.rating    = packet->rate;

   /* error checking, etc.. */
   if (code == PNDMAN_CURL_FAIL) {
      PNDMAN_API_FAIL(rpacket, packet->callback, packet->user_data, (char*)info);
   } else if (chandle && _pndman_json_client_api_return(chandle->file, &status) != RETURN_OK && status.text) {
      PNDMAN_API_FAIL(rpacket, packet->callback, packet->user_data, status.text);
      free(status.text);
   } else { /* success */
      if (packet->rate)
         _pndman_json_get_int_value("new rating", &rpacket.total_rating, chandle->file);
      if (packet->rate == 0)
         _pndman_json_get_int_value("rating", &rpacket.rating, chandle->file);
      packet->callback(PNDMAN_CURL_DONE, &rpacket);
   }

   /* dealloc */
   IFDO(_pndman_curl_handle_free, chandle);
   IFDO(free, packet);
}

/* \brief comment pull callback */
static void _pndman_api_comment_pull_cb(pndman_curl_code code, void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_api_status status;
   pndman_api_comment_packet cpacket;
   pndman_comment_packet *packet = (pndman_comment_packet*)data;

   if (code != PNDMAN_CURL_DONE &&
       code != PNDMAN_CURL_FAIL)
      return;

   if (code == PNDMAN_CURL_DONE && !chandle)
      code = PNDMAN_CURL_FAIL;

   /* init */
   memset(&cpacket, 0, sizeof(pndman_api_comment_packet));
   cpacket.pnd = packet->pnd;
   cpacket.user_data = packet->user_data;

   /* error checking, etc.. */
   if (code == PNDMAN_CURL_FAIL) {
      PNDMAN_API_FAIL(cpacket, packet->pull_callback, packet->user_data, (char*)info);
   } else if (chandle && _pndman_json_client_api_return(chandle->file, &status) != RETURN_OK && status.text) {
      PNDMAN_API_FAIL(cpacket, packet->pull_callback, packet->user_data, status.text);
      free(status.text);
   } else {
      _pndman_json_comment_pull(packet->user_data, packet->pull_callback, packet->pnd, chandle->file);
   }

   /* dealloc */
   IFDO(free, packet->comment);
   free(packet);
   IFDO(_pndman_curl_handle_free, chandle);
}

/* \brief dowload history callback */
static void _pndman_api_download_history_cb(pndman_curl_code code, void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_api_status status;
   pndman_api_history_packet hpacket;
   pndman_history_packet *packet = (pndman_history_packet*)data;

   if (code != PNDMAN_CURL_DONE &&
       code != PNDMAN_CURL_FAIL)
      return;

   if (code == PNDMAN_CURL_DONE && !chandle)
      code = PNDMAN_CURL_FAIL;

   /* init */
   memset(&hpacket, 0, sizeof(pndman_api_history_packet));
   hpacket.user_data = packet->user_data;

   /* error checking etc.. */
   if (code == PNDMAN_CURL_FAIL) {
      PNDMAN_API_FAIL(hpacket, packet->callback, packet->user_data, (char*)info);
   } else if (chandle && _pndman_json_client_api_return(chandle->file, &status) != RETURN_OK && status.text) {
      PNDMAN_API_FAIL(hpacket, packet->callback, packet->user_data, status.text);
      free(status.text);
   } else {
      _pndman_json_download_history(packet->user_data, packet->callback, chandle->file);
   }

   /* dealloc */
   free(packet);
   IFDO(_pndman_curl_handle_free, chandle);
}

/* \brief archived PND callback */
static void _pndman_api_archived_cb(pndman_curl_code code, void *data, const char *info, pndman_curl_handle *chandle)
{
   pndman_api_archived_packet apacket;
   pndman_api_status status;
   pndman_archived_packet *packet = (pndman_archived_packet*)data;

   if (code != PNDMAN_CURL_DONE &&
       code != PNDMAN_CURL_FAIL)
      return;

   if (code == PNDMAN_CURL_DONE && !chandle)
      code = PNDMAN_CURL_FAIL;

   /* init */
   memset(&apacket, 0, sizeof(pndman_api_archived_packet));
   apacket.pnd       = packet->pnd;
   apacket.user_data = packet->user_data;

   /* error checking, etc.. */
   if (code == PNDMAN_CURL_FAIL) {
      PNDMAN_API_FAIL(apacket, packet->callback, packet->user_data, (char*)info);
   } else if (chandle && _pndman_json_client_api_return(chandle->file, &status) != RETURN_OK && status.text) {
      PNDMAN_API_FAIL(apacket, packet->callback, packet->user_data, status.text);
      free(status.text);
   } else { /* success */
      _pndman_json_archived_pnd(packet->pnd, chandle->file);
      packet->callback(PNDMAN_CURL_DONE, &apacket);
   }

   free(packet);
   IFDO(_pndman_curl_handle_free, chandle);
}

/* \brief comment callback */
static void _pndman_api_comment_cb(const char *info, pndman_api_request *request)
{
   pndman_comment_packet *packet = (pndman_comment_packet*)request->data;

   request->handle->callback = _pndman_api_common_cb;
   request->handle->data     = packet;

   /* we free this here */
   request->data = NULL;

   /* error handling */
   if (info) {
      packet->callback(PNDMAN_CURL_FAIL, info, packet->user_data);
   } else {
      char *url;
      int size = snprintf(NULL, 0, "%s/%s", packet->repository->api.root, API_COMMENT)+1;
      if (!(url = malloc(size))) {
         packet->callback(PNDMAN_CURL_FAIL, "out of memory", packet->user_data);
         return;
      }
      sprintf(url, "%s/%s", packet->repository->api.root, API_COMMENT);
      _pndman_curl_handle_set_url(request->handle, url);
      free(url);

      char buffer[256];
      if (!packet->timestamp) snprintf(buffer, sizeof(buffer)-1, "id=%s&c=%s", packet->pnd->id, packet->comment);
      else                    snprintf(buffer, sizeof(buffer)-1, "id=%s&delete=true&time=%lu", packet->pnd->id, packet->timestamp);
      _pndman_curl_handle_set_post(request->handle, buffer);

      if (_pndman_curl_handle_perform(request->handle) == RETURN_OK) {
         request->handle = NULL;
      } else {
         packet->callback(PNDMAN_CURL_FAIL, "_pndman_curl_handle_perform failed", packet->user_data);
      }
   }

   /* free request */
   if (request->handle) {
      IFDO(free, packet->comment);
      free(packet);
   }
   _pndman_api_request_free(request);
}

/* \brief rate callback */
static void _pndman_api_rate_cb(const char *info, pndman_api_request *request)
{
   pndman_api_rate_packet rpacket;
   pndman_rate_packet *packet = (pndman_rate_packet*)request->data;

   request->handle->callback = _pndman_api_rating_cb;
   request->handle->data     = packet;

   /* we free this here */
   request->data = NULL;

   /* error handling */
   if (info) {
      memset(&rpacket, 0, sizeof(pndman_api_rate_packet));
      rpacket.pnd = packet->pnd;
      PNDMAN_API_FAIL(rpacket, packet->callback, packet->user_data, (char*)info);
   } else {
      char *url;
      int size = snprintf(NULL, 0, "%s/%s", packet->repository->api.root, API_RATE)+1;
      if (!(url = malloc(size))) {
         PNDMAN_API_FAIL(rpacket, packet->callback, packet->user_data, "out of memory");
         return;
      }
      sprintf(url, "%s/%s", packet->repository->api.root, API_RATE);
      _pndman_curl_handle_set_url(request->handle, url);
      free(url);

      char buffer[256];
      if (packet->rate) snprintf(buffer, sizeof(buffer)-1, "id=%s&a=r&r=%d", packet->pnd->id, packet->rate);
      else              snprintf(buffer, sizeof(buffer)-1, "id=%s&a=gr", packet->pnd->id);
      _pndman_curl_handle_set_post(request->handle, buffer);

      if (_pndman_curl_handle_perform(request->handle) == RETURN_OK) {
         request->handle = NULL;
         request->data   = NULL;
      } else {
         memset(&rpacket, 0, sizeof(pndman_api_rate_packet));
         rpacket.pnd = packet->pnd;
         PNDMAN_API_FAIL(rpacket, packet->callback, packet->user_data, "_pndman_curl_handle_perform failed");
      }
   }

   /* free request */
   if (request->handle) free(packet);
   _pndman_api_request_free(request);
}

/* \brief authenticated download callback */
static void _pndman_api_download_cb(const char *info, pndman_api_request *request)
{
   pndman_download_packet *packet = (pndman_download_packet*)request->data;
   pndman_package_handle *handle  = packet->handle;

   /* reuse the curl handle and forward to handle.c :) */
   request->handle->callback = _pndman_package_handle_done;
   request->handle->data     = handle;

   /* error handling */
   if (info) {
      _pndman_package_handle_done(PNDMAN_CURL_FAIL, handle, info, NULL);
   } else {
      char *url;
      int size = snprintf(NULL, 0, "%s&a=false", handle->pnd->url)+1;
      if (!(url = malloc(size))) {
         _pndman_package_handle_done(PNDMAN_CURL_FAIL, handle, "out of memory", NULL);
         return;
      }
      sprintf(url, "%s&a=false", handle->pnd->url);
      _pndman_curl_handle_set_url(request->handle, url);
      free(url);

      IFDO(free, request->handle->path);
      if (packet->download_path) request->handle->path = strdup(packet->download_path);
      _pndman_curl_handle_set_post(request->handle, "");

      if (_pndman_curl_handle_perform(request->handle) != RETURN_OK) {
         _pndman_package_handle_done(PNDMAN_CURL_FAIL, handle, "_pndman_curl_handle_perform failed", NULL);
      }
   }

   /* user will free this handle! */
   request->handle = NULL;

   /* free request */
   IFDO(free, packet->download_path);
   _pndman_api_request_free(request);
}

/* \brief create new handshake packet */
static pndman_handshake_packet* _pndman_api_handshake_packet(const char *key, const char *username)
{
   pndman_handshake_packet *packet;
   if (!(packet = calloc(1, sizeof(pndman_handshake_packet))))
      goto fail;

   IFDO(free, packet->username);
   if (username) packet->username = strdup(username);
   IFDO(free, packet->key);
   if (key) packet->key = strdup(key);
   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_handshake_packet");
   return NULL;
}

/* \brief create new comment packet */
static pndman_download_packet* _pndman_api_download_packet(const char *path, pndman_package_handle *handle)
{
   pndman_download_packet *packet;
   if (!(packet = calloc(1, sizeof(pndman_download_packet))))
      goto fail;

   packet->handle = handle;
   if (path) packet->download_path = strdup(path);
   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_download_packet");
   return NULL;
}

/* \brief create new comment packet */
static pndman_comment_packet* _pndman_api_comment_packet(
      pndman_repository *repo, pndman_package *pnd,
      const char *comment, time_t timestamp)
{
   pndman_comment_packet *packet;
   if (!(packet = calloc(1, sizeof(pndman_comment_packet))))
      goto fail;

   packet->pnd = pnd;
   packet->repository = repo;
   packet->timestamp = timestamp;

   if (comment) packet->comment = strdup(comment);
   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_comment_packet");
   return NULL;
}

/* \brief create new rate packet */
static pndman_rate_packet* _pndman_api_rate_packet(pndman_repository *repo, pndman_package *pnd, int rate)
{
   pndman_rate_packet *packet;
   if (!(packet = calloc(1, sizeof(pndman_rate_packet))))
      goto fail;

   packet->pnd = pnd;
   packet->repository = repo;
   packet->rate = rate;
   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_rate_packet");
   return NULL;
}

/* \brief create new download history packet */
static pndman_history_packet* _pndman_api_history_packet(pndman_repository *repo, pndman_api_history_callback callback)
{
   pndman_history_packet *packet;
   if (!(packet = calloc(1, sizeof(pndman_history_packet))))
      goto fail;

   packet->repository = repo;
   packet->callback = callback;
   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_history_packet");
   return NULL;
}

/* \brief create new archived packet */
static pndman_archived_packet* _pndman_api_archived_packet(pndman_repository *repo, pndman_package *pnd)
{
   pndman_archived_packet *packet;
   if (!(packet = calloc(1, sizeof(pndman_archived_packet))))
      goto fail;

   packet->pnd = pnd;
   packet->repository = repo;
   return packet;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_archived_packet");
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
   strncpy(buffer, md5, NONCE_LEN-1);
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
   if (_pndman_json_client_api_return(handle->file, &status) != RETURN_OK)
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "Handshake success!");
   request->callback(NULL, request);
   return;

fail:
   request->callback(status.text, request);
   DEBFAIL(API_FAIL, status.number, status.text);
   IFDO(free, status.text);
}

/* \brief perform download history after handshake */
static void _pndman_api_download_history_perform(const char *info, pndman_api_request *request)
{
   pndman_api_history_packet hpacket;
   pndman_history_packet *packet = (pndman_history_packet*)request->data;

   request->handle->callback = _pndman_api_download_history_cb;
   request->handle->data     = packet;

   /* error checking */
   if (info) {
      memset(&hpacket, 0, sizeof(pndman_api_history_packet));
      PNDMAN_API_FAIL(hpacket, packet->callback, packet->user_data, (char*)info);
   } else { /* success */
      char *url;
      int size = snprintf(NULL, 0, "%s/%s", packet->repository->api.root, API_DOWNLOAD_HISTORY)+1;
      if (!(url = malloc(size))) {
         PNDMAN_API_FAIL(hpacket, packet->callback, packet->user_data, "out of memory");
         return;
      }
      sprintf(url, "%s/%s", packet->repository->api.root, API_DOWNLOAD_HISTORY);
      _pndman_curl_handle_set_url(request->handle, url);
      free(url);

      if (_pndman_curl_handle_perform(request->handle) == RETURN_OK) {
         request->handle = NULL;
         request->data   = NULL;
      }
   }

   /* free request */
   _pndman_api_request_free(request);
}

/* \brief handshake perform
 * Send nonce hashed key and plain text username.
 * On failure request is freed */
static void _pndman_api_handshake_perform(pndman_api_request *request)
{
   char cnonce[NONCE_LEN];
   char buffer[256], *hash = NULL;
   char *nonce = NULL;

   pndman_handshake_packet *handshake = request->handshake;
   pndman_curl_handle *handle = request->handle;

   memset(cnonce, 0, NONCE_LEN);
   if (_pndman_api_generate_nonce(cnonce) != RETURN_OK)
      goto fail;

   if (_pndman_json_get_value("nonce", &nonce, handle->file) != RETURN_OK)
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "%s", nonce);
   sprintf(buffer, "%s%s%s", nonce, cnonce, handshake->key);
   NULLDO(free, nonce);

   if (!(hash = _pndman_md5_buf(buffer, strlen(buffer))))
      goto hash_fail;

   request->type = PNDMAN_API_HANDSHAKE;
   snprintf(buffer, sizeof(buffer)-1, "stage=2&cnonce=%s&user=%s&hash=%s", cnonce, handshake->username, hash);
   NULLDO(free, hash);

   /* callback should be _pndman_api_handshake_cb */
   _pndman_curl_handle_set_post(handle, buffer);
   if (_pndman_curl_handle_perform(handle) != RETURN_OK)
      goto fail;

   return;

hash_fail:
   DEBFAIL(API_HASH_FAIL);
fail:
   IFDO(free, hash);
   IFDO(free, nonce);
   request->callback("_pndman_api_handshake_perform failed", request);
}

/* \brief api request callback
 * Internal callback for handshake.
 * On failure request is freed. */
static void _pndman_api_handshake_cb(pndman_curl_code code,
      void *data, const char *info, pndman_curl_handle *chandle)
{
   (void)chandle;
   pndman_api_request *handle = (pndman_api_request*)data;
   if (code == PNDMAN_CURL_DONE) {
      if (handle->type == PNDMAN_API_NONCE)
         _pndman_api_handshake_perform(handle);
      else if (handle->type == PNDMAN_API_HANDSHAKE)
         _pndman_api_handshake_check(handle);
   } else if (code == PNDMAN_CURL_FAIL) {
      DEBFAIL("handshake failed : %s", info);
      handle->callback(info, handle);
   }
}

/* \brief handshake with repository
 * Creates api request and reuses passed curl handle.
 * Request will be freed on failures, on success the callback will handle freeing. */
static int _pndman_api_handshake(pndman_curl_handle *handle,
      pndman_repository *repo, pndman_api_callback callback,
      void *data)
{
   pndman_api_request *request = NULL;
   assert(handle && repo && callback && data);

   if (!(request = _pndman_api_request_new(PNDMAN_API_NONCE, handle)))
      goto fail;

   if (!(request->handshake = _pndman_api_handshake_packet(repo->api.key, repo->api.username)))
      goto fail;

   char *url;
   int size = snprintf(NULL, 0, "%s/%s", repo->api.root, API_HANDSHAKE)+1;
   if (!(url = malloc(size))) goto fail;
   sprintf(url, "%s/%s", repo->api.root, API_HANDSHAKE);
   _pndman_curl_handle_set_url(request->handle, url);
   _pndman_curl_handle_set_post(request->handle, "stage=1");
   free(url);

   request->data     = data;
   request->callback = callback;
   handle->callback  = _pndman_api_handshake_cb;
   handle->data      = request;

   if (!repo->api.key || !repo->api.username ||
       !strlen(repo->api.key) || !strlen(repo->api.username))
      goto no_user_key;

   if (_pndman_curl_handle_perform(handle) != RETURN_OK)
      goto perform_fail;

   return RETURN_OK;

no_user_key:
   callback("no username or key given", request);
   return RETURN_FAIL;
perform_fail:
   callback("_pndman_api_handshake failed", request);
   return RETURN_FAIL;
fail:
   IFDO(_pndman_api_request_free, request);
   return RETURN_FAIL;
}

/* \brief rate pnd */
static int _pndman_api_rate_pnd(void *user_data,
      pndman_package *pnd, pndman_repository *repository, int rate,
      pndman_api_rate_callback callback)
{
   pndman_rate_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(pnd && repository && callback);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, NULL, NULL)))
      goto fail;

   if (!(packet = _pndman_api_rate_packet(repository, pnd, rate)))
      goto fail;

   packet->callback  = callback;
   packet->user_data = user_data;

   return _pndman_api_handshake(handle, repository, _pndman_api_rate_cb, packet);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   IFDO(free, packet);
   return RETURN_FAIL;
}

/* \brief comment pnd */
static int _pndman_api_comment_pnd(void *user_data,
      pndman_package *pnd, pndman_repository *repository, const char *comment,
      pndman_api_generic_callback callback)
{
   pndman_comment_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(pnd && repository && comment && callback);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, NULL, NULL)))
      goto fail;

   if (!(packet = _pndman_api_comment_packet(repository, pnd, comment, 0)))
      goto fail;

   packet->callback  = callback;
   packet->user_data = user_data;

   return _pndman_api_handshake(handle, repository, _pndman_api_comment_cb, packet);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   IFDO(free, packet);
   return RETURN_FAIL;
}

/* \brief get comments for pnd */
static int _pndman_api_comment_pnd_pull(void *user_data,
      pndman_package *pnd, pndman_repository *repository,
      pndman_api_comment_callback callback)
{
   pndman_comment_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(pnd && repository && callback);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, _pndman_api_comment_pull_cb, NULL)))
      goto fail;

   if (!(packet = _pndman_api_comment_packet(repository, pnd, NULL, 0)))
      goto fail;

   char *url;
   int size = snprintf(NULL, 0, "%s/%s", repository->api.root, API_COMMENT)+1;
   if (!(url = malloc(size))) goto fail;
   sprintf(url, "%s/%s", repository->api.root, API_COMMENT);
   _pndman_curl_handle_set_url(handle, url);
   free(url);

   char buffer[256];
   snprintf(buffer, sizeof(buffer)-1, "id=%s&pull=true", pnd->id);
   _pndman_curl_handle_set_post(handle, buffer);

   handle->data = packet;
   packet->pull_callback = callback;
   packet->user_data = user_data;
   return _pndman_curl_handle_perform(handle);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   IFDO(free, packet);
   return RETURN_FAIL;
}

/* \brief delete comment */
static int _pndman_api_comment_pnd_delete(void *user_data,
      pndman_package *pnd, time_t timestamp, pndman_repository *repository,
      pndman_api_generic_callback callback)
{
   pndman_comment_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(repository && callback);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, NULL, NULL)))
      goto fail;

   if (!(packet = _pndman_api_comment_packet(repository, pnd, NULL, timestamp)))
      goto fail;

   packet->callback  = callback;
   packet->user_data = user_data;
   return _pndman_api_handshake(handle, repository, _pndman_api_comment_cb, packet);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   IFDO(free, packet);
   return RETURN_FAIL;
}

/* \brief get download history */
static int _pndman_api_download_history(void *user_data,
      pndman_repository *repository, pndman_api_history_callback callback)
{
   pndman_history_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(repository && callback);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, NULL, NULL)))
      goto fail;

   if (!(packet = _pndman_api_history_packet(repository, callback)))
      goto fail;

   packet->user_data = user_data;
   return _pndman_api_handshake(handle, repository, _pndman_api_download_history_perform, packet);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   IFDO(free, packet);
   return RETURN_FAIL;
}

/* \brief get archived pnds */
static int _pndman_api_archived_pnd(void *user_data,
      pndman_package *pnd, pndman_repository *repository,
      pndman_api_archived_callback callback)
{
   pndman_archived_packet *packet = NULL;
   pndman_curl_handle *handle;
   assert(pnd && repository && callback);

   if (!(handle = _pndman_curl_handle_new(NULL, NULL, _pndman_api_archived_cb, NULL)))
      goto fail;

   if (!(packet = _pndman_api_archived_packet(repository, pnd)))
      goto fail;

   char *url;
   int size = snprintf(NULL, 0, "%s/%s", repository->api.root, API_ARCHIVED)+1;
   if (!(url = malloc(size))) goto fail;
   sprintf(url, "%s/%s", repository->api.root, API_ARCHIVED);
   _pndman_curl_handle_set_url(handle, url);
   free(url);

   char buffer[256];
   snprintf(buffer, sizeof(buffer)-1, "id=%s", pnd->id);
   _pndman_curl_handle_set_post(handle, buffer);

   handle->data = packet;
   packet->callback = callback;
   packet->user_data = user_data;
   return _pndman_curl_handle_perform(handle);

fail:
   IFDO(_pndman_curl_handle_free, handle);
   IFDO(free, packet);
   return RETURN_FAIL;
}

/* INTERNAL API */

/* \brief handshake for commercial download */
int _pndman_api_commercial_download(pndman_curl_handle *handle, pndman_package_handle *package)
{
   pndman_download_packet *packet;
   assert(handle && package && package->pnd && package->pnd->repositoryptr);

   if (!package->pnd->repositoryptr->prev) {
      BADUSE("repository is local repository");
      return RETURN_FAIL;
   }
   if (!package->pnd->repositoryptr->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!package->pnd->repositoryptr->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }

   if (!(packet = _pndman_api_download_packet(handle->path, package)))
      goto fail;

   return _pndman_api_handshake(handle, package->pnd->repositoryptr, _pndman_api_download_cb, packet);

fail:
   return RETURN_FAIL;
}

/* PUBLIC API */

/* \brief send comment to pnd */
PNDMANAPI int pndman_api_comment_pnd(void *user_data,
      pndman_package *pnd, const char *comment,
      pndman_api_generic_callback callback)
{
   CHECKUSE(pnd);
   CHECKUSE(pnd->repositoryptr);
   CHECKUSE(comment);
   CHECKUSE(callback);
   if (!pnd->repositoryptr->prev) {
      BADUSE("repository is local repository");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }

   return _pndman_api_comment_pnd(user_data, pnd, pnd->repositoryptr, comment, callback);
}

/* \brief get comments from pnd */
PNDMANAPI int pndman_api_comment_pnd_pull(void *user_data, pndman_package *pnd, pndman_api_comment_callback callback)
{
   CHECKUSE(pnd);
   CHECKUSE(pnd->repositoryptr);
   CHECKUSE(callback);
   if (!pnd->repositoryptr->prev) {
      BADUSE("repository is local repository");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }

   return _pndman_api_comment_pnd_pull(user_data, pnd, pnd->repositoryptr, callback);
}

/* \brief delete comment */
PNDMANAPI int pndman_api_comment_pnd_delete(void *user_data,
      pndman_package *pnd, time_t timestamp,
      pndman_api_generic_callback callback)
{
   CHECKUSE(pnd);
   CHECKUSE(pnd->repositoryptr);
   CHECKUSE(callback);
   if (!pnd->repositoryptr->prev) {
      BADUSE("repository is local repository");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }
   if (!timestamp) {
      BADUSE("timestamp must not be 0");
      return RETURN_FAIL;
   }

   return _pndman_api_comment_pnd_delete(user_data, pnd, timestamp, pnd->repositoryptr, callback);
}

/* \brief rate pnd */
PNDMANAPI int pndman_api_rate_pnd(void *user_data,
      pndman_package *pnd, int rate,
      pndman_api_rate_callback callback)
{
   CHECKUSE(pnd);
   CHECKUSE(pnd->repositoryptr);
   CHECKUSE(callback);
   if (!pnd->repositoryptr->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }
   if (rate<1 || rate>5) {
      BADUSE("rating must be in range of 1-5");
      return RETURN_FAIL;
   }

   return _pndman_api_rate_pnd(user_data, pnd, pnd->repositoryptr, rate, callback);
}

/* \brief get own pnd rating */
PNDMANAPI int pndman_api_get_own_rate_pnd(void *user_data, pndman_package *pnd, pndman_api_rate_callback callback)
{
   CHECKUSE(pnd);
   CHECKUSE(pnd->repositoryptr);
   CHECKUSE(callback);
   if (!pnd->repositoryptr->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }

   return _pndman_api_rate_pnd(user_data, pnd, pnd->repositoryptr, 0, callback);
}

/* \brief get download history */
PNDMANAPI int pndman_api_download_history(void *user_data, pndman_repository *repository, pndman_api_history_callback callback)
{
   CHECKUSE(repository);
   CHECKUSE(callback);
   if (!repository->prev) {
      BADUSE("repository is local repository");
      return RETURN_FAIL;
   }
   if (!repository->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!repository->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }

   return _pndman_api_download_history(user_data, repository, callback);
}

/* \brief get archived pnds
 * archived pnd's are store in next_installed */
PNDMANAPI int pndman_api_archived_pnd(void *user_data, pndman_package *pnd, pndman_api_archived_callback callback)
{
   CHECKUSE(pnd);
   CHECKUSE(pnd->repositoryptr);
   if (!pnd->repositoryptr->prev) {
      BADUSE("repository is local repository");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->api.root) {
      BADUSE("the repository is not loaded/synced, or does not support client api.");
      return RETURN_FAIL;
   }
   if (!pnd->repositoryptr->pnd) {
      BADUSE("repository has no packages");
      return RETURN_FAIL;
   }

   return _pndman_api_archived_pnd(user_data, pnd, pnd->repositoryptr, callback);
}

/* vim: set ts=8 sw=2 tw=0 :*/

