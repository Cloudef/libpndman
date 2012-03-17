#ifndef PNDMAN_H
#define PNDMAN_H

#include <time.h>
#ifdef __linux__
#  include <limits.h>
#endif

/* Currently only mingw */
#ifdef __WIN32__
#  include <windows.h>
#  include <limits.h>
#  define LINE_MAX 256
#  ifdef _UNICODE
#     error "Suck it unicde.."
#  endif
#endif

/* def */
#define PND_ID       256
#define PND_NAME     24
#define PND_VER      8
#define PND_STR      LINE_MAX
#define PND_SHRT_STR 24
#define PND_MD5      33
#define PND_INFO     LINE_MAX
#define PND_PATH     PATH_MAX
#define REPO_URL     LINE_MAX
#define REPO_NAME    24
#define REPO_VERSION 8
#define HANDLE_NAME  PND_ID

#ifdef __cplusplus
extern "C" {
#endif

/* \brief flags for sync request to determite what to do */
typedef enum pndman_sync_flags
{
   PNDMAN_SYNC_FULL = 0x001,
} pndman_sync_flags;

/* \brief flags for handle to determite what to do */
typedef enum pndman_handle_flags
{
   PNDMAN_HANDLE_INSTALL         = 0x001,
   PNDMAN_HANDLE_REMOVE          = 0x002,
   PNDMAN_HANDLE_FORCE           = 0x004,
   PNDMAN_HANDLE_INSTALL_DESKTOP = 0x008,
   PNDMAN_HANDLE_INSTALL_MENU    = 0x010,
   PNDMAN_HANDLE_INSTALL_APPS    = 0x020,
} pndman_handle_flags;

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

/* \brief Struct holding version information */
typedef struct pndman_version
{
   const char major[PND_VER];
   const char minor[PND_VER];
   const char release[PND_VER];
   const char build[PND_VER];
   const pndman_version_type type;
} pndman_version;

/* \brief Struct holding execution information */
typedef struct pndman_exec
{
   const int   background;
   const char  startdir[PND_PATH];
   const int   standalone;
   const char  command[PND_PATH];
   const char  arguments[PND_STR];
   const pndman_exec_x11 x11;
} pndman_exec;

/* \brief Struct holding author information */
typedef struct pndman_author
{
   const char name[PND_NAME];
   const char website[PND_STR];
   const char email[PND_STR];
} pndman_author;

/* \brief Struct holding documentation information */
typedef struct pndman_info
{
   const char name[PND_NAME];
   const char type[PND_SHRT_STR];
   const char src[PND_PATH];
} pndman_info;

/* \brief Struct holding translated strings */
typedef struct pndman_translated
{
   const char lang[PND_SHRT_STR];
   const char string[PND_STR];

   struct pndman_translated *next;
} pndman_translated;

/* \brief Struct holding license information */
typedef struct pndman_license
{
   const char name[PND_STR];
   const char url[PND_STR];
   const char sourcecodeurl[PND_STR];

   struct pndman_license *next;
} pndman_license;

/* \brief Struct holding previewpic information */
typedef struct pndman_previewpic
{
   const char src[PND_PATH];
   struct pndman_previewpic *next;
} pndman_previewpic;

/* \brief Struct holding association information */
typedef struct pndman_association
{
   const char name[PND_STR];
   const char filetype[PND_SHRT_STR];
   const char exec[PND_PATH];

   struct pndman_association *next;
} pndman_association;

/* \ brief Struct holding category information */
typedef struct pndman_category
{
   const char main[PND_SHRT_STR];
   const char sub[PND_SHRT_STR];

   struct pndman_category *next;
} pndman_category;

/* \brief Struct that represents PND application */
typedef struct pndman_application
{
   const char id[PND_ID];
   const char appdata[PND_PATH];
   const char icon[PND_PATH];
   const int  frequency;

   const pndman_author  author;
   const pndman_version osversion;
   const pndman_version version;
   const pndman_exec    exec;
   const pndman_info    info;

   pndman_translated    *title;
   pndman_translated    *description;
   pndman_license       *license;
   pndman_previewpic    *previewpic;
   pndman_category      *category;
   pndman_association   *association;

   struct pndman_application *next;
} pndman_application;

/* \brief Struct that represents PND */
typedef struct pndman_package
{
   const char path[PND_PATH];
   const char id[PND_ID];
   const char icon[PND_PATH];
   const char info[PND_INFO];
   const char md5[PND_MD5];
   const char url[PND_STR];
   const char vendor[PND_NAME];
   const size_t size;
   const time_t modified_time;
   const int rating;

   const pndman_author     author;
   const pndman_version    version;
   pndman_application *app;

   /* bleh, lots of data duplication.. */
   pndman_translated *title;
   pndman_translated *description;
   pndman_license    *license;
   pndman_previewpic *previewpic;
   pndman_category   *category;

   const char repository[PND_STR];
   const char device[PND_PATH];
   struct pndman_package *update;
   struct pndman_package *next_installed;
   struct pndman_package *next;
} pndman_package;

/*! \brief
 * COMMNON
 * Repository struct, will contain pointer to repository's packages
 * TODO: fill
 */
typedef struct pndman_repository
{
   const char url[REPO_URL];
   const char name[REPO_NAME];
   const char updates[REPO_URL];
   const char version[REPO_VERSION];
   const time_t timestamp;

   pndman_package *pnd;
   struct pndman_repository *next, *prev;
} pndman_repository;

/*! \brief
 * COMMON
 * Device struct
 */
typedef struct pndman_device
{
   const char mount[PATH_MAX];
   const char device[PATH_MAX];
   const size_t size, free, available;

   char appdata[PATH_MAX];
   struct pndman_device *next, *prev;
} pndman_device;

/* \brief curl_write_result struct for storing curl result to memory */
typedef struct curl_write_result
{
   const void *data;
   const int   pos;
} curl_write_result;

/* \brief struct for holding internal request data */
typedef struct curl_request
{
   const curl_write_result result;
   const void              *curl;
} curl_request;

/* \brief struct for holding progression data of curl handle */
typedef struct curl_progress
{
   const double download;
   const double total_to_download;
   const char done;
} curl_progress;

/*! \brief
 * COMMON
 * Struct for PND transiction
 */
typedef struct pndman_handle
{
   const char     name[HANDLE_NAME];
   const char     error[LINE_MAX];
   pndman_package *pnd;
   pndman_device  *device;
   unsigned int   flags;

   /* progress */
   const curl_progress  progress;

   /* internal */
   const curl_request   request;
   const void           *file;
} pndman_handle;

/* \brief
 * COMMON
 * Struct for repository synchorization */
typedef struct pndman_sync_handle
{
   const char           error[LINE_MAX];
   pndman_repository    *repository;

   /* unlike handles, these get set on function */
   const unsigned int   flags;

   /* progress */
   const curl_progress  progress;

   /* internal */
   const void           *file;
   const void           *curl;
} pndman_sync_handle;

/* \brief get git head */
const char* pndman_git_head();

/* \brief get git commit description */
const char* pndman_git_commit();

/* \brief calculate new md5 for the PND,
 * Use this if you don't have md5 in pnd or want to recalculate */
const char* pndman_get_md5(pndman_package *pnd);

/* \brief set verbose level of pndman
 * (prints to stdout) */
void pndman_set_verbose(int level);

/* \brief get current verbose level */
int pndman_get_verbose();

/* \brief get error string from pndman */
const char* pndman_get_error();

/* Current API functions
 * These are very likely to change.
 * I'm just defining them here, since I'm getting sick of implict declaration warnings.
 * So no need to document these here yet. */
pndman_repository* pndman_repository_init();
pndman_repository* pndman_repository_add(const char *url, pndman_repository *list);
void pndman_repository_clear(pndman_repository *repo);
void pndman_repository_check_local(pndman_repository *local);
pndman_repository* pndman_repository_free(pndman_repository *repo);
int pndman_repository_free_all(pndman_repository *repo);
pndman_device* pndman_device_add(const char *path, pndman_device *list);
pndman_device* pndman_device_detect(pndman_device *device);
pndman_device* pndman_device_free(pndman_device *device);
int pndman_device_free_all(pndman_device *device);
int pndman_handle_init(const char *name, pndman_handle *handle);
int pndman_handle_perform(pndman_handle *handle);
int pndman_handle_commit(pndman_handle *handle, pndman_repository *local);
int pndman_handle_free(pndman_handle *handle);
int pndman_download();
int pndman_read_from_device(pndman_repository *repo, pndman_device *device);
int pndman_sync();
int pndman_sync_request(pndman_sync_handle *handle, unsigned int flags, pndman_repository *repo);
int pndman_sync_request_free(pndman_sync_handle *handle);
int pndman_commit_all(pndman_repository *repo, pndman_device *device);
int pndman_crawl(int full_crawl, pndman_device *device, pndman_repository *local);
int pndman_crawl_pnd(int full_crawl, pndman_package *pnd);
int pndman_check_updates(pndman_repository *list);

/* test thing, for surely */
int pnd_do_something(char *file);

#ifdef __cplusplus
}
#endif

/* undef */
#undef PND_ID
#undef PND_NAME
#undef PND_VER
#undef PND_STR
#undef PND_SHRT_STR
#undef PND_INFO
#undef PND_MD5
#undef PND_PATH
#undef REPO_URL
#undef REPO_NAME
#undef REPO_VERSION
#undef HANDLE_NAME

/* Some design notes here...
 *
 * Library will be initialized sameway with init && quit commands.
 * Operations will be replaced with handles that can be attached to transictions,
 * this way operations can be done same time, queued and so on.. It will be much simpler and more powerful design.
 *
 * Handles could be reused after transictions, so it's not neccessary to always free and get new handle again.
 *
 * The return values will be following:
 * 0  for success
 * -1 for failure
 *
 * Expections will be functions that act as tester/expression for example pnd_exist( pnd ); would return 1, if pnd exists and 0 if not.
 *
 * Pseudo code:
 *
 * if (pndman_init() == -1)
 *    failure("Failed to initialize pndman");
 *
 * pndman_device device;
 * if (pndman_device_add("Absolute path || Mount point || Device", &device) == -1)
 *    failure("Could not add device");
 *
 * // Read repositories from configuration
 * pndman_repository repo;
 * if (pndman_read_repositories_from(&device, &repo) == -1)
 *    puts("Warning, failed to read repositories from configuration");
 *
 * // Add own repo as well
 * if (pndman_repo_add("URL", &repo) == -1)
 *    puts("Warning, failed to add new repo");
 *
 * // Sync repositories
 * pndman_sync_result sync_result;
 * if (pndman_repo_sync(&repo, &sync_result) == -1)
 *    failure("Something went wrong with crawling");
 *
 * // Result would be a linked list with pointers to added && updated pndman_packages
 *
 * // Crawl for local entries
 * pandman_crawl_result crawl_result;
 * if (pandman_repo_crawl(&repo, &crawl_result) == -1)
 *    failure("Something went wrong with crawling");
 *
 * // Result would be a linked list with pointers to found pndman_packages
 *
 * // Create handle for transiction
 * pndman_handle handle;
 * if (pndman_get_handle("Porn download", &handle))
 *    failure("Failed to allocate handle");
 *
 * handle.repository = &repo;
 * handle.pnd = pndman_get_pnd("touhou", &repo);
 * if (!handle.pnd)
 *    failure("PND does not exist");
 *
 * // There would be operation flags (SYNC, REMOVE), which only one can be specified
 * // Plus some minor flags which can be specified with them.
 * handle.flags = PNDMAN_FLAG_SYNC;
 *
 * // This commits our transiction
 * // The library will be thread safe so you can run this in thread.
 * // If ran in thread, you will get process with pndman_handle_process function.
 * if (pndman_handle_commit(&handle) == -1)
 *    failure(handle->error_string);
 *
 * // In reality this switch would be unneccessary here,
 * // As we could just switch transiction flag to FLAG_REMOVE and commit again.
 * if (pndman_reset_handle(&handle) == -1)
 *    failure(handle->error_string);
 *
 * handle.repository = &repo;
 * handle.pnd = pndman_get_pnd("touhou", &repo);
 * handle.flags = PNDMAN_FLAG_REMOVE;
 * if (pndman_handle_commit(&handle) == -1)
 *    failure(handle->error_string);
 *
 * // This is going to be the biggest difference beetwen libmilky and libpndman
 * // You commit the changes to devices manually, database is multidevice aware so no mergin or any other tricks are needed.
 * pndman_device_commit_handle(&handle, &device);
 *
 * pndman_free_handle(&handle);
 * pndman_free_repositories(&repo);
 * pndman_free_devices(&device);
 *
 * if (pndman_quit() == -1)
 *    puts("Warning, pndman quit uncleanly!");
 *
 * Local database is going to have few changes.
 * Instead of mergin all repositories to single JSON blob we are going to keep them seperate
 *
 * eg.
 * [milkshake-repo]
 * LOCAL JSON DATA
 *
 * [ultimate-repo]
 * LOCAL JSON DATA
 *
 * If same PND is installed to multiple devices, you can choose which one to remove and so on.
 *
 * Local database is just seperated modified JSON repository, with following modifications
 *
 * header
 * {
 *    timestamp: %UNIXTIME%
 * }
 *
 * package
 * {
 *    id: "touhou"
 *    install: ["path1", "path2"]
 *    flags: installed, has update etc...
 * }
 *
 * Otherwise the original repository format stays intact
 *
 * PND handling will be done so that it supports multiple backends, such as common XML reader backend for cross-platformity and low-level access and libpnd for high-level access mostly for Pandora.
 *
 * This is the current desing, it might change heavily from this in time of deveploment. But the main point is to keep the library's code _simple_, to prevent any critical errors and such.
 *
 */

#endif /* PNDMAN_H */
