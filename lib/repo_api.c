#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <curl/curl.h>
#include "pndman.h"
#include "md5.h"
#include "curl.h"

static const char *COULD_NOT_GEN_NONCE = "Could not generate nonce data";
static const char *HANDSHAKE_FAIL = "Handshaking failed";

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
int _pndman_handshake(const char *login_url, const char *api_key, const char *user)
{
   curl_request request;
   char cnonce[NONCE_LEN];
   char buffer[1024], *hash = NULL;

   memset(&request, 0, sizeof(curl_request));
   memset(&request.result, 0, sizeof(curl_write_result));

   if (curl_init_request(&request) != RETURN_OK)
      goto fail;

   curl_easy_setopt(request.curl, CURLOPT_URL, login_url);
   curl_easy_setopt(request.curl, CURLOPT_WRITEDATA, &request);
   curl_easy_setopt(request.curl, CURLOPT_WRITEFUNCTION, curl_write_request);
   curl_easy_setopt(request.curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(request.curl, CURLOPT_POSTFIELDS, "stage=1");

   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto fail;

   if (_pndman_gen_nonce(cnonce) != RETURN_OK)
      goto fail;

   sprintf(buffer, "%s%s%s", request.result.data, cnonce, api_key);
   if (!(hash = _pndman_md5_buf(buffer, strlen(buffer))))
      goto fail;

   sprintf(buffer, "stage=2&cnonce=%s&user=%s&hash=%s", cnonce, user, hash);
   free(hash);

   if (curl_init_request(&request) != RETURN_OK)
      goto fail;

   curl_easy_setopt(request.curl, CURLOPT_URL, login_url);
   curl_easy_setopt(request.curl, CURLOPT_COOKIEFILE, "");
   curl_easy_setopt(request.curl, CURLOPT_POSTFIELDS, buffer);
   if (curl_easy_perform(request.curl) != CURLE_OK)
      goto fail;

   curl_free_request(&request);
   return RETURN_OK;

fail:
   curl_free_request(&request);
   if (hash) free(hash);
   DEBFAIL(HANDSHAKE_FAIL);
   return RETURN_FAIL;
}
