#ifndef PNDMAN_CURL_H
#define PNDMAN_CURL_H

#define MAX_REQUEST 1024 * 1024
#define CURL_TIMEOUT 15L

#ifdef __cplusplus
extern "C" {
#endif

/* \brief curl_write_result struct for storing curl result to memory */
typedef struct curl_write_result
{
   char  data[MAX_REQUEST];
   int   pos;
} curl_write_result;

/* \brief struct for holding internal request data */
typedef struct curl_request
{
   curl_write_result result;
   CURL              *curl;
} curl_request;

/* \brief write to file */
size_t curl_write_file(void *data, size_t size, size_t nmemb, FILE *file);

/* \brief progressbar */
int curl_progress_func(void* ptr, double TotalToDownload, double NowDownloaded, double TotalToUpload, double NowUploaded);

/* \brief write response */
size_t curl_write_request(void *data, size_t size, size_t nmemb, curl_request *request);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_CURL_H */

/* vim: set ts=8 sw=3 tw=0 :*/
