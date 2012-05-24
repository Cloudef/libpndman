#ifndef _internal_h_
#define _internal_h_

/* public api */
#include "../include/pndman.h"
#include <string.h>     /* for strrchr */
#include <curl/curl.h>  /* for CURL */

/* internal sizes */
#define PNDMAN_API_COMMENT_LEN 300

/* constants */
#define PNDMAN_APPDATA        "libpndman"
#define PNDMAN_LOCAL_DB       "libpndman repository"
#define PNDMAN_DEFAULT_ICON   "icon.png"
#define PNDMAN_TRUE           "true"
#define PNDMAN_FALSE          "false"
#define PND_TYPE_RELEASE      "release"
#define PND_TYPE_BETA         "beta"
#define PND_TYPE_ALPHA        "alpha"
#define PND_X11_REQ           "req"
#define PND_X11_STOP          "stop"
#define PND_X11_IGNORE        "ignore"

/* helper macros */
#define IFDO(f, x)            if (x) f(x); x = NULL;
#define THIS_FILE             ((strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)
#define DBG_FMT               "\2@FILE \5%-20s \2@LINE \5%-4d \5>> \4%s\n\5%s"
#define DBG_WRN_BAD_USE       "\2-!- Bad usage of function:\5 "
#define BADUSE(fmt,...)       _pndman_debug_hook(THIS_FILE, __LINE__, __func__, \
                                       PNDMAN_LEVEL_WARN, DBG_WRN_BAD_USE""fmt, ##__VA_ARGS__)
#define DEBUG(level,fmt,...)  _pndman_debug_hook(THIS_FILE, __LINE__, __func__, \
                                       level, fmt, ##__VA_ARGS__)
#define DEBFAIL(fmt,...)      _pndman_debug_hook(THIS_FILE, __LINE__, __func__, \
                                       PNDMAN_LEVEL_ERROR, fmt, ##__VA_ARGS__)

/* etc.. */
#define PNDMAN_CURL_TIMEOUT 15L
#define PNDMAN_CURL_CHUNK   1024

/* strings */
#define DATABASE_URL_COPY_FAIL   "Failed to copy url from repository."
#define DATABASE_BAD_URL         "Repository has empty url, or it is a local repository."
#define DATABASE_LOCK_TIMEOUT    "%s blocking for IO operation timed out."
#define WRITE_FAIL               "Failed to open %s, for writing."
#define READ_FAIL                "Failed to open %s, for reading."
#define ACCESS_FAIL              "Can't access: %s"
#define CURL_CURLM_FAIL          "Internal CURLM initialization failed."
#define CURL_HANDLE_FAIL         "Failed to allocate CURL handle."
#define CURL_REQUEST_FAIL        "Failed to init curl request."
#define DEVICE_IS_NOT_DIR        "%s, is not a directory."
#define DEVICE_ACCESS_FAIL       "%s, should have write and read permissions."
#define DEVICE_ROOT_FAIL         "Could not get root device of %s absolute directory."
#define DEVICE_EXISTS            "Device with mount %s, already exists."
#define HANDLE_MD5_FAIL          "MD5 check fail for %s"
#define HANDLE_MD5_DIFF          "MD5 differ, but forcing anyways."
#define HANDLE_MV_FAIL           "Failed to move from %s, to %s"
#define HANDLE_RM_FAIL           "Failed to remove file %s"
#define HANDLE_BACKUP_DIR_FAIL   "Failed to create backup directory to device: %s"
#define HANDLE_NO_PND            "Handle has no PND!"
#define HANDLE_PND_URL           "PND assigned to handle has invalid url."
#define HANDLE_NO_DEV            "Handle has no device! (install)"
#define HANDLE_NO_DEV_UP         "Handle has no device list! (update)"
#define HANDLE_NO_DST            "Handle has no destination, nor the PND has upgrade."
#define HANDLE_WTF               "WTF. Something that should never happen, just happened!"
#define HANDLE_HEADER_FAIL       "Failed to parse filename from HTTP header."
#define JSON_BAD_JSON            "Bad json data, won't process sync for: %s"
#define JSON_NO_P_ARRAY          "No packages array for: %s"
#define JSON_NO_R_HEADER         "No repo header for: %s"
#define PXML_XML_CPY_FAIL        "PXML is too big for pndman :("
#define PXML_START_TAG_FAIL      "PXML parse failed: could not find start tag before EOF."
#define PXML_END_TAG_FAIL        "PXML parse failed: could not find end tag before EOF."
#define PXML_EXPAT_FAIL          "Failed to allocate expat XML parser"
#define PXML_INVALID_XML         "PXML: %s\n-----\n%s\n-----\n"
#define PNDMAN_ALLOC_FAIL        "Failed to allocate %s, shit might break now."
#define PNDMAN_TMP_FILE_FAIL     "Failed to get temporary file."
#define API_HASH_FAIL            "Hashing failed."
#define API_NONCE_FAIL           "Could not generate nonce data."
#define API_FAIL                 "%d: %s"
#define API_PND_NOT_IN_REPO      "PND %s, was not found in repository list."

/*! \brief
 * Default return values.
 * Expections will be test functions where true condition returns 1, and false condition 0 */
typedef enum PNDMAN_RETURN
{
   RETURN_FAIL  = -1,
   RETURN_OK    =  0,

   RETURN_TRUE  =  1,
   RETURN_FALSE =  !RETURN_TRUE
} PNDMAN_RETURN;

/* \brief callback function definition to curl handle */
typedef void (*pndman_curl_callback)(pndman_curl_code code, void *data, const char *info);

/* \brief curl header presention */
typedef struct pndman_curl_header
{
   void *data;
   size_t pos, size;
} pndman_curl_header;

/* \brief internal curl handle */
typedef struct pndman_curl_handle
{
   void *data;
   pndman_curl_callback callback;
   pndman_curl_progress *progress;
   CURL *curl;
   FILE *file;
   pndman_curl_header header;
   char path[PNDMAN_PATH];
} pndman_curl_handle;

/* \brief client api return error code */
typedef enum pndman_api_code {
   API_SUCCESS,
   API_ERROR
} pndman_api_code;

/* \brief struct representing the return status
 * of common repo client api functions */
typedef struct pndman_api_status {
   pndman_api_code status;
   int number;
   char text[PNDMAN_SHRT_STR];
} pndman_api_status;

/* \brief client api comment struct */
typedef struct pndman_api_comment {
   time_t   date;
   char     user[PNDMAN_SHRT_STR];
   char     comment[PNDMAN_API_COMMENT_LEN];
   struct   pndman_api_comment *next;
}pndmann_api_comment;

/* internal string utils */
void  _strip_slash(char *path);
char* _strupstr(const char *hay, const char *needle);
int   _strupcmp(const char *hay, const char *needle);
int   _strnupcmp(const char *hay, const char *needle, size_t len);

/* temporary files */
FILE* _pndman_get_tmp_file();

/* errors && verbose */
void _pndman_set_error(const char *file, int line,
      const char *function, const char *err, ...);
void _pndman_debug_hook(const char *file, int line,
      const char *function, int verbose_level, const char *fmt, ...);

/* curl */
pndman_curl_handle* _pndman_curl_handle_new(void *data,
      pndman_curl_progress *progress, pndman_curl_callback callback,
      const char *path);
void _pndman_curl_handle_free(pndman_curl_handle *handle);
int  _pndman_curl_handle_perform(pndman_curl_handle *handle);

/* json */
int _pndman_json_api_value(const char *key, char *value, size_t size,
      const char *buffer);
int _pndman_json_api_status(const char *buffer, pndman_api_status *status);
int _pndman_json_commit(pndman_repository *repo, FILE *f);
int _pndman_json_process(pndman_repository *repo, FILE *f);
int _pndman_json_client_api_return(FILE *file, pndman_api_status *status);
int _pndman_json_get_value(const char *key, char *value,
      size_t size, FILE *file);

/* md5 functions (remember free result) */
char* _pndman_md5_buf(char *buffer, size_t size);
char* _pndman_md5(const char *file);

/* devices */
pndman_device* _pndman_device_first(pndman_device *device);
pndman_device* _pndman_device_last(pndman_device *device);
char* _pndman_device_get_appdata(pndman_device *device);
void  _pndman_device_get_appdata_no_create(char *appdata,
      pndman_device *device);

/* repositories */
pndman_repository* _pndman_repository_first(pndman_repository *repo);
pndman_repository* _pndman_repository_last(pndman_repository *repo);
pndman_repository* _pndman_repository_get(const char *url,
      pndman_repository *list);
pndman_package* _pndman_repository_new_pnd(pndman_repository *repo);
pndman_package* _pndman_repository_new_pnd_check(char *id,
      char *path, pndman_version *ver, pndman_repository *repo);
int _pndman_repository_free_pnd(pndman_package *pnd, pndman_repository *repo);

/* pndman_package  */
pndman_package* _pndman_new_pnd(void);
int  _pndman_vercmp(pndman_version *lp, pndman_version *rp);
void _pndman_package_free_titles(pndman_package *pnd);
void _pndman_package_free_descriptions(pndman_package *pnd);
void _pndman_package_free_previewpics(pndman_package *pnd);
void _pndman_package_free_licenses(pndman_package *pnd);
void _pndman_package_free_categories(pndman_package *pnd);
void _pndman_package_free_applications(pndman_package *pnd);
pndman_translated*   _pndman_package_new_title(pndman_package *pnd);
pndman_translated*   _pndman_package_new_description(pndman_package *pnd);
pndman_license*      _pndman_package_new_license(pndman_package *pnd);
pndman_previewpic*   _pndman_package_new_previewpic(pndman_package *pnd);
pndman_category*     _pndman_package_new_category(pndman_package *pnd);
pndman_application*  _pndman_package_new_application(pndman_package *pnd);
pndman_translated*   _pndman_application_new_title(pndman_application *app);
pndman_translated*   _pndman_application_new_description(pndman_application *app);
pndman_license*      _pndman_application_new_license(pndman_application *app);
pndman_previewpic*   _pndman_application_new_previewpic(pndman_application *app);
pndman_association*  _pndman_application_new_association(pndman_application *app);
pndman_category*     _pndman_application_new_category(pndman_application *app);

/* \brief copy pndman_package, doesn't copy next && next_installed pointers */
int _pndman_copy_pnd(pndman_package *pnd, pndman_package *src);

/* \brief used by PXML parser */
void _pndman_application_free_titles(pndman_application *app);

/* \brief used by PXML parser */
void _pndman_application_free_descriptions(pndman_application *app);

/* \brief internal free of pndman_package, return next_installed, null if no any */
pndman_package* _pndman_free_pnd(pndman_package *pnd);

#endif /* _internal_h_ */

/* vim: set ts=8 sw=3 tw=0 :*/
