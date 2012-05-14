#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>
#include "pndman.h"
#include "package.h"
#include "repository.h"
#include "md5.h"
#include "curl.h"

#define REPO_API_HANDSHAKE    "handshake"
#define REPO_API_RATE_URL     "rate"
#define REPO_API_COMMENT_URL  "comment"
#define NONCE_LEN 33

static const char *CURL_FAIL           = "Curl perform failed";
static const char *HASH_FAIL           = "Hashing failed";
static const char *COULD_NOT_GEN_NONCE = "Could not generate nonce data";
static const char *HANDSHAKE_FAIL      = "Handshaking failed";
static const char *RATE_FAIL           = "Rating failed";
static const char *COMMENT_FAIL        = "Commenting failed";
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

   /* url to api */
   snprintf(url, REPO_URL-1, "%s/%s", r->client_api, REPO_API_HANDSHAKE);

   _curl_set(request, url, "stage=1");
   if (curl_easy_perform(request->curl) != CURLE_OK)
      goto curl_fail;

   if (_pndman_gen_nonce(cnonce) != RETURN_OK)
      goto fail;

   sprintf(buffer, "%s%s%s", (char*)request->result.data, cnonce, api_key);
   if (!(hash = _pndman_md5_buf(buffer, strlen(buffer))))
      goto hash_fail;

   snprintf(buffer, 1023, "stage=2&cnonce=%s&user=%s&hash=%s", cnonce, user, hash);
   free(hash);
   hash = NULL;

   if (curl_init_request(request) != RETURN_OK)
      goto fail;

   _curl_set(request, url, buffer);
   if (curl_easy_perform(request->curl) != CURLE_OK)
      goto curl_fail;

   if (strncmp((char*)request->result.data, "SUCCESS!", strlen("SUCCESS!")))
      goto handshake_fail;

   DEBUG(3, "Handshake success!\n");
   return RETURN_OK;

hash_fail:
   DEBFAIL(HASH_FAIL);
   goto fail;
curl_fail:
   DEBFAIL(CURL_FAIL);
   goto fail;
handshake_fail:
   DEBFAIL(HANDSHAKE_FAIL);
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
   assert(pnd);

   p = NULL;
   for (r = list; r && p != pnd; r = r->next)
      for (p = r->pnd; p && p != pnd; p = p->next);
   if (!r || p != pnd) goto not_in_repo;

   curl_init_request(&request);
   if (_pndman_handshake(&request, r, _MY_PRIVATE_API_KEY, "Cloudef") != RETURN_OK)
      goto fail;

   snprintf(url, REPO_URL-1, "%s/%s", r->client_api, REPO_API_RATE_URL);
   snprintf(buffer, 1023, "id=%s&r=%d", pnd->id, rate);
   curl_easy_setopt(request.curl, CURLOPT_POSTFIELDS, buffer);

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto curl_fail;

   if (strncmp((char*)request.result.data, "SUCCESS!", strlen("SUCCESS!")))
      goto rate_fail;

   curl_free_request(&request);
   DEBUG(3, "Rating success!\n");
   return RETURN_OK;

 not_in_repo:
   DEBFAIL(PND_NOT_IN_REPO);
   goto fail;
curl_fail:
   DEBFAIL(CURL_FAIL);
   goto fail;
rate_fail:
   DEBFAIL(RATE_FAIL);
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

   p = NULL;
   for (r = list; r && p != pnd; r = r->next)
      for (p = r->pnd; p && p != pnd; p = p->next);
   if (!r || p != pnd) goto not_in_repo;

   curl_init_request(&request);
   if (_pndman_handshake(&request, r, _MY_PRIVATE_API_KEY, "Cloudef") != RETURN_OK)
      goto fail;

   snprintf(url, REPO_URL-1, "%s/%s", r->client_api, REPO_API_COMMENT_URL);
   snprintf(buffer, 1023, "id=%s&c=%s", pnd->id, comment);
   _curl_set(&request, url, buffer);

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto curl_fail;

   if (strncmp((char*)request.result.data, "SUCCESS!", strlen("SUCCESS!")))
      goto rate_fail;

   curl_free_request(&request);
   DEBUG(3, "Rating success!\n");
   return RETURN_OK;

not_in_repo:
   DEBFAIL(PND_NOT_IN_REPO);
   goto fail;
curl_fail:
   DEBFAIL(CURL_FAIL);
   goto fail;
rate_fail:
   DEBFAIL(COMMENT_FAIL);
fail:
   curl_free_request(&request);
   return RETURN_FAIL;
}
