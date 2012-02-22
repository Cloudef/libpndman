#ifndef PNDMAN_CURL_H
#define PNDMAN_CURL_H

#define MAX_REQUEST  2048
#define CURL_TIMEOUT 15L

#ifdef __cplusplus
extern "C" {
#endif

/* \brief curl_write_result struct for storing curl result to memory */
typedef struct curl_write_result
{
   void *data;
   int   pos;
} curl_write_result;

/* \brief struct for holding progression data of curl handle */
typedef struct curl_progress
{
   double download;
   double total_to_download;
   char done;
} curl_progress;

/* \brief struct for holding internal request data */
typedef struct curl_request
{
   curl_write_result result;
   CURL              *curl;
} curl_request;

/* \brief write to file */
size_t curl_write_file(void *data, size_t size, size_t nmemb, FILE *file);

/* \brief progressbar */
int curl_progress_func(curl_progress *ptr, double total_to_download, double download, double total_to_upload, double upload);

/* \brief write response */
size_t curl_write_request(void *data, size_t size, size_t nmemb, curl_request *request);

int curl_init_request(curl_request *request);
void curl_free_request(curl_request *request);

void curl_init_progress(curl_progress *progress);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_CURL_H */

/* vim: set ts=8 sw=3 tw=0 :*/
