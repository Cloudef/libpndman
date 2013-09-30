#include "internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#  include <malloc/malloc.h>
#else
#  include <malloc.h>
#endif

/* \brief compare package versions, return 1 on newer, 0 otherwise
 * NOTE: lp == package to use as base, rp == package to compare against
 *       so rp > lp == 1 */
int _pndman_vercmp(pndman_version *lp, pndman_version *rp)
{
   char *l, *r;

   l = lp->major, r = rp->major;
   while (*l && *l == '0') ++l;
   while (*r && *r == '0') ++r;
   if (strlen(l) < strlen(r)) return RETURN_TRUE;
   else if (strlen(l) > strlen(r)) return RETURN_FALSE;
   for (; *l && *r; ++l, ++r) {
      if (*r > *l) return RETURN_TRUE;
      else if (*r < *l) return RETURN_FALSE;
   }

   l = lp->minor, r = rp->minor;
   while (*l && *l == '0') ++l;
   while (*r && *r == '0') ++r;
   if (strlen(l) < strlen(r)) return RETURN_TRUE;
   else if (strlen(l) > strlen(r)) return RETURN_FALSE;
   for (; *l && *r; ++l, ++r) {
      if (*r > *l) return RETURN_TRUE;
      else if (*r < *l) return RETURN_FALSE;
   }

   l = lp->release, r = rp->release;
   while (*l && *l == '0') ++l;
   while (*r && *r == '0') ++r;
   if (strlen(l) < strlen(r)) return RETURN_TRUE;
   else if (strlen(l) > strlen(r)) return RETURN_FALSE;
   for (; *l && *r; ++l, ++r) {
      if (*r > *l) return RETURN_TRUE;
      else if (*r < *l) return RETURN_FALSE;
   }

   l = lp->build, r = rp->build;
   while (*l && *l == '0') ++l;
   while (*r && *r == '0') ++r;
   if (strlen(l) < strlen(r)) return RETURN_TRUE;
   else if (strlen(l) > strlen(r)) return RETURN_FALSE;
   for (; *l && *r; ++l, ++r) {
      if (*r > *l) return RETURN_TRUE;
      else if (*r < *l) return RETURN_FALSE;
   }

   return RETURN_FALSE;
}

/* \brief get full path of PND */
char* _pndman_pnd_get_path(pndman_package *pnd)
{
   char *path;
   if (!pnd->mount || !pnd->path) return NULL;
   int size = snprintf(NULL, 0, "%s/%s", pnd->mount, pnd->path)+1;
   if (!(path = malloc(size))) return NULL;
   sprintf(path, "%s/%s", pnd->mount, pnd->path);
   return path;
}

/* \brief Init version struct */
static void _pndman_init_version(pndman_version *ver)
{
   memset(ver, 0, sizeof(pndman_version));
   ver->major = strdup("0");
   ver->minor = strdup("0");
   ver->release = strdup("0");
   ver->build = strdup("0");
   ver->type = PND_VERSION_RELEASE;
}

void _pndman_free_version(pndman_version *ver)
{
   IFDO(free, ver->major);
   IFDO(free, ver->minor);
   IFDO(free, ver->release);
   IFDO(free, ver->build);
}

/* \brief Copy version struct */
void _pndman_copy_version(pndman_version *dst, pndman_version *src)
{
   if (src->major) {
      IFDO(free, dst->major);
      dst->major = strdup(src->major);
   }
   if (src->minor) {
      IFDO(free, dst->minor);
      dst->minor = strdup(src->minor);
   }
   if (src->release) {
      IFDO(free, dst->release);
      dst->release = strdup(src->release);
   }
   if (src->build) {
      IFDO(free, dst->build);
      dst->build = strdup(src->build);
   }
   dst->type = src->type;
}

/* \brief Init exec struct */
static void _pndman_init_exec(pndman_exec *exec)
{
   memset(exec, 0, sizeof(pndman_exec));
   exec->standalone  = 1;
   exec->background  = 0;
   exec->x11         = PND_EXEC_IGNORE;
}

static void _pndman_free_exec(pndman_exec *exec)
{
   IFDO(free, exec->startdir);
   IFDO(free, exec->command);
   IFDO(free, exec->arguments);
}

/* \brief Copy exec struct */
static void _pndman_copy_exec(pndman_exec *dst, pndman_exec *src)
{
   if (src->startdir) {
      IFDO(free, dst->startdir);
      dst->startdir = strdup(src->startdir);
   }
   if (src->command) {
      IFDO(free, dst->command);
      dst->command = strdup(src->command);
   }
   if (src->arguments) {
      IFDO(free, dst->arguments);
      dst->arguments = strdup(src->arguments);
   }
   dst->standalone   = src->standalone;
   dst->background   = src->background;
   dst->x11          = src->x11;
}

/* \brief Init author struct */
static void _pndman_init_author(pndman_author *author)
{
   memset(author, 0, sizeof(pndman_author));
}

static void _pndman_free_author(pndman_author *author)
{
   IFDO(free, author->name);
   IFDO(free, author->website);
   IFDO(free, author->email);
}

/* \brief Copy author struct */
static void _pndman_copy_author(pndman_author *dst, pndman_author *src)
{
   if (src->name) {
      IFDO(free, dst->name);
      dst->name = strdup(src->name);
   }
   if (src->website) {
      IFDO(free, dst->website);
      dst->website = strdup(src->website);
   }
   if (src->email) {
      IFDO(free, dst->email);
      dst->email = strdup(src->email);
   }
}

/* \brief Init info struct */
static void _pndman_init_info(pndman_info *info)
{
   memset(info, 0, sizeof(pndman_info));
}

static void _pndman_free_info(pndman_info *info)
{
   IFDO(free, info->name);
   IFDO(free, info->type);
   IFDO(free, info->src);
}

/* \brief Copy info struct */
static void _pndman_copy_info(pndman_info *dst, pndman_info *src)
{
   if (src->name) {
      IFDO(free, dst->name);
      dst->name = strdup(src->name);
   }
   if (src->type) {
      IFDO(free, dst->type);
      dst->type = strdup(src->type);
   }
   if (src->src) {
      IFDO(free, dst->src);
      dst->src = strdup(src->src);
   }
}

/* \brief Internal allocation of pndman_translated */
static pndman_translated* _pndman_translated_new(void)
{
   pndman_translated *t;

   if (!(t = calloc(1, sizeof(pndman_translated))))
      goto fail;
   return t;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_translated");
   return NULL;
}

static void _pndman_free_translated(pndman_translated *t)
{
   IFDO(free, t->lang);
   IFDO(free, t->string);
}

/* \brief Internal copy of pndman_translated */
static pndman_translated* _pndman_copy_translated(pndman_translated *src)
{
   pndman_translated *t;

   if (!src) return NULL;
   if (!(t = calloc(1, sizeof(pndman_translated))))
      goto fail;

   if (src->lang) t->lang = strdup(src->lang);
   if (src->string) t->string = strdup(src->string);
   t->next = _pndman_copy_translated(src->next);

   return t;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_translated");
   return NULL;
}

/* \brief Internal allocation of pndman_license */
static pndman_license* _pndman_license_new(void)
{
   pndman_license *l;

   if (!(l = calloc(1, sizeof(pndman_license))))
      goto fail;
   return l;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_license");
   return NULL;
}

static void _pndman_free_license(pndman_license *l)
{
   IFDO(free, l->name);
   IFDO(free, l->url);
   IFDO(free, l->sourcecodeurl);
}

/* \brief Internal copy of pndman_license */
static pndman_license* _pndman_copy_license(pndman_license *src)
{
   pndman_license *l;

   if (!src) return NULL;
   if (!(l = calloc(1, sizeof(pndman_license))))
      goto fail;

   if (src->name) l->name = strdup(src->name);
   if (src->url) l->url = strdup(src->url);
   if (src->sourcecodeurl) l->sourcecodeurl = strdup(src->sourcecodeurl);
   l->next = _pndman_copy_license(src->next);

   return l;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_license");
   return NULL;
}

/* \brief Internal allocation of pndman_previewpic */
static pndman_previewpic* _pndman_previewpic_new(void)
{
   pndman_previewpic *p;

   if (!(p = calloc(1, sizeof(pndman_previewpic))))
      goto fail;
   return p;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_previewpic");
   return NULL;
}

static void _pndman_free_previewpic(pndman_previewpic *pic)
{
   IFDO(free, pic->src);
}

/* \brief Internal copy of pndman_previewpic */
static pndman_previewpic* _pndman_copy_previewpic(pndman_previewpic *src)
{
   pndman_previewpic *p;

   if (!src) return NULL;
   if (!(p = calloc(1, sizeof(pndman_previewpic))))
      goto fail;

   if (src->src) p->src = strdup(src->src);
   p->next = _pndman_copy_previewpic(src->next);
   return p;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_previewpic");
   return NULL;
}

/* \brief Internal allocation of pndman_association */
static pndman_association* _pndman_association_new(void)
{
   pndman_association *a;

   if (!(a = calloc(1, sizeof(pndman_association))))
      goto fail;
   return a;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_association");
   return NULL;
}

static void _pndman_free_associaton(pndman_association *assoc)
{
   IFDO(free, assoc->name);
   IFDO(free, assoc->filetype);
   IFDO(free, assoc->exec);
}

/* \brief Internal copy of pndman_assocation */
static pndman_association* _pndman_copy_association(pndman_association *src)
{
   pndman_association *a;

   if (!src) return NULL;
   if (!(a = calloc(1, sizeof(pndman_association))))
      goto fail;

   if (src->name) a->name = strdup(src->name);
   if (src->filetype) a->filetype = strdup(src->filetype);
   if (src->exec) a->exec = strdup(src->exec);
   a->next = _pndman_copy_association(src->next);
   return a;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_association");
   return NULL;
}

/* \brief Internal allocation of pndman_category */
static pndman_category* _pndman_category_new(void)
{
   pndman_category *c;

   if (!(c = calloc(1, sizeof(pndman_category))))
      goto fail;
   return c;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_category");
   return NULL;
}

static void _pndman_free_category(pndman_category *cat)
{
   IFDO(free, cat->main);
   IFDO(free, cat->sub);
}

/* \brief Internal copy of pndman_category */
static pndman_category* _pndman_copy_category(pndman_category *src)
{
   pndman_category *c;

   if (!src) return NULL;
   if (!(c = calloc(1, sizeof(pndman_category))))
      goto fail;

   if (src->main) c->main = strdup(src->main);
   if (src->sub) c->sub = strdup(src->sub);
   c->next = _pndman_copy_category(src->next);
   return c;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_category");
   return NULL;
}

/* \brief Allocate new pndman_application */
static pndman_application* _pndman_new_application(void)
{
   pndman_application *app;

   /* allocate */
   if (!(app = calloc(1, sizeof(pndman_application))))
      goto fail;

   /* init */
   app->icon = strdup(PNDMAN_DEFAULT_ICON);
   _pndman_init_author(&app->author);
   _pndman_init_version(&app->osversion);
   _pndman_init_version(&app->version);
   _pndman_init_exec(&app->exec);
   _pndman_init_info(&app->info);

   return app;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_application");
   return NULL;
}

/* \brief Copy all applications */
static pndman_application* _pndman_copy_application(pndman_application *src)
{
   pndman_application *app;

   if (!src) return NULL;
   if (!(app = _pndman_new_application()))
      goto fail;

   /* copy */
   if (src->id) {
      IFDO(free, app->id);
      app->id = strdup(src->id);
   }
   if (src->appdata) {
      IFDO(free, app->appdata);
      app->appdata = strdup(src->appdata);
   }
   if (src->icon) {
      IFDO(free, app->icon);
      app->icon = strdup(src->icon);
   }

   _pndman_copy_author(&app->author, &src->author);
   _pndman_copy_version(&app->osversion, &src->osversion);
   _pndman_copy_version(&app->version, &src->version);
   _pndman_copy_exec(&app->exec, &src->exec);
   _pndman_copy_info(&app->info, &src->info),

   app->title        = _pndman_copy_translated(src->title);
   app->description  = _pndman_copy_translated(src->description);
   app->category     = _pndman_copy_category(src->category);
   app->previewpic   = _pndman_copy_previewpic(src->previewpic);
   app->license      = _pndman_copy_license(src->license);
   app->association  = _pndman_copy_association(src->association);

   app->frequency    = src->frequency;
   app->next         = _pndman_copy_application(src->next);

   return app;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_application");
   return NULL;
}

/* \brief Allocate new pndman_package */
pndman_package* _pndman_new_pnd(void)
{
   pndman_package *pnd;

   /* allocate */
   if (!(pnd = calloc(1, sizeof(pndman_package))))
      goto fail;

   /* init */
   pnd->icon = strdup(PNDMAN_DEFAULT_ICON);
   _pndman_init_author(&pnd->author);
   _pndman_init_version(&pnd->version);

   return pnd;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_package");
   return NULL;
}

/* \brief Copy pndman_package, doesn't copy next && next_installed pointer */
int _pndman_copy_pnd(pndman_package *pnd, pndman_package *src)
{
   assert(pnd && src);

   /* copy */
   if (src->path) {
      IFDO(free, pnd->path);
      pnd->path = strdup(src->path);
   }
   if (src->id) {
      IFDO(free, pnd->id);
      pnd->id = strdup(src->id);
   }
   if (src->info) {
      IFDO(free, pnd->info);
      pnd->info = strdup(src->info);
   }
   if (src->md5) {
      IFDO(free, pnd->md5);
      pnd->md5 = strdup(src->md5);
   }
   if (src->url) {
      IFDO(free, pnd->url);
      pnd->url = strdup(src->url);
   }
   if (src->vendor) {
      IFDO(free, pnd->vendor);
      pnd->vendor = strdup(src->vendor);
   }
   if (src->icon) {
      IFDO(free, pnd->icon);
      pnd->icon = strdup(src->icon);
   }
   if (src->repository) {
      IFDO(free, pnd->repository);
      pnd->repository = strdup(src->repository);
   }
   if (src->mount) {
      IFDO(free, pnd->mount);
      pnd->mount = strdup(src->mount);
   }

   _pndman_copy_author(&pnd->author, &src->author);
   _pndman_copy_version(&pnd->version, &src->version);

   /* TODO: merge non existant ones instead? */
   if (!pnd->app)
      pnd->app          = _pndman_copy_application(src->app);

   if (!pnd->title)
      pnd->title        = _pndman_copy_translated(src->title);
   if (!pnd->description)
      pnd->description  = _pndman_copy_translated(src->description);

   if (!pnd->license)
      pnd->license      = _pndman_copy_license(src->license);

   if (!pnd->previewpic)
      pnd->previewpic   = _pndman_copy_previewpic(src->previewpic);

   if (!pnd->category)
      pnd->category     = _pndman_copy_category(src->category);

   if (src->size)
      pnd->size            = src->size;
   if (src->modified_time)
      pnd->modified_time   = src->modified_time;
   if (src->local_modified_time)
      pnd->local_modified_time = src->local_modified_time;
   if (src->rating)
      pnd->rating          = src->rating;
   if (src->update)
      pnd->update          = src->update;

   return RETURN_OK;
}

/* \brief Internal free of pndman_packages's titles */
void _pndman_package_free_titles(pndman_package *pnd)
{
   pndman_translated  *t, *tn;

   /* free titles */
   t = pnd->title;
   for (; t; t = tn)
   { tn = t->next; _pndman_free_translated(t); free(t); }

   pnd->title = NULL;
}

/* \brief Internal free of pndman_package's descriptions */
void _pndman_package_free_descriptions(pndman_package *pnd)
{
   pndman_translated  *t, *tn;

   /* free descriptions */
   t = pnd->description;
   for (; t; t = tn)
   { tn = t->next; _pndman_free_translated(t); free(t); }

   pnd->description = NULL;
}

/* \brief Internal free of pndman_package's previewpics */
void _pndman_package_free_previewpics(pndman_package *pnd)
{
   pndman_previewpic  *p, *pn;

   /* free previewpics */
   p = pnd->previewpic;
   for (; p; p = pn)
   { pn = p->next; _pndman_free_previewpic(p); free(p); }

   pnd->previewpic = NULL;
}

/* \brief Internal free of pndman_package's licenses */
void _pndman_package_free_licenses(pndman_package *pnd)
{
   pndman_license  *l, *ln;

   /* free licenses */
   l = pnd->license;
   for (; l; l = ln)
   { ln = l->next; _pndman_free_license(l); free(l); }

   pnd->license = NULL;
}

/* \brief Internal free of pndman_package's categories */
void _pndman_package_free_categories(pndman_package *pnd)
{
   pndman_category  *c, *cn;

   /* free categoires */
   c = pnd->category;
   for (; c; c = cn)
   { cn = c->next; _pndman_free_category(c);; free(c); }

   pnd->category = NULL;
}

/* \brief Internal free of pndman_application's titles
 * NOTE: Used by PXML parser */
void _pndman_application_free_titles(pndman_application *app)
{
   pndman_translated  *t, *tn;

   /* free titles */
   t = app->title;
   for (; t; t = tn)
   { tn = t->next; _pndman_free_translated(t); free(t); }

   app->title = NULL;
}

/* \brief Internal free of pndman_application's descriptions
 * NOTE: Used by PXML parser */
void _pndman_application_free_descriptions(pndman_application *app)
{
   pndman_translated  *t, *tn;

   /* free titles */
   t = app->description;
   for (; t; t = tn)
   { tn = t->next; _pndman_free_translated(t); free(t); }

   app->description = NULL;
}

/* \brief Internal free of pndman_applications's categories */
void _pndman_application_free_categories(pndman_application *app)
{
   pndman_category  *c, *cn;

   /* free categoires */
   c = app->category;
   for (; c; c = cn)
   { cn = c->next; _pndman_free_category(c); free(c); }

   app->category = NULL;
}

/* \brief Internal free of pndman_application's previewpics */
void _pndman_application_free_previewpics(pndman_application *app)
{
   pndman_previewpic  *p, *pn;

   /* free previewpics */
   p = app->previewpic;
   for (; p; p = pn)
   { pn = p->next; _pndman_free_previewpic(p); free(p); }

   app->previewpic = NULL;
}

/* \brief Internal free of pndman_application's licenses */
void _pndman_application_free_licenses(pndman_application *app)
{
   pndman_license  *l, *ln;

   /* free licenses */
   l = app->license;
   for (; l; l = ln)
   { ln = l->next; _pndman_free_license(l); free(l); }

   app->license = NULL;
}

/* \brief Internal free of pndman_application's associations */
void _pndman_application_free_associations(pndman_application *app)
{
   pndman_association  *a, *an;

   /* free licenses */
   a = app->association;
   for (; a; a = an)
   { an = a->next; _pndman_free_associaton(a); free(a); }

   app->association = NULL;
}

/* \brief Internal free of pndman_application */
static void _pndman_free_application(pndman_application *app)
{
   /* should never be null */
   assert(app);

   IFDO(free, app->id);
   IFDO(free, app->appdata);
   IFDO(free, app->icon);
   _pndman_free_author(&app->author);
   _pndman_free_version(&app->osversion);
   _pndman_free_version(&app->version);
   _pndman_free_exec(&app->exec);
   _pndman_free_info(&app->info);
   _pndman_application_free_titles(app);
   _pndman_application_free_descriptions(app);
   _pndman_application_free_categories(app);
   _pndman_application_free_licenses(app);
   _pndman_application_free_previewpics(app);
   _pndman_application_free_associations(app);

   /* free this */
   free(app);
}

/* \brief Internal free of pndman_application's */
void _pndman_package_free_applications(pndman_package *pnd)
{
   pndman_application *a, *an;
   assert(pnd);
   a = pnd->app;
   for (; a; a = an)
   { an = a->next; _pndman_free_application(a); }
   pnd->app = NULL;
}

/* \brief Internal free of pndman_package */
pndman_package* _pndman_free_pnd(pndman_package *pnd)
{
   pndman_package     *pp;

   /* should never be null */
   assert(pnd);

   IFDO(free, pnd->path);
   IFDO(free, pnd->id);
   IFDO(free, pnd->icon);
   IFDO(free, pnd->info);
   IFDO(free, pnd->md5);
   IFDO(free, pnd->url);
   IFDO(free, pnd->vendor);
   IFDO(free, pnd->repository);
   IFDO(free, pnd->mount);

   /* this is no longer a valid update */
   if (pnd->update) pnd->update->update = NULL;

   _pndman_free_author(&pnd->author);
   _pndman_free_version(&pnd->version);
   _pndman_package_free_titles(pnd);
   _pndman_package_free_descriptions(pnd);
   _pndman_package_free_licenses(pnd);
   _pndman_package_free_previewpics(pnd);
   _pndman_package_free_categories(pnd);
   _pndman_package_free_applications(pnd);

   /* store next installed for return */
   pp = pnd->next_installed;

   /* free this */
   free(pnd);

   /* return next installed */
   return pp;
}

/* \brief Internal allocation of title for pndman_package */
pndman_translated* _pndman_package_new_title(pndman_package *pnd)
{
   pndman_translated *t;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->title) t = pnd->title = _pndman_translated_new();
   else {
      /* find last */
      t = pnd->title;
      for (; t->next; t = t->next);
      t->next = _pndman_translated_new();
      t = t->next;
   }

   return t;
}

/* \brief Internal allocation of description for pndman_package */
pndman_translated* _pndman_package_new_description(pndman_package *pnd)
{
   pndman_translated *t;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->description) t = pnd->description = _pndman_translated_new();
   else {
      /* find last */
      t = pnd->description;
      for (; t->next; t = t->next);
      t->next = _pndman_translated_new();
      t = t->next;
   }

   return t;
}

 /* \brief Internal allocation of license for pndman_package */
pndman_license* _pndman_package_new_license(pndman_package *pnd)
{
   pndman_license *l;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->license) l = pnd->license = _pndman_license_new();
   else {
      /* find last */
      l = pnd->license;
      for (; l->next; l = l->next);
      l->next = _pndman_license_new();
      l = l->next;
   }

   return l;
}

/* \brief Internal allocation of previewpic for pndman_package */
pndman_previewpic* _pndman_package_new_previewpic(pndman_package *pnd)
{
   pndman_previewpic *p;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->previewpic) p = pnd->previewpic = _pndman_previewpic_new();
   else {
      /* find last */
      p = pnd->previewpic;
      for (; p->next; p = p->next);
      p->next = _pndman_previewpic_new();
      p = p->next;
   }

   return p;
}

/* \brief Internal allocation of category for pndman_package */
pndman_category* _pndman_package_new_category(pndman_package *pnd)
{
   pndman_category *c;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->category) c = pnd->category = _pndman_category_new();
   else {
      /* find last */
      c = pnd->category;
      for (; c->next; c = c->next);
      c->next = _pndman_category_new();
      c = c->next;
   }

   return c;
}

/* \brief Internal allocation of application for pndman_package */
pndman_application* _pndman_package_new_application(pndman_package *pnd)
{
   pndman_application *app;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->app) app = pnd->app = _pndman_new_application();
   else {
      /* find last */
      app = pnd->app;
      for (; app->next; app = app->next);
      app->next = _pndman_new_application();
      app = app->next;
   }

   return app;
}

/* \brief Internal allocation of title for pndman_application */
pndman_translated* _pndman_application_new_title(pndman_application *app)
{
   pndman_translated *t;

   /* should not be null */
   assert(app);

   /* how to allocate? */
   if (!app->title) t = app->title = _pndman_translated_new();
   else {
      /* find last */
      t = app->title;
      for (; t->next; t = t->next);
      t->next = _pndman_translated_new();
      t = t->next;
   }

   return t;
}

/* \brief Internal allocation of description for pndman_application */
pndman_translated* _pndman_application_new_description(pndman_application *app)
{
   pndman_translated *t;

   /* should not be null */
   assert(app);

   /* how to allocate? */
   if (!app->description) t = app->description = _pndman_translated_new();
   else {
      /* find last */
      t = app->description;
      for (; t->next; t = t->next);
      t->next = _pndman_translated_new();
      t = t->next;
   }

   return t;
}

/* \brief Internal allocation of license for pndman_application */
pndman_license* _pndman_application_new_license(pndman_application *app)
{
   pndman_license *l;

   /* should not be null */
   assert(app);

   /* how to allocate? */
   if (!app->license) l = app->license = _pndman_license_new();
   else {
      /* find last */
      l = app->license;
      for (; l->next; l = l->next);
      l->next = _pndman_license_new();
      l = l->next;
   }

   return l;
}

/* \brief Internal allocation of previewpic for pndman_application */
pndman_previewpic* _pndman_application_new_previewpic(pndman_application *app)
{
   pndman_previewpic *p;

   /* should not be null */
   assert(app);

   /* how to allocate? */
   if (!app->previewpic) p = app->previewpic = _pndman_previewpic_new();
   else {
      /* find last */
      p = app->previewpic;
      for (; p->next; p = p->next);
      p->next = _pndman_previewpic_new();
      p = p->next;
   }

   return p;
}

/* \brief Internal allocation of assocation for pndman_application */
pndman_association* _pndman_application_new_association(pndman_application *app)
{
   pndman_association *a;

   /* should not be null */
   assert(app);

   /* how to allocate? */
   if (!app->association) a = app->association = _pndman_association_new();
   else {
      /* find last */
      a = app->association;
      for (; a->next; a = a->next);
      a->next = _pndman_association_new();
      a = a->next;
   }

   return a;
}

/* \brief Internal allocation of category for pndman_application */
pndman_category* _pndman_application_new_category(pndman_application *app)
{
   pndman_category *c;

   /* should not be null */
   assert(app);

   /* how to allocate? */
   if (!app->category) c = app->category = _pndman_category_new();
   else {
      /* find last */
      c = app->category;
      for (; c->next; c = c->next);
      c->next = _pndman_category_new();
      c = c->next;
   }

   return c;
}

/* API */

/* \brief calculate new md5 for the PND,
 * Use this if you don't have md5 in pnd or want to recalculate */
const char* pndman_package_fill_md5(pndman_package *pnd)
{
   char *md5;
   if (!pnd) return NULL;

   /* get full path */
   char *path = _pndman_pnd_get_path(pnd);
   if (!path) return NULL;

   /* get md5 */
   md5 = _pndman_md5(path); free(path);
   if (!md5) return NULL;

   /* store it in pnd */
   IFDO(free, pnd->md5);
   pnd->md5 = strdup(md5);
   free(md5);
   return pnd->md5;
}

/* vim: set ts=8 sw=3 tw=0 :*/
