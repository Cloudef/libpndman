#ifndef PNDMAN_CURL_H
#define PNDMAN_CURL_H

/* \brief curl_write_result struct for storing curl result to memory */
typedef struct curl_write_result
{
   char *data;
   int   pos;
} curl_write_result;

/* \brief write to file */
size_t curl_write_file(void *data, size_t size, size_t nmemb, FILE *file);

/* \brief progressbar */
int curl_progress_func(void* ptr, double TotalToDownload, double NowDownloaded, double TotalToUpload, double NowUploaded);

/* \brief write response */
size_t curl_write_response(void *data, size_t size, size_t nmemb, curl_write_result *result);

/* \brief Init curl's multi interface
 * NOTE: Does checkup if it's already initialized */
int _pndman_curl_init(void);

/* \brief Free curl's multi interface */
int _pndman_curl_free(void);

/* \brief Add easy handle to multi handle */
int _pndman_curl_add_handle(CURL *curl);

/* \brief Remove easy handle from multi handle */
int _pndman_curl_remove_handle(CURL *curl);

/* \brief Internal perform of curl's multi handle */
int _pndman_curl_perform(int *still_running);

/* \brief Internal timeout of curl's multi handle */
int _pndman_curl_timeout(long *timeout);

/* \brief Internal fdset of curl's multi handle */
int _pndman_curl_fdset(fd_set *read, fd_set *write, fd_set *expect, int *max);

/* \brief Internal fdset of curl's multi handle */
CURLMsg* _pndman_curl_info_read(int *msgs_left);

#endif /* PNDMAN_CURL_H */

/* vim: set ts=8 sw=3 tw=0 :*/
