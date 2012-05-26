#include "internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

/* \brief compare package versions, return 1 on newer, 0 otherwise
 * NOTE: lp == package to use as base, rp == package to compare against
 *       so rp > lp == 1 */
int _pndman_vercmp(pndman_version *lp, pndman_version *rp)
{
   int major, minor, release, build;
   int major2, minor2, release2, build2;
   int ret;

   /* do vanilla version checking */
   major     = strtol(lp->major, (char **) NULL, 10);
   minor     = strtol(lp->minor, (char **) NULL, 10);
   release   = strtol(lp->release, (char **) NULL, 10);
   build     = strtol(lp->build, (char **) NULL, 10);

   /* remote */
   major2    = strtol(rp->major, (char **) NULL, 10);
   minor2    = strtol(rp->minor, (char **) NULL, 10);
   release2  = strtol(rp->release, (char **) NULL, 10);
   build2    = strtol(rp->build, (char **) NULL, 10);

   ret = RETURN_FALSE;
   if (major2 > major)
      ret = RETURN_TRUE;
   else if (major2 == major && minor2 > minor)
      ret = RETURN_TRUE;
   else if (major2 == major && minor2 == minor && release2 > release)
      ret = RETURN_TRUE;
   else if (major2 == major && minor2 == minor && release2 == release && build2 > build)
      ret = RETURN_TRUE;

   if (ret == RETURN_TRUE) {
      DEBUG(PNDMAN_LEVEL_CRAP,
            "L: %d.%d.%d.%d < R: %d.%d.%d.%d",
            major, minor, release, build,
            major2, minor2, release2, build2);
   }

   return ret;
}

/* \brief Init version struct */
static void _pndman_init_version(pndman_version *ver)
{
   memset(ver, 0, sizeof(pndman_version));
   strcpy(ver->major,   "0");
   strcpy(ver->minor,   "0");
   strcpy(ver->release, "0");
   strcpy(ver->build,   "0");
   ver->type = PND_VERSION_RELEASE;
}

/* \brief Copy version struct */
static void _pndman_copy_version(pndman_version *dst, pndman_version *src)
{
   if (strlen(src->major))
      memcpy(dst->major, src->major, PNDMAN_VERSION);
   if (strlen(src->minor))
      memcpy(dst->minor, src->minor, PNDMAN_VERSION);
   if (strlen(src->release))
      memcpy(dst->release, src->release, PNDMAN_VERSION);
   if (strlen(src->build))
      memcpy(dst->build, src->build, PNDMAN_VERSION);

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

/* \brief Copy exec struct */
static void _pndman_copy_exec(pndman_exec *dst, pndman_exec *src)
{
   if (strlen(src->startdir))
      memcpy(dst->startdir, src->startdir, PNDMAN_PATH);
   if (strlen(src->command))
      memcpy(dst->command, src->command, PNDMAN_STR);
   if (strlen(src->arguments))
      memcpy(dst->arguments, src->arguments, PNDMAN_STR);

   dst->standalone   = src->standalone;
   dst->background   = src->background;
   dst->x11          = src->x11;
}

/* \brief Init author struct */
static void _pndman_init_author(pndman_author *author)
{
   memset(author, 0, sizeof(pndman_author));
}

/* \brief Copy author struct */
static void _pndman_copy_author(pndman_author *dst, pndman_author *src)
{
   if (strlen(src->name))
      memcpy(dst->name, src->name, PNDMAN_NAME);
   if (strlen(src->website))
      memcpy(dst->website, src->website, PNDMAN_STR);
   if (strlen(src->email))
      memcpy(dst->email, src->email, PNDMAN_STR);
}

/* \brief Init info struct */
static void _pndman_init_info(pndman_info *info)
{
   memset(info, 0, sizeof(pndman_info));
}

/* \brief Copy info struct */
static void _pndman_copy_info(pndman_info *dst, pndman_info *src)
{
   if (strlen(src->name))
      memcpy(dst->name, src->name, PNDMAN_NAME);
   if (strlen(src->type))
      memcpy(dst->type, src->type, PNDMAN_SHRT_STR);
   if (strlen(src->src))
      memcpy(dst->src, src->src, PNDMAN_PATH);
}

/* \brief Internal allocation of pndman_translated */
static pndman_translated* _pndman_translated_new(void)
{
   pndman_translated *t;

   if (!(t = malloc(sizeof(pndman_translated))))
      goto fail;

   /* init */
   memset(t->lang,   0, PNDMAN_SHRT_STR);
   memset(t->string, 0, PNDMAN_STR);
   t->next = NULL;

   return t;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_translated");
   return NULL;
}

/* \brief Internal copy of pndman_translated */
static pndman_translated* _pndman_copy_translated(pndman_translated *src)
{
   pndman_translated *t;

   if (!src) return NULL;
   if (!(t = malloc(sizeof(pndman_translated))))
      goto fail;

   memcpy(t->lang, src->lang, PNDMAN_SHRT_STR);
   memcpy(t->string, src->string, PNDMAN_STR);
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

   if (!(l = malloc(sizeof(pndman_license))))
      goto fail;

   /* init */
   memset(l, 0, sizeof(pndman_license));

   return l;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_license");
   return NULL;
}

/* \brief Internal copy of pndman_license */
static pndman_license* _pndman_copy_license(pndman_license *src)
{
   pndman_license *l;

   if (!src) return NULL;
   if (!(l = malloc(sizeof(pndman_license))))
      goto fail;

   memcpy(l->name, src->name, PNDMAN_SHRT_STR);
   memcpy(l->url, src->url, PNDMAN_STR);
   memcpy(l->sourcecodeurl, src->sourcecodeurl, PNDMAN_STR);
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

   if (!(p = malloc(sizeof(pndman_previewpic))))
      goto fail;

   /* init */
   memset(p, 0, sizeof(pndman_previewpic));

   return p;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_previewpic");
   return NULL;
}

/* \brief Internal copy of pndman_previewpic */
static pndman_previewpic* _pndman_copy_previewpic(pndman_previewpic *src)
{
   pndman_previewpic *p;

   if (!src) return NULL;
   if (!(p = malloc(sizeof(pndman_previewpic))))
      goto fail;

   memcpy(p->src, src->src, PNDMAN_PATH);
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

   if (!(a = malloc(sizeof(pndman_association))))
      goto fail;

   /* init */
   memset(a, 0, sizeof(pndman_association));

   return a;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_association");
   return NULL;
}

/* \brief Internal copy of pndman_assocation */
static pndman_association* _pndman_copy_association(pndman_association *src)
{
   pndman_association *a;

   if (!src) return NULL;
   if (!(a = malloc(sizeof(pndman_association))))
      goto fail;

   memcpy(a->name, src->name, PNDMAN_STR);
   memcpy(a->filetype, src->filetype, PNDMAN_SHRT_STR);
   memcpy(a->exec, src->exec, PNDMAN_STR);
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

   if (!(c = malloc(sizeof(pndman_category))))
      goto fail;

   /* init */
   memset(c, 0, sizeof(pndman_category));

   return c;

fail:
   DEBFAIL(PNDMAN_ALLOC_FAIL, "pndman_category");
   return NULL;
}

/* \brief Internal copy of pndman_category */
static pndman_category* _pndman_copy_category(pndman_category *src)
{
   pndman_category *c;

   if (!src) return NULL;
   if (!(c = malloc(sizeof(pndman_category))))
      goto fail;

   memcpy(c->main, src->main, PNDMAN_SHRT_STR);
   memcpy(c->sub, src->sub, PNDMAN_SHRT_STR);
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
   if (!(app = malloc(sizeof(pndman_application))))
      goto fail;

   /* NULL */
   memset(app, 0, sizeof(pndman_application));
   strcpy(app->icon, PNDMAN_DEFAULT_ICON);

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
   memcpy(app->id,      src->id,       PNDMAN_ID);
   memcpy(app->appdata, src->appdata,  PNDMAN_PATH);
   memcpy(app->icon,    src->icon,     PNDMAN_PATH);

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
   if (!(pnd = malloc(sizeof(pndman_package))))
      goto fail;

   /* NULL */
   memset(pnd, 0, sizeof(pndman_package));
   strcpy(pnd->icon, PNDMAN_DEFAULT_ICON);
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
   if (strlen(src->path))
      memcpy(pnd->path,    src->path,     PNDMAN_PATH);
   if (strlen(src->id))
      memcpy(pnd->id,      src->id,       PNDMAN_ID);
   if (strlen(src->url))
      memcpy(pnd->info,    src->info,     PNDMAN_STR);
   if (strlen(pnd->md5))
      memcpy(pnd->md5,     src->md5,      PNDMAN_MD5);
   if (strlen(src->url))
      memcpy(pnd->url,     src->url,      PNDMAN_STR);
   if (strlen(src->vendor))
      memcpy(pnd->vendor,  src->vendor,   PNDMAN_NAME);
   if (strlen(src->icon))
      memcpy(pnd->icon,    src->icon,     PNDMAN_PATH);
   if (strlen(src->repository))
      memcpy(pnd->repository, src->repository, PNDMAN_STR);
   if (strlen(src->mount))
      memcpy(pnd->mount,  src->mount,   PNDMAN_PATH);

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
   { tn = t->next; free(t); }

   pnd->title = NULL;
}

/* \brief Internal free of pndman_package's descriptions */
void _pndman_package_free_descriptions(pndman_package *pnd)
{
   pndman_translated  *t, *tn;

   /* free descriptions */
   t = pnd->description;
   for (; t; t = tn)
   { tn = t->next; free(t); }

   pnd->description = NULL;
}

/* \brief Internal free of pndman_package's previewpics */
void _pndman_package_free_previewpics(pndman_package *pnd)
{
   pndman_previewpic  *p, *pn;

   /* free previewpics */
   p = pnd->previewpic;
   for (; p; p = pn)
   { pn = p->next; free(p); }

   pnd->previewpic = NULL;
}

/* \brief Internal free of pndman_package's licenses */
void _pndman_package_free_licenses(pndman_package *pnd)
{
   pndman_license  *l, *ln;

   /* free licenses */
   l = pnd->license;
   for (; l; l = ln)
   { ln = l->next; free(l); }

   pnd->license = NULL;
}

/* \brief Internal free of pndman_package's categories */
void _pndman_package_free_categories(pndman_package *pnd)
{
   pndman_category  *c, *cn;

   /* free categoires */
   c = pnd->category;
   for (; c; c = cn)
   { cn = c->next; free(c); }

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
   { tn = t->next; free(t); }

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
   { tn = t->next; free(t); }

   app->description = NULL;
}

/* \brief Internal free of pndman_application */
static void _pndman_free_application(pndman_application *app)
{
   pndman_translated  *t, *tn;
   pndman_category    *c, *cn;
   pndman_license     *l, *ln;
   pndman_previewpic  *p, *pn;
   pndman_association *a, *an;

   /* should never be null */
   assert(app);

   /* free titles */
   t = app->title;
   for (; t; t = tn)
   { tn = t->next; free(t); }

   /* free descriptions */
   t = app->description;
   for (; t; t = tn)
   { tn = t->next; free(t); }

   /* free categories */
   c = app->category;
   for (; c; c = cn)
   { cn = c->next; free(c); }

   /* free licenses */
   l = app->license;
   for (; l; l = ln)
   { ln = l->next; free(l); }

   /* free previewpics */
   p = app->previewpic;
   for (; p; p = pn)
   { pn = p->next; free(p); }

   /* free associations */
   a = app->association;
   for (; a; a = an)
   { an = a->next; free(a); }

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
   pndman_translated  *t, *tn;
   pndman_license     *l, *ln;
   pndman_previewpic  *p, *pn;
   pndman_category    *c, *cn;
   pndman_package     *pp;

   /* should never be null */
   assert(pnd);

   /* this is no longer a valid update */
   if (pnd->update) pnd->update->update = NULL;

   /* free titles */
   t = pnd->title;
   for (; t; t = tn)
   { tn = t->next; free(t); }

   /* free descriptions */
   t = pnd->description;
   for (; t; t = tn)
   { tn = t->next; free(t); }

   /* free licenses */
   l = pnd->license;
   for (; l; l = ln)
   { ln = l->next; free(l); }

   /* free previewpics */
   p = pnd->previewpic;
   for (; p; p = pn)
   { pn = p->next; free(p); }


   /* free categories */
   c = pnd->category;
   for (; c; c = cn)
   { cn = c->next; free(c); }

   /* free applications */
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

   /* get md5 */
   if (!(md5 = _pndman_md5(pnd->path)))
      return NULL;

   /* store it in pnd */
   strncpy(pnd->md5, md5, PNDMAN_MD5);
   free(md5);

   return pnd->md5;
}

/* vim: set ts=8 sw=3 tw=0 :*/
