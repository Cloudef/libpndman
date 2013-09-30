#ifndef _PNDMAN_H_
#define _PNDMAN_H_

#if !defined(_WIN32) && (defined(__WIN32__) || defined(WIN32) || defined(__CYGWIN__))
#  define _WIN32
#endif /* _WIN32 */

#if defined(_WIN32) && defined(_PNDMAN_BUILD_DLL)
/* we are building a Win32 DLL */
#  define PNDMANAPI __declspec(dllexport)
#elif defined(_WIN32) && defined(PNDMAN_DLL)
/* we are calling a Win32 DLL */
#  if defined(__LCC__)
#     define PNDMANAPI extern
#  else
#     define PNDMANAPI __declspec(dllimport)
#  endif
#else
/* we are either building/calling a static lib or we are non-win32 */
#  define PNDMANAPI
#endif

#include <time.h> /* for time_t */
#ifndef _WIN32
#  include <limits.h> /* for LINE_MAX, PATH_MAX */
#endif

/* currently only mingw */
#ifdef _WIN32
#  include <windows.h> /* for malware */
#  include <limits.h> /* for PATH_MAX */
#  define LINE_MAX 2048
#  ifdef _UNICODE
#     error "Suck it unicde.."
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* \brief debug level enum */
typedef enum pndman_debug_level {
   PNDMAN_LEVEL_ERROR,
   PNDMAN_LEVEL_WARN,
   PNDMAN_LEVEL_CRAP
} pndman_debug_level;

/* \brief curl callback return code */
typedef enum pndman_curl_code
{
   PNDMAN_CURL_DONE,
   PNDMAN_CURL_FAIL,
   PNDMAN_CURL_PROGRESS,
} pndman_curl_code;

/* \brief debug hook typedef */
typedef void (*PNDMAN_DEBUG_HOOK_FUNC)(
      const char *file, int line, const char *function,
      int verbose_level, const char *str);

/* \brief flags for sync handle */
typedef enum pndman_sync_handle_flags
{
   PNDMAN_SYNC_FULL = 0x001,
} pndman_sync_handle_flags;

/* \brief flags for handles */
typedef enum pndman_package_handle_flags
{
   PNDMAN_PACKAGE_INSTALL         = 0x001,
   PNDMAN_PACKAGE_REMOVE          = 0x002,
   PNDMAN_PACKAGE_FORCE           = 0x004,
   PNDMAN_PACKAGE_INSTALL_DESKTOP = 0x008,
   PNDMAN_PACKAGE_INSTALL_MENU    = 0x010,
   PNDMAN_PACKAGE_INSTALL_APPS    = 0x020,
   PNDMAN_PACKAGE_BACKUP          = 0x040,
   PNDMAN_PACKAGE_LOG_HISTORY     = 0x080,
} pndman_package_handle_flags;

/* \brief type enum for version struct */
typedef enum pndman_version_type
{
   PND_VERSION_RELEASE,
   PND_VERSION_BETA,
   PND_VERSION_ALPHA
} pndman_version_type;

/* \brief x11 enum for exec struct */
typedef enum pndman_exec_x11
{
   PND_EXEC_REQ,
   PND_EXEC_STOP,
   PND_EXEC_IGNORE
} pndman_exec_x11;

/* \brief struct holding version information */
typedef struct pndman_version
{
   char *major;
   char *minor;
   char *release;
   char *build;
   pndman_version_type type;
} pndman_version;

/* \brief struct holding execution information */
typedef struct pndman_exec
{
   int   background;
   char *startdir;
   int   standalone;
   char *command;
   char *arguments;
   pndman_exec_x11 x11;
} pndman_exec;

/* \brief struct holding author information */
typedef struct pndman_author
{
   char *name;
   char *website;
   char *email;
} pndman_author;

/* \brief struct holding documentation information */
typedef struct pndman_info
{
   char *name;
   char *type;
   char *src;
} pndman_info;

/* \brief struct holding translated strings */
typedef struct pndman_translated
{
   char *lang;
   char *string;
   struct pndman_translated *next;
} pndman_translated;

/* \brief struct holding license information */
typedef struct pndman_license
{
   char *name;
   char *url;
   char *sourcecodeurl;
   struct pndman_license *next;
} pndman_license;

/* \brief struct holding previewpic information */
typedef struct pndman_previewpic
{
   char *src;
   struct pndman_previewpic *next;
} pndman_previewpic;

/* \brief struct holding association information */
typedef struct pndman_association
{
   char *name;
   char *filetype;
   char *exec;
   struct pndman_association *next;
} pndman_association;

/* \brief struct holding category information */
typedef struct pndman_category
{
   char *main;
   char *sub;
   struct pndman_category *next;
} pndman_category;

/* \brief struct that represents PND application */
typedef struct pndman_application
{
   char *id;
   char *appdata;
   char *icon;
   int  frequency;
   pndman_author  author;
   pndman_version osversion;
   pndman_version version;
   pndman_exec    exec;
   pndman_info    info;
   pndman_translated    *title;
   pndman_translated    *description;
   pndman_license       *license;
   pndman_previewpic    *previewpic;
   pndman_category      *category;
   pndman_association   *association;
   struct pndman_application *next;
} pndman_application;

/* \brief struct that represents PND */
typedef struct pndman_package
{
   char *path;
   char *id;
   char *icon;
   char *info;
   char *md5;
   char *url;
   char *vendor;
   size_t size;
   time_t modified_time;
   time_t local_modified_time;
   int rating;
   pndman_author     author;
   pndman_version    version;
   pndman_application *app;
   pndman_translated *title;
   pndman_translated *description;
   pndman_license    *license;
   pndman_previewpic *previewpic;
   pndman_category   *category;
   char *repository;
   char *mount;
   struct pndman_package *update;
   struct pndman_package *next_installed;
   struct pndman_package *next;
   int commercial;
   struct pndman_repository *repositoryptr;
} pndman_package;

/*! \brief struct representing client api access */
typedef struct pndman_repository_api
{
   char *root;
   char *username;
   char *key;
   char store_credentials;
} pndman_repository_api;

/*! \brief struct representing repository */
typedef struct pndman_repository
{
   char *url;
   char *name;
   char *updates;
   char *version;
   char commited;
   time_t timestamp;
   pndman_package *pnd;
   pndman_repository_api api;
   struct pndman_repository *next, *prev;
} pndman_repository;

/*! \brief struct representing device */
typedef struct pndman_device
{
   char *mount;
   char *device;
   size_t size, free, available;
   char *appdata;
   struct pndman_device *next, *prev;
} pndman_device;

/* \brief struct for holding progression data of
 * curl handle */
typedef struct pndman_curl_progress
{
   double download;
   double total_to_download;
   char done;
} pndman_curl_progress;

/* forward declarations */
struct pndman_package_handle;
struct pndman_sync_handle;

/* \brief callback function definition to sync handle */
typedef void (*pndman_package_handle_callback)(
      pndman_curl_code code, struct pndman_package_handle *handle);
typedef void (*pndman_sync_handle_callback)(
      pndman_curl_code code, struct pndman_sync_handle *handle);

/*! \brief Struct for PND transaction */
typedef struct pndman_package_handle
{
   char *name;
   char *error;
   pndman_package *pnd;
   pndman_device  *device;
   unsigned int flags;
   pndman_curl_progress progress;
   pndman_package_handle_callback callback;

   /* assign your own data here */
   void *user_data;

   /* internal request data,
    * you don't want to touch this. */
   void *data;
} pndman_package_handle;

/* \brief struct for repository synchorization */
typedef struct pndman_sync_handle
{
   char *error;
   pndman_repository *repository;
   unsigned int flags;
   pndman_curl_progress progress;
   pndman_sync_handle_callback callback;

   /* assign your own data here */
   void *user_data;

   /* internal request data,
    * you don't want to touch this. */
   void *data;
} pndman_sync_handle;

/* forward declarations */
struct pndman_api_comment_packet;
struct pndman_api_history_packet;
struct pndman_api_archived_packet;
struct pndman_api_rate_packet;

/* \brief generic callback for repo api access */
typedef void (*pndman_api_generic_callback)(
      pndman_curl_code code, const char *info, void *user_data);

/* \brief callback for comment pull */
typedef void (*pndman_api_comment_callback)(
      pndman_curl_code code, struct pndman_api_comment_packet *packet);

/* \brief callback for download history */
typedef void (*pndman_api_history_callback)(
      pndman_curl_code code, struct pndman_api_history_packet *packet);

/* \brief callback for archived pnd's */
typedef void (*pndman_api_archived_callback)(
      pndman_curl_code code, struct pndman_api_archived_packet *packet);

/* \brief callback for rate calls */
typedef void (*pndman_api_rate_callback)(
      pndman_curl_code code, struct pndman_api_rate_packet *packet);

/* \brief comment pull callback packet */
typedef struct pndman_api_comment_packet
{
   char *error;
   pndman_package *pnd;
   pndman_version *version;
   time_t date;
   const char *username;
   const char *comment;
   int is_last;

   /* your data returned */
   void *user_data;
} pndman_api_comment_packet;

/* \brief download history callback packet */
typedef struct pndman_api_history_packet
{
   char *error;
   const char *id;
   pndman_version *version;
   time_t download_date;
   int is_last;

   /* your data returned */
   void *user_data;
} pndman_api_history_packet;

/* \brief archived callback packet */
typedef struct pndman_api_archived_packet
{
   char *error;
   pndman_package *pnd;

   /* your data returned */
   void *user_data;
} pndman_api_archived_packet;

/* \brief rating callback packet */
typedef struct pndman_api_rate_packet
{
   char *error;
   pndman_package *pnd;
   int rating;       /* your rating */
   int total_rating; /* new rating on repo */

   /* your data returned */
   void *user_data;
} pndman_api_rate_packet;

/* \brief get git head */
PNDMANAPI const char* pndman_git_head(void);

/* \brief get git commit description */
PNDMANAPI const char* pndman_git_commit(void);

/* \brief set verbose level of pndman
 * (prints to stdout, if no hook set) */
PNDMANAPI void pndman_set_verbose(int level);

/* \brief set debug hook function */
PNDMANAPI void pndman_set_debug_hook(
      PNDMAN_DEBUG_HOOK_FUNC func);

/* \brief get current verbose level */
PNDMANAPI int pndman_get_verbose(void);

/* \brief set internal curl timeout for libpndman */
PNDMANAPI void pndman_set_curl_timeout(int timeout);

/* \brief get internal curl timeout for libpndman */
PNDMANAPI int pndman_get_curl_timeout(void);

/* \brief colored put function
 * this is manily provided public to milkyhelper,
 * to avoid some code duplication.
 *
 * Plus pndman uses it internally, if no debug hooks set :)
 * Feel free to use it as well if you want. */
PNDMANAPI void pndman_puts(const char *buffer);

/* \brief use this to disable internal color output */
PNDMANAPI void pndman_set_color(int use_color);

/* \brief initialize repository list
 * on success: returns list object containing
 * local repository
 * on failure: returns NULL */
PNDMANAPI pndman_repository* pndman_repository_init(void);

/* \brief add new repository to the list
 * on success: returns added repository
 * on failure: returns NULL */
PNDMANAPI pndman_repository* pndman_repository_add(
      const char *url, pndman_repository *list);

/* \brief clear all PND's from repository */
PNDMANAPI void pndman_repository_clear(
      pndman_repository *repo);

/* \brief check local repository for invalid PND's,
 * such PND's are ones that don't exist anymore
 * on the filesystem.
 *
 * returns the number of packages removed */
PNDMANAPI int pndman_repository_check_local(
      pndman_repository *local);

/* \brief free a repository (and remove from list)
 * returns the first item on list */
PNDMANAPI pndman_repository* pndman_repository_free(
      pndman_repository *repo);

/* \brief free all repositories
 * the list will be invalid after this */
PNDMANAPI void pndman_repository_free_all(
      pndman_repository *repo);

/* \brief commit all repositories to specific device */
PNDMANAPI int pndman_repository_commit_all(
      pndman_repository *list, pndman_device *device);

/* \brief check PND updates for all repositories
 * in the repository list
 * returns number of updates found */
PNDMANAPI int pndman_repository_check_updates(
      pndman_repository *list);

/* \brief set credentials for repository
 * and decide, if to store them locally or not */
PNDMANAPI void pndman_repository_set_credentials(pndman_repository *repository,
      const char *username, const char *key, int store_credentials);

/* \brief add new device or initalize new device list
 * on success: returns pointer to the new device
 * on failure: returns NULL */
PNDMANAPI pndman_device* pndman_device_add(
      const char *path, pndman_device *list);

/* \brief detect new devices and add them to list
 * on success: returns pointer to the first found device
 * on failure/none found: returns NULL */
PNDMANAPI pndman_device* pndman_device_detect(
      pndman_device *device);

/* \brief free a device (and remove from list) */
PNDMANAPI pndman_device* pndman_device_free(
      pndman_device *device);

/* \brief free all devices
 * the list will be invalid after this */
PNDMANAPI void pndman_device_free_all(
      pndman_device *device);

/* \brief read repository data from device
 * 0 if repository data could be found
 * -1 if failure or no repository data */
PNDMANAPI int pndman_device_read_repository(
      pndman_repository *repo, pndman_device *device);

/* \brief calculate new MD5 for the PND,
 * Use this if you don't have md5 in pnd or want
 * to recalculate.
 *
 * The MD5 sums are normally calculated only when,
 * PND was installed by libpndman.
 *
 * If you do any PND launching or such, and PND has no MD5
 * yet, it's good idea to recalculate using this,
 * and check if the MD5 differs againt remote MD5. */
PNDMANAPI const char* pndman_package_fill_md5(
      pndman_package *pnd);

/* \brief crawl device for PNDs, fills local repository
 * if full_crawl is 1, everything from PND is crawled,
 * otherwise only package data is crawled,
 * while all the application data is left untouched.
 * returns number of crawled PNDs */
PNDMANAPI int pndman_package_crawl(int full_crawl,
      pndman_device *device, pndman_repository *local);

/* \brief update pnd_package struct
 * by crawling it locally.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_package_crawl_single_package(int full_crawl,
      pndman_package *pnd);

/* \brief get embedded png file from pnd.
 * you need to provide your buffer and it's size.
 * if this functions returns 0, your buffer is filled
 * with the png data.
 * returns number of bytes copied on success, 0 on failure */
PNDMANAPI size_t pndman_package_get_embedded_png(
      pndman_package *pnd, char *buffer, size_t buflen);

/* \brief initialize package handle
 * NOTE: you should pass reference to
 * declared pndman_sync_handle variable
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_package_handle_init(const char *name,
      pndman_package_handle *handle);

/* \brief perform action with the package handle
 * NOTE: this will start a internal curl operation,
 * if flag PNDMAN_PACKAGE_INSTALL is used.
 * see pndman_curl_process, function for what
 * to do next.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_package_handle_perform(
      pndman_package_handle *handle);

/* \brief commit handle to local database
 * NOTE: this won't write anything to disc,
 * see pndman_repository_commit_all function.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_package_handle_commit(
      pndman_package_handle *handle, pndman_repository *local);

/* \brief free/cancel handle
 * NOTE: this function should be safe
 * to call from anywhere. */
PNDMANAPI void pndman_package_handle_free(
      pndman_package_handle *handle);

/* \brief initialize synchorization handle.
 * NOTE: you should pass reference to
 * declared pndman_sync_handle variable.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_sync_handle_init(
      pndman_sync_handle *handle);

/* \brief perform synchorization.
 * NOTE: this starts a internal curl operation,
 * see pndman_curl_process, function for what
 * to do next.
 * return 0 on success, -1 on failure */
PNDMANAPI int pndman_sync_handle_perform(
      pndman_sync_handle *handle);

/* \brief free/cancel synchorization
 * NOTE: this function should be safe
 * to call from anywhere. */
PNDMANAPI void pndman_sync_handle_free(
      pndman_sync_handle *handle);

/* \brief comment on package at repository,
 * you need to pass the repository as well.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_api_comment_pnd(void *user_data, pndman_package *pnd,
      const char *comment, pndman_api_generic_callback callback);

/* \brief get comments from package on repository,
 * comment data is retivied through
 * pndman_api_comment_callback prototype.
 * user_data pointer is for your own data,
 * which is passed back on callback.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_api_comment_pnd_pull(void *user_data, pndman_package *pnd,
      pndman_api_comment_callback callback);

/* \brief delete comment from repository.
 * takes the comment timestamp which will be removed.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_api_comment_pnd_delete(void *user_data,
      pndman_package *pnd, time_t timestamp,
      pndman_api_generic_callback callback);

/* \brief rate the package on repository.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_api_rate_pnd(void *user_data, pndman_package *pnd,
      int rate, pndman_api_rate_callback callback);

/* \brief get own rating for the package on repository.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_api_get_own_rate_pnd(void *user_data, pndman_package *pnd,
      pndman_api_rate_callback callback);

/* \brief get download history from repository
 * history is retivied through
 * pndman_api_history_callback prototype.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_api_download_history(void *user_data,
      pndman_repository *repository, pndman_api_history_callback callback);

/* \brief get archived packages from repository
 * archived pnd's are store in next_installed of pnd
 * user_data pointer is for your own data,
 * which is passed back on callback.
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_api_archived_pnd(void *user_data, pndman_package *pnd,
      pndman_api_archived_callback callback);

/* \brief perform curl operation
 * This is the magic functions that make,
 * everything that needs network transmission work.
 *
 * When you have performed either sync or package handle,
 * you should call this function until it returns either
 * failure or zero.
 *
 * WARNING: It is important that this function runs
 * until it returns either failure or zero,
 * since it does many internal checks and freeups,
 * when needed or requested.
 *
 * If you have GUI application, you can simply run it all the time either on main loop or by intervals.
 * The function won't do anything if it doesn't need to do anything.
 * However since it select's it might be good idea to run on thread for responsitivity.
 *
 * For CLI application, you should run it inside while loop
 * for example until it returns what's expected above.
 *
 * This function does select by the value returned by curl_multi_timeout,
 * however the timeout value is clipped to avoid long stalls (to 1 minute).
 *
 * When curl_multi_timeout doens't return valid timeout and there are internal
 * curl requests active, you can specify what timeout to use with the tv_sec and tv_usec arguments.
 * (seconds, microseconds)
 *
 * NOTE: this replaces pndman_sync(); pndman_download();
 * returns number of curl operations pending, -1 on failure */
PNDMANAPI int pndman_curl_process(unsigned long tv_sec, unsigned long tv_usec);

/* \brief function that does some internal
 * tests to catch up bad programming..
 * eh, let it be :) */
PNDMANAPI int pndman_pxml_test(const char *file);

#ifdef __cplusplus
}
#endif

#endif /* _PNDMAN_H_ */
