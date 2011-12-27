#ifndef PNDMAN_H
#define PNDMAN_H

#ifdef __linux__
#  include <limits.h>
#endif

#ifdef __WIN32__
#  define WINVER 0x0500
#  include <windows.h>
#  define PATH_MAX MAX_PATH
#  define LINE_MAX 256
#  ifdef _UNICODE
#     error "Suck it unicde.."
#  endif
#endif

/*! \brief
 * COMMON
 * Struct for PND
 */
typedef struct pndman_package
{
   const char path[PATH_MAX];
   unsigned int flags;

   struct pndman_package *next_installed;
   struct pndman_package *next;
} pndman_package;

/*! \brief
 * COMMNON
 * Repository struct, will contain pointer to repository's packages
 */
typedef struct pndman_repository
{
   const char url[LINE_MAX];
   const char name[LINE_MAX];
   const char updates[LINE_MAX];
   float version;
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
   size_t size, free, available;

   struct pndman_device *next, *prev;
} pndman_device;

/*! \brief
 * COMMON
 * Struct for PND transiction
 */
typedef struct pndman_handle
{
   char           name[25];
   char           error[LINE_MAX];
   char           url[LINE_MAX];
   unsigned int   flags;

   /* info */
   int            done;
   void*          curl;
} pndman_handle;

/*! \brief
 * Initializes the library and all the resources
 */
int pndman_init();

/*! \brief
 * Quits the library cleanly, frees all the resources
 */
int pndman_quit();

/*! \brief
 * Return a new handle for transiction
 * */
pndman_handle* pndman_get_handle(char *name);

/*! \brief
 * Use this to reset the handle for reuse.
 * Transiction assigments will be lost, while other data will be intact.
 */
int pndman_reset_handle(pndman_handle *handle);

/*! \brief
 * Free a transiction handle, when you don't need it anymore
 */
int pndman_free_handle(pndman_handle *handle);

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
