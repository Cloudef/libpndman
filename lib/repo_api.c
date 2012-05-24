#include "internal.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>

#define API_HANDSHAKE   "handshake"
#define API_RATE        "rate"
#define API_COMMENT     "comment"
#define NONCE_LEN 33

/* \brief api request type enum */
typedef enum pndman_api_request_type
{
   PNDMAN_API_NONCE,
   PNDMAN_API_HANDSHAKE,
   PNDMAN_API_RATE,
   PNDMAN_API_COMMENT
} pndman_api_request_type;

/* \brief internal api request */
typedef struct pndman_api_request
{
   pndman_api_request_type type;
   pndman_api_status status;
   pndman_curl_handle *handle;
   void *packet;
} pndman_api_request;

/* \brief handshake packet */
typedef struct pndman_handshake_packet
{
   char username[PNDMAN_SHRT_STR];
   char key[PNDMAN_STR];
} pndman_handshake_packet;

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
   IFDO(free, request->packet);
   IFDO(_pndman_curl_handle_free, request->handle);
   free(request);
}

/* \brief generate nonce */
static int _pndman_gen_nonce(char *buffer)
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

/* \brief check if handshake was success */
static void _pndman_handshake_check(pndman_api_request *request)
{
   pndman_api_status status;
   pndman_curl_handle *handle = request->handle;

   /* check return value */
   if (_pndman_json_client_api_return(handle->file,
               &status) != RETURN_OK)
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "Handshake success!");
   return;

fail:
   DEBFAIL(API_FAIL, status.number, status.text);
}

/* \brief handshake perform */
static void _pndman_handshake_perform(pndman_api_request *request)
{
   char cnonce[NONCE_LEN];
   char buffer[1024], *hash = NULL;
   char nonce[1024];

   pndman_handshake_packet *packet;
   pndman_curl_handle *handle = request->handle;
   packet = (pndman_handshake_packet*)request->packet;

   if (_pndman_gen_nonce(cnonce) != RETURN_OK)
      goto fail;

   if (_pndman_json_get_value("nonce", nonce, sizeof(nonce),
            request->handle->file) != RETURN_OK)
      goto fail;

   DEBUG(PNDMAN_LEVEL_CRAP, "%s", nonce);
   sprintf(buffer, "%s%s%s", nonce, cnonce, packet->key);
   if (!(hash = _pndman_md5_buf(buffer, strlen(buffer))))
      goto hash_fail;

   request->type = PNDMAN_API_HANDSHAKE;
   snprintf(buffer, sizeof(buffer)-1,
         "stage=2&cnonce=%s&user=%s&hash=%s",cnonce, packet->username, hash);
   curl_easy_setopt(handle->curl, CURLOPT_POSTFIELDS, buffer);

   _pndman_curl_handle_perform(handle);
   return;

hash_fail:
   DEBFAIL(API_HASH_FAIL);
fail:
   IFDO(free, hash);
}

/* \brief api request callback */
static void _pndman_handshake_cb(pndman_curl_code code,
      void *data, const char *info)
{
   pndman_api_request *handle = (pndman_api_request*)data;
   if (code == PNDMAN_CURL_DONE) {
      if (handle->type == PNDMAN_API_NONCE)
         _pndman_handshake_perform(handle);
      else if (handle->type == PNDMAN_API_HANDSHAKE)
         _pndman_handshake_check(handle);
#if 0
      else if (handle->type == PNDMAN_API_RATE)
         _pndman_rate_perform(handle);
      else if (handle->type == PNDMAN_API_COMMENT)
         _pndman_comment_perform(handle);
#endif
   } else {
      DEBUG(PNDMAN_LEVEL_WARN, "handshake request failed");
   }
}

/* \brief create new handshake packet */
pndman_handshake_packet* _pndman_handshake_packet(const char *key, const char *username)
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

/* \brief handshake with repository */
int _pndman_handshake(pndman_curl_handle *handle,
      pndman_repository *repo, const char *key, const char *username)
{
   char url[PNDMAN_URL];
   pndman_api_request *request;

   snprintf(url, PNDMAN_URL-1, "%s/%s", repo->api.root, API_HANDSHAKE);
   DEBUG(PNDMAN_LEVEL_CRAP, url);

   if (!(request = _pndman_api_request_new(PNDMAN_API_NONCE, handle)))
      goto fail;

   if (!(request->packet = _pndman_handshake_packet(key, username)))
      goto fail;

   handle->callback = _pndman_handshake_cb;
   curl_easy_setopt(handle->curl, CURLOPT_POSTFIELDS, "stage=1");
   if (_pndman_curl_handle_perform(handle) != RETURN_OK)
      goto fail;

   return RETURN_OK;

fail:
   IFDO(_pndman_api_request_free, request);
   return RETURN_FAIL;
}

#if 0
/* \brief rate pnd */
int _pndman_rate_pnd(pndman_package *pnd, pndman_repository *list, int rate)
{
   pndman_repository *r;
   pndman_package *p;
   curl_request request;
   char buffer[1024];
   char url[REPO_URL];
   client_api_return status;
   assert(pnd);

   memset(&request, 0, sizeof(curl_request));
   curl_init_request(&request);

   p = NULL;
   for (r = list; r && p != pnd; r = r->next)
      for (p = r->pnd; p && p != pnd; p = p->next) if (p == pnd) break;
   if (!r || p != pnd) goto not_in_repo;

   if (_pndman_handshake(&request, r, _MY_PRIVATE_API_KEY, "Cloudef") != RETURN_OK)
      goto fail;

   snprintf(url, REPO_URL-1, "%s/%s", r->client_api, REPO_API_RATE_URL);
   snprintf(buffer, sizeof(buffer)-1, "id=%s&r=%d", pnd->id, rate);
   curl_easy_setopt(request.curl, CURLOPT_POSTFIELDS, buffer);

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto curl_fail;

   curl_free_request(&request);
   DEBUG(PNDMAN_LEVEL_CRAP, "Rating success!");
   return RETURN_OK;

 not_in_repo:
   DEBFAIL(PND_NOT_IN_REPO);
   goto fail;
curl_fail:
   DEBFAIL(CURL_FAIL);
   goto fail;
fail:
   curl_free_request(&request);
   return RETURN_FAIL;
}

/* \brief comment pnd */
int _pndman_comment_pnd(pndman_package *pnd, pndman_repository *list, const char *comment)
{
   pndman_repository *r;
   pndman_package *p;
   curl_request request;
   char buffer[1024];
   char url[REPO_URL];
   assert(pnd);

   memset(&request, 0, sizeof(curl_request));
   curl_init_request(&request);

   /*
   p = NULL;
   for (r = list; r && p != pnd; r = r->next)
      for (p = r->pnd; p && p != pnd; p = p->next);
   if (!r || p != pnd) goto not_in_repo;
   */

   r = list;
   assert(r);

   if (_pndman_handshake(&request, r, _MY_PRIVATE_API_KEY, "Cloudef") != RETURN_OK)
      goto fail;

   snprintf(url, REPO_URL-1, "%s/%s", r->client_api, REPO_API_COMMENT_URL);
   DEBUG(PNDMAN_LEVEL_CRAP, url);
   snprintf(buffer, sizeof(buffer)-1, "id=%s&c=%s", pnd->id, comment);
   _curl_set(&request, url, buffer);

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto curl_fail;

   curl_free_request(&request);
   DEBUG(PNDMAN_LEVEL_CRAP, "Comment success!");
   return RETURN_OK;

not_in_repo:
   DEBFAIL(PND_NOT_IN_REPO);
   goto fail;
curl_fail:
   DEBFAIL(CURL_FAIL);
   goto fail;
fail:
   curl_free_request(&request);
   return RETURN_FAIL;
}

/* \brief get comments for pnd */
int _pndman_comment_pnd_get(pndman_package *pnd, pndman_repository *list)
{
   pndman_repository *r;
   pndman_package *p;
   curl_request request;
   char buffer[1024];
   char url[REPO_URL];
   assert(pnd);

   memset(&request, 0, sizeof(curl_request));
   curl_init_request(&request);

   /*
   p = NULL;
   for (r = list; r && p != pnd; r = r->next)
      for (p = r->pnd; p && p != pnd; p = p->next);
   if (!r || p != pnd) goto not_in_repo;
   */

   r = list;
   assert(r);

   if (_pndman_handshake(&request, r, _MY_PRIVATE_API_KEY, "Cloudef") != RETURN_OK)
      goto fail;

   snprintf(url, REPO_URL-1, "%s/%s", r->client_api, REPO_API_COMMENT_URL);
   DEBUG(PNDMAN_LEVEL_CRAP, url);
   snprintf(buffer, sizeof(buffer)-1, "id=%s&pull=true", pnd->id);
   _curl_set(&request, url, buffer);

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto curl_fail;

   DEBUG(PNDMAN_LEVEL_CRAP, request.result.data);
   curl_free_request(&request);
   DEBUG(PNDMAN_LEVEL_CRAP, "Comments got!");
   return RETURN_OK;

not_in_repo:
   DEBFAIL(PND_NOT_IN_REPO);
   goto fail;
curl_fail:
   DEBFAIL(CURL_FAIL);
   goto fail;
fail:
   curl_free_request(&request);
   return RETURN_FAIL;
}
#endif
