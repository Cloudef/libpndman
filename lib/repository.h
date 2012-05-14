#ifndef PNDMAN_REPOSITORY_H
#define PNDMAN_REPOSITORY_H

#include <time.h>

#define LOCAL_DB_NAME "libpndman repository"

#define REPO_URL        LINE_MAX
#define REPO_NAME       24
#define REPO_TIMESTAMP  48
#define REPO_VERSION    8

#ifdef __cplusplus
extern "C" {
#endif

/* \brief pndman_repository struct */
typedef struct pndman_repository
{
   char url[REPO_URL];
   char name[REPO_NAME];
   char updates[REPO_URL];
   char client_api[REPO_URL];
   char version[REPO_VERSION];
   time_t timestamp;

   pndman_package *pnd;
   struct pndman_repository *next, *prev;
} pndman_repository;

pndman_repository* _pndman_repository_first(pndman_repository *repo);
pndman_repository* _pndman_repository_last(pndman_repository *repo);
pndman_repository* _pndman_repository_get(const char *url, pndman_repository *list);

pndman_package* _pndman_repository_new_pnd(pndman_repository *repo);
pndman_package* _pndman_repository_new_pnd_check(char *id, char *path, pndman_version *ver, pndman_repository *repo);
int _pndman_repository_free_pnd(pndman_package *pnd, pndman_repository *r);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_REPOSITORY_H */

/* vim: set ts=8 sw=3 tw=0 :*/
