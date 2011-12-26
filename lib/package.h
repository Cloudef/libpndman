#ifndef PNDMAN_PACKAGE_H
#define PNDMAN_PACKAGE_H

/* if these limits are exceeded, there are no sane people left alive */

#define PND_ID       256
#define PND_NAME     24
#define PND_VER      8
#define PND_STR      256
#define PND_SHRT_STR 24
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

/* type enum for version struct */
typedef enum pndman_version_type
{
   PND_VERSION_RELEASE,
   PND_VERSION_BETA,
   PND_VERSION_ALPHA
} pndman_version_type;

/* x11 enum for exec struct */
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
   char command[PND_STR];
   char arguments[PND_STR];
   pndman_exec_x11 x11;
} pndman_exec;

/* \brief Struct holding author information */
typedef struct pndman_author
{
   char name[PND_NAME];
   char website[PND_STR];
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
   char name[PND_SHRT_STR];
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
   char exec[PND_STR];

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

   pndman_author     author;
   pndman_version    version;
   pndman_application *app;

   /* bleh, lots of data duplication.. */
   pndman_translated *title;
   pndman_translated *description;
   pndman_category   *category;

   unsigned int flags;
   struct pndman_package *next_installed;
   struct pndman_package *next;
} pndman_package;

#endif /* PNDMAN_PACKAGE_H */
