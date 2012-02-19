#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "pndman.h"
#include "package.h"

/* \brief Init version struct */
static void _pndman_init_version(pndman_version *ver)
{
   memset(ver->major,  0, PND_VER);
   memset(ver->minor,  0, PND_VER);
   memset(ver->release,0, PND_VER);
   memset(ver->build,  0, PND_VER);
   strcpy(ver->major,   "0");
   strcpy(ver->minor,   "0");
   strcpy(ver->release, "0");
   strcpy(ver->build,   "0");
   ver->type = PND_VERSION_RELEASE;
}

/* \brief Copy version struct */
static void _pndman_copy_version(pndman_version *dst, pndman_version *src)
{
   memcpy(dst->major, src->major, PND_VER);
   memcpy(dst->minor, src->minor, PND_VER);
   memcpy(dst->release, src->release, PND_VER);
   memcpy(dst->build, src->build, PND_VER);
   dst->type = src->type;
}

/* \brief Init exec struct */
static void _pndman_init_exec(pndman_exec *exec)
{
   memset(exec->startdir,   0, PND_PATH);
   memset(exec->command,    0, PND_STR);
   memset(exec->arguments,  0, PND_STR);

   exec->standalone  = 1;
   exec->background  = 0;
   exec->x11         = PND_EXEC_IGNORE;
}

/* \brief Copy exec struct */
static void _pndman_copy_exec(pndman_exec *dst, pndman_exec *src)
{
   memcpy(dst->startdir, src->startdir, PND_PATH);
   memcpy(dst->command, src->command, PND_STR);
   memcpy(dst->arguments, src->arguments, PND_STR);

   dst->standalone   = src->standalone;
   dst->background   = src->background;
   dst->x11          = src->x11;
}

/* \brief Init author struct */
static void _pndman_init_author(pndman_author *author)
{
   memset(author->name,    0, PND_NAME);
   memset(author->website, 0, PND_STR);
   memset(author->email,   0, PND_STR);
}

/* \brief Copy author struct */
static void _pndman_copy_author(pndman_author *dst, pndman_author *src)
{
   memcpy(dst->name, src->name, PND_NAME);
   memcpy(dst->website, src->website, PND_STR);
   memcpy(dst->email, src->email, PND_STR);
}

/* \brief Init info struct */
static void _pndman_init_info(pndman_info *info)
{
   memset(info->name,   0, PND_NAME);
   memset(info->type,   0, PND_SHRT_STR);
   memset(info->src,    0, PND_PATH);
}

/* \brief Copy info struct */
static void _pndman_copy_info(pndman_info *dst, pndman_info *src)
{
   memcpy(dst->name, src->name, PND_NAME);
   memcpy(dst->type, src->type, PND_SHRT_STR);
   memcpy(dst->src, src->src, PND_PATH);
}

/* \brief Internal allocation of pndman_translated */
static pndman_translated* _pndman_translated_new(void)
{
   pndman_translated *t;

   t = malloc(sizeof(pndman_translated));
   if (!t) return NULL;

   /* init */
   memset(t->lang,   0, PND_SHRT_STR);
   memset(t->string, 0, PND_STR);
   t->next = NULL;

   return t;
}

/* \brief Internal copy of pndman_translated */
static pndman_translated* _pndman_copy_translated(pndman_translated *src)
{
   pndman_translated *t;

   if (!src) return NULL;
   t = malloc(sizeof(pndman_translated));
   if (!t) return NULL;

   memcpy(t->lang, src->lang, PND_SHRT_STR);
   memcpy(t->string, src->string, PND_STR);
   t->next = _pndman_copy_translated(src->next);

   return t;
}

/* \brief Internal allocation of pndman_license */
static pndman_license* _pndman_license_new(void)
{
   pndman_license *l;

   l = malloc(sizeof(pndman_license));
   if (!l) return NULL;

   /* init */
   memset(l->name,            0, PND_SHRT_STR);
   memset(l->url,             0, PND_STR);
   memset(l->sourcecodeurl,   0, PND_STR);
   l->next = NULL;

   return l;
}

/* \brief Internal copy of pndman_license */
static pndman_license* _pndman_copy_license(pndman_license *src)
{
   pndman_license *l;

   if (!src) return NULL;
   l = malloc(sizeof(pndman_license));
   if (!l) return NULL;

   memcpy(l->name, src->name, PND_SHRT_STR);
   memcpy(l->url, src->url, PND_STR);
   memcpy(l->sourcecodeurl, src->sourcecodeurl, PND_STR);
   l->next = _pndman_copy_license(src->next);

   return l;
}

/* \brief Internal allocation of pndman_previewpic */
static pndman_previewpic* _pndman_previewpic_new(void)
{
   pndman_previewpic *p;

   p = malloc(sizeof(pndman_previewpic));
   if (!p) return NULL;

   /* init */
   memset(p->src,   0, PND_PATH);
   p->next = NULL;

   return p;
}

/* \brief Internal copy of pndman_previewpic */
static pndman_previewpic* _pndman_copy_previewpic(pndman_previewpic *src)
{
   pndman_previewpic *p;

   if (!src) return NULL;
   p = malloc(sizeof(pndman_previewpic));
   if (!p) return NULL;

   memcpy(p->src, src->src, PND_PATH);
   p->next = _pndman_copy_previewpic(src->next);

   return p;
}

/* \brief Internal allocation of pndman_association */
static pndman_association* _pndman_association_new(void)
{
   pndman_association *a;

   a = malloc(sizeof(pndman_association));
   if (!a) return NULL;

   /* init */
   memset(a->name,            0, PND_STR);
   memset(a->filetype,        0, PND_SHRT_STR);
   memset(a->exec,            0, PND_STR);
   a->next = NULL;

   return a;
}

/* \brief Internal copy of pndman_assocation */
static pndman_association* _pndman_copy_association(pndman_association *src)
{
   pndman_association *a;

   if (!src) return NULL;
   a = malloc(sizeof(pndman_association));
   if (!a) return NULL;

   memcpy(a->name, src->name, PND_STR);
   memcpy(a->filetype, src->filetype, PND_SHRT_STR);
   memcpy(a->exec, src->exec, PND_STR);
   a->next = _pndman_copy_association(src->next);

   return a;
}

/* \brief Internal allocation of pndman_category */
static pndman_category* _pndman_category_new(void)
{
   pndman_category *c;

   c = malloc(sizeof(pndman_category));
   if (!c) return NULL;

   /* init */
   memset(c->main,   0, PND_SHRT_STR);
   memset(c->sub,    0, PND_SHRT_STR);
   c->next = NULL;

   return c;
}

/* \brief Internal copy of pndman_category */
static pndman_category* _pndman_copy_category(pndman_category *src)
{
   pndman_category *c;

   if (!src) return NULL;
   c = malloc(sizeof(pndman_category));
   if (!c) return NULL;

   memcpy(c->main, src->main, PND_SHRT_STR);
   memcpy(c->sub, src->sub, PND_SHRT_STR);
   c->next = _pndman_copy_category(src->next);

   return c;
}

/* \brief Allocate new pndman_application */
static pndman_application* _pndman_new_application(void)
{
   pndman_application *app;

   /* allocate */
   app = malloc(sizeof(pndman_application));
   if (!app)
      return NULL;

   /* NULL */
   memset(app->id,      0, PND_ID);
   memset(app->appdata, 0, PND_PATH);

   strcpy(app->icon, PND_DEFAULT_ICON);

   _pndman_init_author(&app->author);
   _pndman_init_version(&app->osversion);
   _pndman_init_version(&app->version);
   _pndman_init_exec(&app->exec);
   _pndman_init_info(&app->info);

   app->title        = NULL;
   app->description  = NULL;
   app->category     = NULL;
   app->previewpic   = NULL;
   app->license      = NULL;
   app->association  = NULL;
   app->frequency    = 0;

   app->next = NULL;

   return app;
}

/* \brief Copy all applications */
static pndman_application* _pndman_copy_application(pndman_application *src)
{
   pndman_application *app;

   if (!src) return NULL;
   app = malloc(sizeof(pndman_application));
   if (!app)
      return NULL;

   /* copy */
   memcpy(app->id,      src->id,       PND_ID);
   memcpy(app->appdata, src->appdata,  PND_PATH);
   memcpy(app->icon,    src->icon,     PND_PATH);

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
}

/* \brief Allocate new pndman_package */
pndman_package* _pndman_new_pnd(void)
{
   pndman_package *pnd;

   /* allocate */
   pnd = malloc(sizeof(pndman_package));
   if (!pnd)
      return NULL;

   /* NULL */
   memset(pnd->path,    0, PND_PATH);
   memset(pnd->id,      0, PND_ID);
   memset(pnd->info,    0, PND_INFO);
   memset(pnd->md5,     0, PND_MD5);
   memset(pnd->url,     0, PND_STR);
   memset(pnd->vendor,  0, PND_NAME);

   //memset(pnd->icon, 0, PND_PATH-1);
   strcpy(pnd->icon, PND_DEFAULT_ICON);

   _pndman_init_author(&pnd->author);
   _pndman_init_version(&pnd->version);

   pnd->app             = NULL;
   pnd->title           = NULL;
   pnd->description     = NULL;
   pnd->license         = NULL;
   pnd->previewpic      = NULL;
   pnd->category        = NULL;

   pnd->size            = 0;
   pnd->modified_time   = 0;
   pnd->rating          = 0;
   pnd->flags           = 0;
   pnd->next            = NULL;
   pnd->next_installed  = NULL;

   return pnd;
}

/* \brief Copy pndman_package, doesn't copy next && next_installed pointer */
int _pndman_copy_pnd(pndman_package *pnd, pndman_package *src)
{
   assert(pnd && src);

   /* copy */
   memcpy(pnd->path,    src->path,     PND_PATH);
   memcpy(pnd->id,      src->id,       PND_ID);
   memcpy(pnd->info,    src->info,     PND_INFO);
   memcpy(pnd->md5,     src->md5,      PND_MD5);
   memcpy(pnd->url,     src->url,      PND_STR);
   memcpy(pnd->vendor,  src->vendor,   PND_NAME);
   memcpy(pnd->icon,    src->icon,     PND_PATH);

   _pndman_copy_author(&pnd->author, &src->author);
   _pndman_copy_version(&pnd->version, &src->version);
   pnd->app          = _pndman_copy_application(src->app);
   pnd->title        = _pndman_copy_translated(src->title);
   pnd->description  = _pndman_copy_translated(src->description);
   pnd->license      = _pndman_copy_license(src->license);
   pnd->previewpic   = _pndman_copy_previewpic(src->previewpic);
   pnd->category     = _pndman_copy_category(src->category);

   pnd->size            = src->size;
   pnd->modified_time   = src->modified_time;
   pnd->rating          = src->rating;
   pnd->flags           = src->flags;

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

/* \brief Internal free of pndman_package */
void _pndman_free_pnd(pndman_package *pnd)
{
   pndman_translated  *t, *tn;
   pndman_license     *l, *ln;
   pndman_previewpic  *p, *pn;
   pndman_category    *c, *cn;
   pndman_application *a, *an;
   pndman_package     *pp, *ppn;

   /* should never be null */
   assert(pnd);

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
   a = pnd->app;
   for (; a; a = an)
   { an = a->next; _pndman_free_application(a); }

   /* free next installed PND */
   pp = pnd->next_installed;
   for (; pp; pp = ppn)
   { ppn = pp->next_installed; _pndman_free_pnd(pp); }

   /* free this */
   free(pnd);
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

/* vim: set ts=8 sw=3 tw=0 :*/
