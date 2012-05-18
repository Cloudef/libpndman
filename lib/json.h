#ifndef PNDMAN_JSON_H
#define PNDMAN_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#define API_TEXT 256

/* \brief status type */
typedef enum _api_return {
   API_SUCCESS,
   API_ERROR
} _api_return;

/* \brief client api return struct */
typedef struct client_api_return {
   _api_return status;
   int number;
   char text[API_TEXT];
} client_api_return;

int _pndman_json_get_value(const char *key, char *value, size_t size, const char *buffer);
int _pndman_json_client_api_return(const char *buffer, client_api_return *status);
int _pndman_json_commit(pndman_repository *r, FILE *f);
int _pndman_json_process(pndman_repository *repo, FILE *data);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_JSON_H */

/* vim: set ts=8 sw=3 tw=0 :*/
