#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include "pndman.h"
#include "package.h"
#include "device.h"
#include "repository.h"

/* \brief initialize repository struct */
static pndman_repository* _pndman_repository_init()
{
   pndman_repository *repo;

   repo = malloc(sizeof(pndman_repository));
   if (!repo) return NULL;

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

/* \brief add new pnd to repository */
pndman_package* _pndman_repository_new_pnd(pndman_repository *repo)
{
   pndman_package *p;
   assert(repo);

   if (repo->pnd) {
         for (p = repo->pnd; p->next; p = p->next);
         p = p->next = _pndman_new_pnd();
   } else p = repo->pnd = _pndman_new_pnd();

   return p;
}

/* \brief check duplicate pnd on repo, return that pnd if found, else return new */
pndman_package* _pndman_repository_new_pnd_check(char *id, char *path, pndman_repository *repo)
{
   pndman_package *pnd, *pni;

   for (pnd = repo->pnd; pnd; pnd = pnd->next) {
      if (!strcmp(id, pnd->id)) {
         /* if local repository, create instance */
         if (!repo->prev) {
            if (strcmp(path, pnd->path)) {
               /* create instance here, path differs! */
               for (pni = pnd; pni->next_installed; pni = pni->next_installed);
               pni->next_installed = _pndman_new_pnd();
               if (!pni->next_installed) return NULL;
               pni->next_installed->next = pni->next;
               return pni->next_installed;
            } else
               return pnd; /* this is the same pnd as installed locally */
         } else
            return pnd; /* remote repository can't have instances :) */
      }
   }

   /* create new pnd */
   return _pndman_repository_new_pnd(repo);
}

/* \brief free pnd from repository */
int _pndman_repository_free_pnd(pndman_package *pnd, pndman_repository *repo)
{
   pndman_package *p;
   assert(pnd && repo);

   for (p = repo->pnd; p; p = p->next)
      if (p->next == pnd) {
         if (p->next->next) p->next = p->next->next;
         else               p->next = NULL;
         free(pnd);
         return RETURN_OK;
      }
   return RETURN_FAIL;
}

/* \brief free all pnds from repository */
int _pndman_repository_free_pnd_all(pndman_repository *repo)
{
   pndman_package *p, *n;
   for (p = repo->pnd; p; p = n)
   { n = p->next; _pndman_free_pnd(p); }
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
static pndman_repository* _pndman_repository_new_if_exist(pndman_repository **repo, char *check_existing)
{
   pndman_repository *r;
   if (check_existing) {
      r = _pndman_repository_first(*repo);
      for(; r; r = r->next) {
         // DEBUGP("%s == %s\n", r->url, check_existing);
         if (!strcmp(r->url, check_existing)) return NULL;
      }
   }

   return _pndman_repository_new(repo);
}

/* \brief Add new repo */
static pndman_repository* _pndman_repository_add(char *url, pndman_repository *repo)
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
      repo->prev->next    = repo->next;

   /* set next point back to the previous repo */
   if (repo->next)
      repo->next->prev = repo->prev;

   /* get first repo */
   first = _pndman_repository_first(repo);

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

   /* find the last repo */
   repo = _pndman_repository_last(repo);

   /* free everything */
   for(; repo; repo = next) {
      next = repo->next;
      _pndman_repository_free_pnd_all(repo);
      free(repo);
   }

   return RETURN_OK;
}

/* API */

/* \brief Initialize repo list
 * NOTE: first repository == local repository */
pndman_repository* pndman_repository_init()
{
   pndman_repository *repo;
   DEBUG("pndman repo init");
   repo = _pndman_repository_init();
   if (!repo) return NULL;
   strcpy(repo->name, LOCAL_DB_NAME);
   return repo;
}

/* \brief Add new repository */
pndman_repository* pndman_repository_add(char *url, pndman_repository *repo)
{
   DEBUG("pndman repo add");
   if (!repo) return NULL;
   return _pndman_repository_add(url, repo);
}

/* \brief Free one repo, returns first repo */
pndman_repository* pndman_repository_free(pndman_repository *repo)
{
   DEBUG("pndman repo free");
   if (!repo) return NULL;
   return _pndman_repository_free(repo);
}

/* \brief Free all repos */
int pndman_repository_free_all(pndman_repository *repo)
{
   DEBUG("pndman repo free all");
   if (!repo) return RETURN_FAIL;
   return _pndman_repository_free_all(repo);
}

/* vim: set ts=8 sw=3 tw=0 :*/
