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
#define PND_STR      256
#define PND_SHRT_STR 24
#define PND_MD5      32
#define PND_INFO     1024
#define PND_PATH     PATH_MAX
#define REPO_URL     LINE_MAX
#define REPO_NAME    24
#define REPO_VERSION 8
#define HANDLE_NAME  24

#ifdef __cplusplus
extern "C" {
#endif

/* \brief flags for handle to determite what to do */
typedef enum pndman_handle_flags
{
   PNDMAN_HANDLE_INSTALL = 0x01,
   PNDMAN_HANDLE_REMOVE  = 0x02,
   PNDMAN_HANDLE_FORCE   = 0x04,
   PNDMAN_HANDLE_INSTALL_DESKTOP = 0x08,
   PNDMAN_HANDLE_INSTALL_MENU    = 0x16,
   PNDMAN_HANDLE_INSTALL_APPS    = 0x32,
} pndman_handle_flags;

/* type enum for version struct */
typedef enum pndman_version_type
{
   PND_VERSION_RELEASE,
   PND_VERSION_BETA,
   PND_VERSION_ALPHA
} pndman_version_type;

/* x11 enum for exec struct */
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
   const char name[PND_SHRT_STR];
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

   const pndman_translated    *title;
   const pndman_translated    *description;
   const pndman_license       *license;
   const pndman_previewpic    *previewpic;
   const pndman_category      *category;
   const pndman_association   *association;

   struct pndman_application *next;
} pndman_application;

/* \brief Struct that represents PND */
typedef struct pndman_package
{
   const char path[PND_PATH];
   const char id[PND_ID];
   const char icon[PND_PATH];
   char info[PND_INFO];
   char md5[PND_MD5];
   char url[PND_STR];
   char vendor[PND_NAME];
   size_t size;
   time_t modified_time;
   int rating;

   const pndman_author     author;
   const pndman_version    version;
   const pndman_application *app;

   /* bleh, lots of data duplication.. */
   const pndman_translated *title;
   const pndman_translated *description;
   const pndman_category   *category;

   unsigned int flags;
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
   const int exist;
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

   struct pndman_device *next, *prev;
   const int exist;
} pndman_device;

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
   pndman_handle_flags flags;

   /* info */
   int            done;
   const void     *curl;
   const void     *file;
} pndman_handle;

/*! \brief
 * Initializes the library and all the resources
 */
int pndman_init();

/*! \brief
 * Quits the library cleanly, frees all the resources
 */
int pndman_quit();

/* Current API functions
 * These are very likely to change.
 * I'm just defining them here, since I'm getting sick of implict declaration warnings.
 * So no need to document these here yet. */
int pndman_repository_init(pndman_repository *repo);
int pndman_repository_add(char *url, pndman_repository *repo);
int pndman_repository_free(pndman_repository *repo);
int pndman_repository_free_all(pndman_repository *repo);
int pndman_device_init(pndman_device *device);
int pndman_device_add(char *path, pndman_device *device);
int pndman_device_detect(pndman_device *device);
int pndman_device_free(pndman_device *device);
int pndman_device_free_all(pndman_device *device);
int pndman_handle_init(char *name, pndman_handle *handle);
int pndman_handle_perform(pndman_handle *handle);
int pndman_handle_free(pndman_handle *handle);
int pndman_download();
int pndman_read_from_device(pndman_repository *repo, pndman_device *device);
int pndman_sync();
int pndman_sync_request(pndman_repository *repo);
int pndman_commit(pndman_repository *repo, pndman_device *device);

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
