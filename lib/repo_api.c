#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <curl/curl.h>
#include "pndman.h"
#include "md5.h"
#include "curl.h"

static const char *CURL_FAIL           = "Curl perform failed";
static const char *HASH_FAIL           = "Hashing failed";
static const char *COULD_NOT_GEN_NONCE = "Could not generate nonce data";
static const char *HANDSHAKE_FAIL      = "Handshaking failed";

#define NONCE_LEN 33

/* \brief generate nonce */
int _pndman_gen_nonce(char *buffer)
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

/* \brief handshake with repository */
int _pndman_handshake(curl_request *request, const char *login_url, const char *api_key, const char *user)
{
   char cnonce[NONCE_LEN];
   char buffer[1024], *hash = NULL;

   curl_easy_setopt(request->curl, CURLOPT_URL, login_url);
   curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, request);
   curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, curl_write_request);
   curl_easy_setopt(request->curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(request->curl, CURLOPT_POSTFIELDS, "stage=1");

   if (curl_easy_perform(request->curl) != CURLE_OK)
      goto curl_fail;

   if (_pndman_gen_nonce(cnonce) != RETURN_OK)
      goto fail;

   sprintf(buffer, "%s%s%s", (char*)request->result.data, cnonce, api_key);
   if (!(hash = _pndman_md5_buf(buffer, strlen(buffer))))
      goto hash_fail;

   sprintf(buffer, "stage=2&cnonce=%s&user=%s&hash=%s", cnonce, user, hash);
   free(hash);
   hash = NULL;

   if (curl_init_request(request) != RETURN_OK)
      goto fail;

   curl_easy_setopt(request->curl, CURLOPT_URL, login_url);
   curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, &request);
   curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, curl_write_request);
   curl_easy_setopt(request->curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(request->curl, CURLOPT_POSTFIELDS, buffer);
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
