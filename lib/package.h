#ifndef PNDMAN_PACKAGE_H
#define PNDMAN_PACKAGE_H

/* if these limits are exceeded, there are no sane people left alive */

#define PND_ID       255
#define PND_NAME     24
#define PND_VER      8
#define PND_STR      255
#define PND_SHRT_STR 24
#define PND_PATH     PATH_MAX-1

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
   char major[PND_VER+1];
   char minor[PND_VER+1];
   char release[PND_VER+1];
   char build[PND_VER+1];
   pndman_version_type type;
} pndman_version;

/* \brief Struct holding execution information */
typedef struct pndman_exec
{
   int background;
   char startdir[PND_PATH+1];
   int standalone;
   char command[PND_PATH+1];
   char arguments[PND_STR+1];
   pndman_exec_x11 x11;
} pndman_exec;

/* \brief Struct holding author information */
typedef struct pndman_author
{
   char name[PND_NAME+1];
   char website[PND_STR+1];
} pndman_author;

/* \brief Struct holding documentation information */
typedef struct pndman_info
{
   char name[PND_NAME+1];
   char type[PND_SHRT_STR+1];
   char src[PND_PATH+1];
} pndman_info;

/* \brief Struct holding translated strings */
typedef struct pndman_translated
{
   char lang[PND_SHRT_STR+1];
   char string[PND_STR+1];

   struct pndman_translated *next;
} pndman_translated;

/* \brief Struct holding license information */
typedef struct pndman_license
{
   char name[PND_SHRT_STR+1];
   char url[PND_STR+1];
   char sourcecodeurl[PND_STR+1];

   struct pndman_license *next;
} pndman_license;

/* \brief Struct holding previewpic information */
typedef struct pndman_previewpic
{
   char src[PND_PATH+1];
   struct pndman_previewpic *next;
} pndman_previewpic;

/* \brief Struct holding association information */
typedef struct pndman_association
{
   char name[PND_STR+1];
   char filetype[PND_SHRT_STR+1];
   char exec[PND_PATH+1];

   struct pndman_association *next;
} pndman_association;

/* \ brief Struct holding category information */
typedef struct pndman_category
{
   char main[PND_SHRT_STR+1];
   char sub[PND_SHRT_STR+1];

   struct pndman_category *next;
} pndman_category;

/* \brief Struct that represents PND application */
typedef struct pndman_application
{
   char id[PND_ID+1];
   char appdata[PND_PATH+1];
   char icon[PND_PATH+1];
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
   char path[PND_PATH+1];
   char id[PND_ID+1];
   char icon[PND_PATH+1];

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
