#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include "pndman.h"
#include "package.h"
#include "device.h"
#include "repository.h"

/* strings */
static const char *REPO_INIT_FAIL   = "Failed to allocate pndman_repository, shit might break.";

/* \brief initialize repository struct */
static pndman_repository* _pndman_repository_init()
{
   pndman_repository *repo;

   repo = malloc(sizeof(pndman_repository));
   if (!repo) {
      DEBFAIL(REPO_INIT_FAIL);
      return NULL;
   }

   memset(repo->url,       0, REPO_URL);
   memset(repo->name,      0, REPO_NAME);
   memset(repo->updates,   0, REPO_URL);
   strcpy(repo->version, "1.0");
   repo->timestamp = 0;
   repo->pnd  = NULL;
   repo->next = NULL;
   repo->prev = NULL;
   return repo;
}

/* INTERNAL */

/* \brief Find first repo */
inline pndman_repository* _pndman_repository_first(pndman_repository *repo)
{
   /* find first */
   for(; repo->prev; repo = repo->prev);
   return repo;
}

/* \brief Find last repo */
inline pndman_repository* _pndman_repository_last(pndman_repository *repo)
{
   /* find last */
   for(; repo->next; repo = repo->next);
   return repo;
}

/* \brief get repository by url */
inline pndman_repository* _pndman_repository_get(const char *url, pndman_repository *list)
{
   pndman_repository *r;
   r = _pndman_repository_first(list);
   for(; r; r = r->next) if (!strcmp(r->url, url)) return r;
   return NULL;
}

/* \brief add new pnd to repository */
pndman_package* _pndman_repository_new_pnd(pndman_repository *repo)
{
   pndman_package *p;
   assert(repo);

   if (repo->pnd) {
         for (p = repo->pnd; p->next; p = p->next);
         p = p->next = _pndman_new_pnd();
   } else p = repo->pnd = _pndman_new_pnd();
   strncpy(p->repository, repo->url, PND_STR-1);

   return p;
}

/* \brief check duplicate pnd on repo, return that pnd if found, else return new */
pndman_package* _pndman_repository_new_pnd_check(char *id, char *path, pndman_version *ver, pndman_repository *repo)
{
   pndman_package *pnd, *pni, *pr;

   pr = NULL;
   for (pnd = repo->pnd; pnd; pnd = pnd->next) {
      if (!strcmp(id, pnd->id)) {
         /* if local repository, create instance */
         if (!repo->prev) {
            /* create instance here, path differs! */
            if (!repo->prev && strlen(path) && strcmp(path, pnd->path)) { /* only local repo can have next installed */
               if (!_pndman_vercmp(&pnd->version, ver)) {
                  /* new pnd is older, assing it to end */
                  for (pni = pnd; pni && pni->next_installed; pni = pni->next_installed)
                     if (!strcmp(path, pni->path)) return pni; /* it's next installed */
                  if (!(pni = pni->next_installed = _pndman_new_pnd()))
                     return NULL;
                  pni->next = pnd->next; /* assign parent's next to next */
                  DEBUGP(3, "Older : %s\n", path);
               } else {
                  /* new pnd is newer, assign it to first */
                  if (!(pni = _pndman_new_pnd())) return NULL;
                  pr->next = pni; pni->next_installed = pnd; /* assign nexts */
                  pni->next = pnd->next;
                  DEBUGP(3, "Newer : %s\n", path);
               }
               strncpy(pni->repository, repo->url, PND_STR-1);
               return pni;
            } else
               return pnd; /* this is the same pnd as installed locally */
         } else
            return pnd; /* remote repository can't have instances :) */
      }
      pr = pnd;
   }

   /* create new pnd */
   return _pndman_repository_new_pnd(repo);
}

/* \brief free pnd from repository */
int _pndman_repository_free_pnd(pndman_package *pnd, pndman_repository *repo)
{
   pndman_package *p, *pn, *pr;
   assert(pnd && repo);

   /* return fail if no pnd at repo */
   if (!repo->pnd) return RETURN_FAIL;

   /* check first first */
   if (repo->pnd == pnd) {
      /* if no next_installed, assing next pnd in repo list */
      if (!(repo->pnd = _pndman_free_pnd(pnd)))
         repo->pnd = repo->pnd->next; /* next pnd */
      return RETURN_OK;
   }

   pr = NULL;
   for (p = repo->pnd; p; p = p->next) {
      /* check parent */
      if (p == pnd) {
         assert(pr); /* this should never fail */
         pr->next = p->next; /* assign next to previous next */
         if ((p = _pndman_free_pnd(pnd)))
            pr->next = p; /* assign next_installed to previous next */
         return RETURN_OK;
      }

      /* check childs */
      pr = p; /* set this as parent for next_installed */
      for (pn = p->next_installed; pn; pn = pn->next_installed) {
         if (pn == pnd) {
            assert(pr); /* this should never fail */
            pr->next_installed = p->next_installed; /* assign next_installed to previous next_installed */
            if ((p = _pndman_free_pnd(pnd)))
               pr->next_installed = p; /* assign next_installed to previous next_installed or parent */
            return RETURN_OK;
         }
         pr = pn; /* set this next_installed as previous */
      }

      pr = p; /* set the parent as previous again */
   }

   /* pnd not found */
   return RETURN_FAIL;
}

/* \brief free all pnds from repository */
int _pndman_repository_free_pnd_all(pndman_repository *repo)
{
   pndman_package *p, *n;
   for (p = repo->pnd; p; p = n)
   { n = p->next; while ((p = _pndman_free_pnd(p))); }
   return RETURN_OK;
}


/* \brief Allocate next repo */
static pndman_repository* _pndman_repository_new(pndman_repository **repo)
{
   pndman_repository *new;
   if (!repo) return NULL;

   /* find last repo */
   *repo = _pndman_repository_last(*repo);
   new = _pndman_repository_init();
   if (!new) return NULL;

   /* set defaults */
   new->prev     = *repo;
   (*repo)->next = new;
   *repo         = new;
   return *repo;
}

/* \brief Allocate new if exists */
static pndman_repository* _pndman_repository_new_if_exist(pndman_repository **repo, const char *check_existing)
{
   if (!*repo) return *repo = _pndman_repository_init();
   if (check_existing && _pndman_repository_get(check_existing, *repo))
      return NULL;

   return _pndman_repository_new(repo);
}

/* \brief Add new repo */
static pndman_repository* _pndman_repository_add(const char *url, pndman_repository *repo)
{
   if (!_pndman_repository_new_if_exist(&repo, url))
      return NULL;

   strncpy(repo->url, url, REPO_URL-1);
   return repo;
}

/* \brief Free one repo, returns first repo */
static pndman_repository* _pndman_repository_free(pndman_repository *repo)
{
   pndman_repository *first;
   assert(repo);

   /* set previous repo point to the next repo */
   if (repo->prev)
      repo->prev->next = repo->next;

   /* set next point back to the previous repo */
   if (repo->next)
      repo->next->prev = repo->prev;

   /* get first repo */
   first = repo->prev ? _pndman_repository_first(repo) : repo->next;

   /* free the repository */
   _pndman_repository_free_pnd_all(repo);
   free(repo);
   return first;
}

/* \brief Free all repos */
static int _pndman_repository_free_all(pndman_repository *repo)
{
   pndman_repository *next;
   assert(repo);

   /* find the first repo */
   repo = _pndman_repository_first(repo);

   /* free everything */
   for(; repo; repo = next) {
      next = repo->next;
      _pndman_repository_free_pnd_all(repo);
      free(repo);
   }

   return RETURN_OK;
}

/* \brief Check local repository for bad/removed PNDs, return number of packages removed */
static int _pndman_repository_check(pndman_repository *repo)
{
   pndman_package *p, *pn;
   FILE *f;
   int removed = 0;
   assert(repo);

   for (p = repo->pnd; p; p = pn) {
      pn = p->next;
      if (!(f = fopen(p->path, "r"))) {
         if (_pndman_repository_free_pnd(p, repo) == RETURN_OK)
            ++removed;
      }
      else fclose(f);
   }
   return removed;
}

/* API */

/* \brief Initialize repo list
 * NOTE: first repository == local repository */
pndman_repository* pndman_repository_init()
{
   pndman_repository *repo;
   repo = _pndman_repository_init();
   if (!repo) return NULL;
   strcpy(repo->name, LOCAL_DB_NAME);
   return repo;
}

/* \brief Add new repository */
pndman_repository* pndman_repository_add(const char *url, pndman_repository *repo)
{
   if (!repo) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "repo list pointer is NULL");
      return NULL;
   }
   return _pndman_repository_add(url, repo);
}

/* \brief Clear repository, effectiely frees the PND list */
void pndman_repository_clear(pndman_repository *repo)
{
   if (!repo) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "repo pointer is NULL");
      return;
   }
   _pndman_repository_free_pnd_all(repo);
   repo->pnd = NULL;
}

/* \brief Check local repository for bad/removed PNDs, returns number of packages removed */
int pndman_repository_check_local(pndman_repository *local)
{
   if (!local) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "local pointer is NULL");
      return 0;
   }
   local = _pndman_repository_first(local); /* make fool proof */
   return _pndman_repository_check(local);
}

/* \brief Free one repo, returns first repo */
pndman_repository* pndman_repository_free(pndman_repository *repo)
{
   if (!repo) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "repo pointer is NULL");
      return NULL;
   }
   return _pndman_repository_free(repo);
}

/* \brief Free all repos */
int pndman_repository_free_all(pndman_repository *repo)
{
   if (!repo) {
      DEBUGP(1, _PNDMAN_WRN_BAD_USE, "repo pointer is NULL");
      return RETURN_FAIL;
   }
   return _pndman_repository_free_all(repo);
}

/* vim: set ts=8 sw=3 tw=0 :*/
