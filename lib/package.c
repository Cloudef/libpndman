#include <assert.h>
#include "stdio.h"
#include "string.h"
#include "malloc.h"
#include "pndman.h"
#include "package.h"

/* \brief Init version struct */
static void _pndman_init_version(pndman_version *ver)
{
   strncpy(ver->major,   "0", PND_VER);
   strncpy(ver->minor,   "0", PND_VER);
   strncpy(ver->release, "0", PND_VER);
   strncpy(ver->build,   "0", PND_VER);
   ver->type = PND_VERSION_RELEASE;
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

/* \brief Init author struct */
static void _pndman_init_author(pndman_author *author)
{
   memset(author->name,    0, PND_NAME);
   memset(author->website, 0, PND_STR);
}

/* \brief Init info struct */
static void _pndman_init_info(pndman_info *info)
{
   memset(info->name,   0, PND_NAME);
   memset(info->type,   0, PND_SHRT_STR);
   memset(info->src,    0, PND_PATH);
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

   //memset(app->icon,    0, PND_PATH);
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

/* \brief Allocate new pndman_package */
pndman_package* _pndman_new_pnd(void)
{
   pndman_package *pnd;

   /* allocate */
   pnd = malloc(sizeof(pndman_package));
   if (!pnd)
      return NULL;

   /* NULL */
   memset(pnd->path, 0, PND_PATH);
   memset(pnd->id,   0, PND_ID);

   //memset(pnd->icon, 0, PND_PATH);
   strcpy(pnd->icon, PND_DEFAULT_ICON);

   _pndman_init_author(&pnd->author);
   _pndman_init_version(&pnd->version);

   pnd->app             = NULL;
   pnd->title           = NULL;
   pnd->description     = NULL;
   pnd->category        = NULL;

   pnd->flags           = 0;
   pnd->next            = NULL;
   pnd->next_installed  = NULL;

   return pnd;
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
   pndman_category    *c, *cn;
   pndman_application *a, *an;
   pndman_package     *p, *pn;

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

   /* free categories */
   c = pnd->category;
   for (; c; c = cn)
   { cn = c->next; free(c); }

   /* free applications */
   a = pnd->app;
   for (; a; a = an)
   { an = a->next; _pndman_free_application(a); }

   /* free next installed PND */
   p = pnd->next_installed;
   for (; p; p = pn)
   { pn = p->next_installed; free(p); }

   /* free this */
   free(pnd);
}

/* \brief Internal allocation of pndman_translated */
static pndman_translated* _pndman_translated_new(void)
{
   pndman_translated *t;

   t = malloc(sizeof(pndman_translated));
   if (!t)
      return NULL;

   /* init */
   memset(t->lang,   0, PND_SHRT_STR);
   memset(t->string, 0, PND_STR);
   t->next = NULL;

   return t;
}

/* \brief Internal allocation of pndman_license */
static pndman_license* _pndman_license_new(void)
{
   pndman_license *l;

   l = malloc(sizeof(pndman_license));
   if (!l)
      return NULL;

   /* init */
   memset(l->name,            0, PND_SHRT_STR);
   memset(l->url,             0, PND_STR);
   memset(l->sourcecodeurl,   0, PND_STR);
   l->next = NULL;

   return l;
}

/* \brief Internal allocation of pndman_previewpic */
static pndman_previewpic* _pndman_previewpic_new(void)
{
   pndman_previewpic *p;

   p = malloc(sizeof(pndman_previewpic));
   if (!p)
      return NULL;

   /* init */
   memset(p->src,   0, PND_PATH);
   p->next = NULL;

   return p;
}

/* \brief Internal allocation of pndman_association */
static pndman_association* _pndman_association_new(void)
{
   pndman_association *a;

   a = malloc(sizeof(pndman_association));
   if (!a)
      return NULL;

   /* init */
   memset(a->name,            0, PND_STR);
   memset(a->filetype,        0, PND_SHRT_STR);
   memset(a->exec,            0, PND_STR);
   a->next = NULL;

   return a;
}

/* \brief Internal allocation of pndman_category */
static pndman_category* _pndman_category_new(void)
{
   pndman_category *c;

   c = malloc(sizeof(pndman_category));
   if (!c)
      return NULL;

   /* init */
   memset(c->main,   0, PND_SHRT_STR);
   memset(c->sub,    0, PND_SHRT_STR);
   c->next = NULL;

   return c;
}

/* \brief Internal allocation of title for pndman_package */
pndman_translated* _pndman_package_new_title(pndman_package *pnd)
{
   pndman_translated *t;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->title) t = pnd->title = _pndman_translated_new();
   else
   {
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
   else
   {
      /* find last */
      t = pnd->description;
      for (; t->next; t = t->next);
      t->next = _pndman_translated_new();
      t = t->next;
   }

   return t;
}

/* \brief Internal allocation of application for pndman_package */
pndman_application* _pndman_package_new_application(pndman_package *pnd)
{
   pndman_application *app;

   /* should not be null */
   assert(pnd);

   /* how to allocate? */
   if (!pnd->app) app = pnd->app = _pndman_new_application();
   else
   {
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
   else
   {
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
   else
   {
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
   else
   {
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
   else
   {
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
   else
   {
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
   else
   {
      /* find last */
      c = app->category;
      for (; c->next; c = c->next);
      c->next = _pndman_category_new();
      c = c->next;
   }

   return c;
}
