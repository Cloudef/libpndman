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
#ifdef __linux__
#  include <limits.h> /* for LINE_MAX, PATH_MAX */
#endif

/* currently only mingw */
#ifdef __WIN32__
#  include <windows.h> /* for malware */
#  include <limits.h> /* for PATH_MAX */
#  define LINE_MAX 2048
#  ifdef _UNICODE
#     error "Suck it unicde.."
#  endif
#endif

/* public sizes */
#define PNDMAN_ID          256
#define PNDMAN_URL         LINE_MAX
#define PNDMAN_NAME        256
#define PNDMAN_TIMESTAMP   48
#define PNDMAN_VERSION     8
#define PNDMAN_STR         LINE_MAX
#define PNDMAN_SHRT_STR    256
#define PNDMAN_MD5         33
#define PNDMAN_PATH        PATH_MAX

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
   char major[PNDMAN_VERSION];
   char minor[PNDMAN_VERSION];
   char release[PNDMAN_VERSION];
   char build[PNDMAN_VERSION];
   pndman_version_type type;
} pndman_version;

/* \brief struct holding execution information */
typedef struct pndman_exec
{
   int   background;
   char  startdir[PNDMAN_PATH];
   int   standalone;
   char  command[PNDMAN_PATH];
   char  arguments[PNDMAN_STR];
   pndman_exec_x11 x11;
} pndman_exec;

/* \brief struct holding author information */
typedef struct pndman_author
{
   char name[PNDMAN_NAME];
   char website[PNDMAN_STR];
   char email[PNDMAN_STR];
} pndman_author;

/* \brief struct holding documentation information */
typedef struct pndman_info
{
   char name[PNDMAN_NAME];
   char type[PNDMAN_SHRT_STR];
   char src[PNDMAN_PATH];
} pndman_info;

/* \brief struct holding translated strings */
typedef struct pndman_translated
{
   char lang[PNDMAN_SHRT_STR];
   char string[PNDMAN_STR];
   struct pndman_translated *next;
} pndman_translated;

/* \brief struct holding license information */
typedef struct pndman_license
{
   char name[PNDMAN_STR];
   char url[PNDMAN_STR];
   char sourcecodeurl[PNDMAN_STR];
   struct pndman_license *next;
} pndman_license;

/* \brief struct holding previewpic information */
typedef struct pndman_previewpic
{
   char src[PNDMAN_PATH];
   struct pndman_previewpic *next;
} pndman_previewpic;

/* \brief struct holding association information */
typedef struct pndman_association
{
   char name[PNDMAN_STR];
   char filetype[PNDMAN_SHRT_STR];
   char exec[PNDMAN_PATH];
   struct pndman_association *next;
} pndman_association;

/* \brief struct holding category information */
typedef struct pndman_category
{
   char main[PNDMAN_SHRT_STR];
   char sub[PNDMAN_SHRT_STR];
   struct pndman_category *next;
} pndman_category;

/* \brief struct that represents PND application */
typedef struct pndman_application
{
   char id[PNDMAN_ID];
   char appdata[PNDMAN_PATH];
   char icon[PNDMAN_PATH];
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
   char path[PNDMAN_PATH];
   char id[PNDMAN_ID];
   char icon[PNDMAN_PATH];
   char info[PNDMAN_STR];
   char md5[PNDMAN_MD5];
   char url[PNDMAN_STR];
   char vendor[PNDMAN_NAME];
   size_t size;
   time_t modified_time;
   int rating;
   pndman_author     author;
   pndman_version    version;
   pndman_application *app;
   pndman_translated *title;
   pndman_translated *description;
   pndman_license    *license;
   pndman_previewpic *previewpic;
   pndman_category   *category;
   char repository[PNDMAN_STR];
   char mount[PNDMAN_PATH];
   struct pndman_package *update;
   struct pndman_package *next_installed;
   struct pndman_package *next;
   int commercial;
} pndman_package;

/*! \brief struct representing client api access */
typedef struct pndman_repository_api
{
   char root[PNDMAN_URL];
   char username[PNDMAN_SHRT_STR];
   char key[PNDMAN_STR];
} pndman_repository_api;

/*! \brief struct representing repository */
typedef struct pndman_repository
{
   char url[PNDMAN_URL];
   char name[PNDMAN_NAME];
   char updates[PNDMAN_URL];
   char version[PNDMAN_VERSION];
   time_t timestamp;
   pndman_package *pnd;
   pndman_repository_api api;
   struct pndman_repository *next, *prev;
} pndman_repository;

/*! \brief struct representing device */
typedef struct pndman_device
{
   char mount[PNDMAN_PATH];
   char device[PNDMAN_PATH];
   size_t size, free, available;
   char appdata[PNDMAN_PATH];
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

/* \brief callback function definition to handle */
typedef void (*pndman_callback)(pndman_curl_code code, void *data);

/*! \brief Struct for PND transaction */
typedef struct pndman_package_handle
{
   char name[PNDMAN_NAME];
   char error[PNDMAN_STR];
   pndman_package *pnd;
   pndman_device  *device;
   pndman_repository *repository;
   unsigned int flags;
   pndman_curl_progress progress;
   pndman_callback callback;

   /* don't touch */
   const void *data;
} pndman_package_handle;

/* \brief struct for repository synchorization */
typedef struct pndman_sync_handle
{
   char error[PNDMAN_STR];
   pndman_repository *repository;
   unsigned int flags;
   pndman_curl_progress progress;
   pndman_callback callback;

   /* don't touch */
   const void *data;
} pndman_sync_handle;

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

/* \brief update pnd_package by crawling it locally
 * returns 0 on success, -1 on failuer */
PNDMANAPI int pndman_package_crawl_single_package(int full_crawl,
      pndman_package *pnd);

/* \brief initialize new pndman handle
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_package_handle_init(const char *name,
      pndman_package_handle *handle);

/* \brief perform action with the handle
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_package_handle_perform(
      pndman_package_handle *handle);

/* \brief commit handle to local database
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_package_handle_commit(
      pndman_package_handle *handle, pndman_repository *local);

/* \brief free/cancel handle */
PNDMANAPI void pndman_package_handle_free(
      pndman_package_handle *handle);

/* \brief create new synchorization handle
 * returns 0 on success, -1 on failure */
PNDMANAPI int pndman_sync_handle_init(
      pndman_sync_handle *handle);

/* \brief perform synchorization
 * return 0 on success, -1 on failure */
PNDMANAPI int pndman_sync_handle_perform(
      pndman_sync_handle *handle);

/* \brief free/cancel synchorization */
PNDMANAPI void pndman_sync_handle_free(
      pndman_sync_handle *handle);

/* \brief perform curl operation
 * returns number of curl operations pending
 *
 * NOTE: this replaces pndman_sync(); pndman_download(); */
PNDMANAPI int pndman_curl_process(void);

/* eh, let it be :) */
int pnd_do_something(const char *file);

#ifdef __cplusplus
}
#endif

#endif /* _PNDMAN_H_ */
