#ifndef PNDMAN_PACKAGE_H
#define PNDMAN_PACKAGE_H

#include <time.h>

/* if these limits are exceeded, there are no sane people left alive */
#define PND_ID       256
#define PND_NAME     24
#define PND_VER      8
#define PND_STR      LINE_MAX
#define PND_SHRT_STR 24
#define PND_MD5      33
#define PND_INFO     LINE_MAX
#define PND_PATH     PATH_MAX

#define PND_DEFAULT_ICON "icon.png"

/* arguments */
#define PND_TRUE           "true"
#define PND_FALSE          "false"
#define PND_TYPE_RELEASE   "release"
#define PND_TYPE_BETA      "beta"
#define PND_TYPE_ALPHA     "alpha"
#define PND_X11_REQ        "req"
#define PND_X11_STOP       "stop"
#define PND_X11_IGNORE     "ignore"

#ifdef __cplusplus
extern "C" {
#endif

/* \brief type enum for version struct */
typedef enum pndman_version_type
{
   PND_VERSION_RELEASE,
   PND_VERSION_BETA,
   PND_VERSION_ALPHA
} pndman_version_type;

/* \brief x11 enum for exec struct */
typedef enum pndman_exec_x11
{
   PND_EXEC_REQ,
   PND_EXEC_STOP,
   PND_EXEC_IGNORE
} pndman_exec_x11;

/* \brief Struct holding version information */
typedef struct pndman_version
{
   char major[PND_VER];
   char minor[PND_VER];
   char release[PND_VER];
   char build[PND_VER];
   pndman_version_type type;
} pndman_version;

/* \brief Struct holding execution information */
typedef struct pndman_exec
{
   int background;
   char startdir[PND_PATH];
   int standalone;
   char command[PND_PATH];
   char arguments[PND_STR];
   pndman_exec_x11 x11;
} pndman_exec;

/* \brief Struct holding author information */
typedef struct pndman_author
{
   char name[PND_NAME];
   char website[PND_STR];
   char email[PND_STR];
} pndman_author;

/* \brief Struct holding documentation information */
typedef struct pndman_info
{
   char name[PND_NAME];
   char type[PND_SHRT_STR];
   char src[PND_PATH];
} pndman_info;

/* \brief Struct holding translated strings */
typedef struct pndman_translated
{
   char lang[PND_SHRT_STR];
   char string[PND_STR];

   struct pndman_translated *next;
} pndman_translated;

/* \brief Struct holding license information */
typedef struct pndman_license
{
   char name[PND_STR];
   char url[PND_STR];
   char sourcecodeurl[PND_STR];

   struct pndman_license *next;
} pndman_license;

/* \brief Struct holding previewpic information */
typedef struct pndman_previewpic
{
   char src[PND_PATH];
   struct pndman_previewpic *next;
} pndman_previewpic;

/* \brief Struct holding association information */
typedef struct pndman_association
{
   char name[PND_STR];
   char filetype[PND_SHRT_STR];
   char exec[PND_PATH];

   struct pndman_association *next;
} pndman_association;

/* \ brief Struct holding category information */
typedef struct pndman_category
{
   char main[PND_SHRT_STR];
   char sub[PND_SHRT_STR];

   struct pndman_category *next;
} pndman_category;

/* \brief Struct that represents PND application */
typedef struct pndman_application
{
   char id[PND_ID];
   char appdata[PND_PATH];
   char icon[PND_PATH];
   int frequency;

   pndman_author  author;
   pndman_version osversion;
   pndman_version version;
   pndman_exec    exec;
   pndman_info    info;

   pndman_translated    *title;
   pndman_translated    *description;
   pndman_license       *license;
   pndman_previewpic    *previewpic;
   pndman_category      *category;
   pndman_association   *association;

   struct pndman_application *next;
} pndman_application;

/* \brief Struct that represents PND */
typedef struct pndman_package
{
   char path[PND_PATH];
   char id[PND_ID];
   char icon[PND_PATH];
   char info[PND_INFO];
   char md5[PND_MD5];
   char url[PND_STR];
   char vendor[PND_NAME];
   size_t size;
   time_t modified_time;
   int rating;

   pndman_author     author;
   pndman_version    version;
   pndman_application *app;

   /* bleh, lots of data duplication.. */
   pndman_translated *title;
   pndman_translated *description;
   pndman_license    *license;
   pndman_previewpic *previewpic;
   pndman_category   *category;

   char repository[PND_STR];
   char mount[PND_PATH];
   struct pndman_package *update;
   struct pndman_package *next_installed;
   struct pndman_package *next;
} pndman_package;

/* INTERNAL */

/* \brief Allocate new pndman_package */
pndman_package* _pndman_new_pnd(void);

/* \brief compare pnd versions */
int _pndman_vercmp(pndman_version *lp, pndman_version *rp);

/* \brief copy pndman_package, doesn't copy next && next_installed pointers */
int _pndman_copy_pnd(pndman_package *pnd, pndman_package *src);

/* \brief Internal free of pndman_package's titles */
void _pndman_package_free_titles(pndman_package *pnd);

/* \brief Internal free of pndman_package's descriptions */
void _pndman_package_free_descriptions(pndman_package *pnd);

/* \brief Internal free of pndman_package's previewpics */
void _pndman_package_free_previewpics(pndman_package *pnd);

/* \brief Internal free of pndman_package's licenses */
void _pndman_package_free_licenses(pndman_package *pnd);

/* \brief Internal free of pndman_package's categories */
void _pndman_package_free_categories(pndman_package *pnd);

/* \brief Internal free of pndman_application's */
void _pndman_package_free_applications(pndman_package *pnd);

/* \brief Internal free of pndman_application's titles
 * NOTE: Used by PXML parser */
void _pndman_application_free_titles(pndman_application *app);

/* \brief Internal free of pndman_application's descriptions
 * NOTE: Used by PXML parser */
void _pndman_application_free_descriptions(pndman_application *app);

/* \brief Internal free of pndman_package, return next_installed, null if no any */
pndman_package* _pndman_free_pnd(pndman_package *pnd);

/* \brief Internal allocation of title for pndman_package */
pndman_translated* _pndman_package_new_title(pndman_package *pnd);

/* \brief Internal allocation of description for pndman_package */
pndman_translated* _pndman_package_new_description(pndman_package *pnd);

/* \brief Internal allocation of license for pndman_package */
pndman_license* _pndman_package_new_license(pndman_package *pnd);

/* \brief Internal allocation of previewpic for pndman_package */
pndman_previewpic* _pndman_package_new_previewpic(pndman_package *pnd);

/* \brief Internal allocation of category for pndman_package */
pndman_category* _pndman_package_new_category(pndman_package *pnd);

/* \brief Internal allocation of application for pndman_package */
pndman_application* _pndman_package_new_application(pndman_package *pnd);

/* \brief Internal allocation of title for pndman_application */
pndman_translated* _pndman_application_new_title(pndman_application *app);

/* \brief Internal allocation of description for pndman_application */
pndman_translated* _pndman_application_new_description(pndman_application *app);

/* \brief Internal allocation of license for pndman_application */
pndman_license* _pndman_application_new_license(pndman_application *app);

/* \brief Internal allocation of previewpic for pndman_application */
pndman_previewpic* _pndman_application_new_previewpic(pndman_application *app);

/* \brief Internal allocation of assocation for pndman_application */
pndman_association* _pndman_application_new_association(pndman_application *app);

/* \brief Internal allocation of category for pndman_application */
pndman_category* _pndman_application_new_category(pndman_application *app);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_PACKAGE_H */

/* vim: set ts=8 sw=3 tw=0 :*/
