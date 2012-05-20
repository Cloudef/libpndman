#include "internal.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>

#define REPO_API_HANDSHAKE    "handshake"
#define REPO_API_RATE_URL     "rate"
#define REPO_API_COMMENT_URL  "comment"
#define NONCE_LEN 33

static const char *CURL_FAIL           = "Curl perform failed";
static const char *HASH_FAIL           = "Hashing failed";
static const char *COULD_NOT_GEN_NONCE = "Could not generate nonce data";
static const char *API_FAIL      = "%d: %s";
static const char *PND_NOT_IN_REPO     = "Pnd was not found in repository list";

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
   if (md5) free(md5);
   DEBFAIL(COULD_NOT_GEN_NONCE);
   return RETURN_OK;
}

/* \brief set curl opts */
static void _curl_set(curl_request *request, const char *url, const char *post)
{
   curl_easy_setopt(request->curl, CURLOPT_URL, url);
   curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, request);
   curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, curl_write_request);
   curl_easy_setopt(request->curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(request->curl, CURLOPT_POSTFIELDS, post);
}

/* \brief handshake with repository */
int _pndman_handshake(curl_request *request, pndman_repository *r, const char *api_key, const char *user)
{
   char cnonce[NONCE_LEN];
   char buffer[1024], *hash = NULL;
   char url[REPO_URL];
   char nonce[1024];
   client_api_return status;

   /* url to api */
   snprintf(url, REPO_URL-1, "%s/%s", r->client_api, REPO_API_HANDSHAKE);
   DEBUG(3, url);

   _curl_set(request, url, "stage=1");
   if (curl_easy_perform(request->curl) != CURLE_OK)
      goto curl_fail;

   if (_pndman_gen_nonce(cnonce) != RETURN_OK)
      goto fail;

   if (_pndman_json_get_value("nonce", nonce, sizeof(nonce),
         request->result.data) != RETURN_OK)
      goto fail;

   DEBUG(3, "%s", nonce);
   sprintf(buffer, "%s%s%s", nonce, cnonce, api_key);
   if (!(hash = _pndman_md5_buf(buffer, strlen(buffer))))
      goto hash_fail;

   snprintf(buffer, sizeof(buffer)-1, "stage=2&cnonce=%s&user=%s&hash=%s", cnonce, user, hash);
   free(hash);
   hash = NULL;

   if (curl_init_request(request) != RETURN_OK)
      goto fail;

   _curl_set(request, url, buffer);
   if (curl_easy_perform(request->curl) != CURLE_OK)
      goto curl_fail;

   DEBUG(3, "%s", request->result.data);
   if (_pndman_json_client_api_return(request->result.data,
               &status) != RETURN_OK)
      goto api_fail;

   DEBUG(3, "Handshake success!\n");
   return RETURN_OK;

hash_fail:
   DEBFAIL(HASH_FAIL);
   goto fail;
curl_fail:
   DEBFAIL(CURL_FAIL);
   goto fail;
api_fail:
   DEBFAIL(API_FAIL, status.number, status.text);
fail:
   if (hash) free(hash);
   return RETURN_FAIL;
}

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
   DEBUG(3, "Rating success!\n");
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
   DEBUG(3, url);
   snprintf(buffer, sizeof(buffer)-1, "id=%s&c=%s", pnd->id, comment);
   _curl_set(&request, url, buffer);

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto curl_fail;

   curl_free_request(&request);
   DEBUG(3, "Comment success!\n");
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
   DEBUG(3, url);
   snprintf(buffer, sizeof(buffer)-1, "id=%s&pull=true", pnd->id);
   _curl_set(&request, url, buffer);

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto curl_fail;

   DEBUG(3, request.result.data);
   curl_free_request(&request);
   DEBUG(3, "Comments got!\n");
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
