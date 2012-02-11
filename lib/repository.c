#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

#include "pndman.h"
#include "package.h"
#include "device.h"
#include "repository.h"

/* INTERNAL */

/* \brief initialize repository struct */
static int _pndman_repository_init(pndman_repository *repo)
{
   memset(repo->url,       0, REPO_URL);
   memset(repo->name,      0, REPO_NAME);
   memset(repo->updates,   0, REPO_URL);
   repo->version   = 0;
   repo->exist     = 1;
   repo->timestamp = 0;
   repo->pnd  = NULL;
   repo->next = NULL;
   repo->prev = NULL;
}

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

/* \brief free pnd from repository */
int _pndman_repository_free_pnd(pndman_package *pnd, pndman_repository *repo)
{
   pndman_package *p;
   assert(repo);
   assert(pnd);

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
static int _pndman_repository_new(pndman_repository **repo)
{
   pndman_repository *new;
   if (!repo) return RETURN_FAIL;

   /* find last repo */
   *repo = _pndman_repository_last(*repo);
   new = calloc(1, sizeof(pndman_repository));
   if (!new) return RETURN_FAIL;

   /* set defaults */
   _pndman_repository_init(new);
   new->prev     = *repo;
   (*repo)->next = new;
   *repo         = new;
   return RETURN_OK;
}

/* \brief Allocate new if exists */
static int _pndman_repository_new_if_exist(pndman_repository **repo, char *check_existing)
{
   pndman_repository *r;
   if (check_existing) {
      r = _pndman_repository_first(*repo);
      for(; r && r->exist; r = r->next) {
         // DEBUGP("%s == %s\n", r->url, check_existing);
         if (!strcmp(r->url, check_existing)) return RETURN_FAIL;
      }
   }

   *repo = _pndman_repository_last(*repo);
   if (!(*repo)->exist) return RETURN_OK;

   return _pndman_repository_new(repo);
}

/* \brief Add new repo */
static int _pndman_repository_add(char *url, pndman_repository *repo)
{
   if (_pndman_repository_new_if_exist(&repo, url) != RETURN_OK)
      return RETURN_FAIL;

   strncpy(repo->url, url, REPO_URL-1);
   repo->exist = 1;

   return RETURN_OK;
}

/* \brief Free one repo */
static int _pndman_repository_free(pndman_repository *repo)
{
   pndman_repository *deleted;

   if (!repo) return RETURN_FAIL;

   /* avoid freeing the first repo */
   if (repo->prev) {
      /* set previous repo point to the next repo */
      repo->prev->next    = repo->next;

      /* set next point back to the previous repo */
      if (repo->next)
         repo->next->prev = repo->prev;

      _pndman_repository_free_pnd_all(repo);
      free(repo);
   }
   else {
      if (repo->next) {
         /* copy next to this and free the next */
         strcpy(repo->url,     repo->next->url);
         strcpy(repo->name,    repo->next->name);
         strcpy(repo->updates, repo->next->updates);
         repo->version  = repo->next->version;
         repo->exist    = repo->next->exist;
         repo->pnd      = repo->next->pnd;

         deleted     = repo->next;
         repo->next  = deleted->next;

         if (repo->next)
            repo->next->prev = repo;

         _pndman_repository_free_pnd_all(deleted);
         free(deleted);
      }
      else _pndman_repository_init(repo);
   }

   return RETURN_OK;
}

/* \brief Free all repos */
static int _pndman_repository_free_all(pndman_repository *repo)
{
   pndman_repository *prev;

   if (!repo) return RETURN_FAIL;

   /* find the last repo */
   repo = _pndman_repository_last(repo);

   /* free everything */
   for(; repo->prev; repo = prev) {
      prev = repo->prev;
      _pndman_repository_free_pnd_all(repo);
      free(repo);
   }
   _pndman_repository_free_pnd_all(repo);
   _pndman_repository_init(repo);

   return RETURN_OK;
}

/* \brief Sync repository */
static int _pndman_repository_sync(pndman_repository *repo, pndman_device *device)
{
   /* check against local */
   if (device) _pndman_query_repository_from_devices(repo, device);

   /* check against remote */
   return _pndman_query_repository_from_json(repo);
}

/* API */

/* \brief Initialize repo list, call this only once after declaring pndman_repository
 * NOTE: first repository == local repository */
int pndman_repository_init(pndman_repository *repo)
{
   DEBUG("pndman repo init");
   if (!repo) return RETURN_FAIL;
   _pndman_repository_init(repo);
   strcpy(repo->name, LOCAL_DB_NAME);
   return RETURN_OK;
}

/* \brief Add new repository */
int pndman_repository_add(char *url, pndman_repository *repo)
{
   DEBUG("pndman repo add");
   if (!repo) return RETURN_FAIL;
   return _pndman_repository_add(url, repo);
}

/* \brief Sync repositories, takes device argument for synchorizing only changes beetwen remote and local
 *  If 0 is returned, call the function again. If 1 returned, everything is complete.
 *  On -1 error. */
int pndman_repository_sync(pndman_repository *repo, pndman_device *device)
{
   if (!repo) return RETURN_FAIL;
   return _pndman_repository_sync(repo, device);
}

/* \brief Free one repo */
int pndman_repository_free(pndman_repository *repo)
{
   DEBUG("pndman repo free");
   if (!repo) return RETURN_FAIL;
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
