#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <expat.h>
#include <assert.h>
#include <ctype.h>
#include "pndman.h"
#include "package.h"
#include "repository.h"
#include "device.h"
#include "md5.h"

#ifdef __linux__
#  include <sys/stat.h>
#  include <dirent.h>
#endif

#define PXML_START_TAG "<PXML"
#define PXML_END_TAG   "</PXML>"
#define PXML_TAG_SIZE  7
#define PXML_MAX_SIZE  1024 * 1024
#define XML_HEADER     "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"

/* PXML Tags */
#define PXML_PACKAGE_TAG      "package"
#define PXML_APPLICATION_TAG  "application"
#define PXML_EXEC_TAG         "exec"
#define PXML_AUTHOR_TAG       "author"
#define PXML_OSVERSION_TAG    "osversion"
#define PXML_VERSION_TAG      "version"
#define PXML_ICON_TAG         "icon"
#define PXML_CLOCKSPEED_TAG   "clockspeed"
#define PXML_INFO_TAG         "info"
#define PXML_TITLES_TAG       "titles"
#define PXML_TITLE_TAG        "title"
#define PXML_DESCRIPTIONS_TAG "descriptions"
#define PXML_DESCRIPTION_TAG  "description"
#define PXML_PREVIEWPICS_TAG  "previewpics"
#define PXML_PIC_TAG          "pic"
#define PXML_LICENSES_TAG     "licenses"
#define PXML_LICENSE_TAG      "license"
#define PXML_CATEGORIES_TAG   "categories"
#define PXML_CATEGORY_TAG     "category"
#define PXML_SUBCATEGORY_TAG  "subcategory"
#define PXML_ASSOCIATIONS_TAG "associations"
#define PXML_ASSOCIATION_TAG  "association"

/* PXML Attrs */
#define PXML_ID_ATTR          "id"
#define PXML_APPDATA_ATTR     "appdata"
#define PXML_MAJOR_ATTR       "major"
#define PXML_MINOR_ATTR       "minor"
#define PXML_RELEASE_ATTR     "release"
#define PXML_BUILD_ATTR       "build"
#define PXML_TYPE_ATTR        "type"
#define PXML_NAME_ATTR        "name"
#define PXML_WEBSITE_ATTR     "website"
#define PXML_EMAIL_ATTR       "email"
#define PXML_LANG_ATTR        "lang"
#define PXML_BACKGROUND_ATTR  "background"
#define PXML_STARTDIR_ATTR    "startdir"
#define PXML_STANDALONE_ATTR  "standalone"
#define PXML_COMMAND_ATTR     "command"
#define PXML_ARGUMENTS_ATTR   "arguments"
#define PXML_X11_ATTR         "x11"
#define PXML_URL_ATTR         "url"
#define PXML_SOURCECODE_ATTR  "sourcecodeurl"
#define PXML_SRC_ATTR         "src"
#define PXML_FILETYPE_ATTR    "filetype"
#define PXML_EXEC_ATTR        "exec"
#define PXML_FREQUENCY_ATTR   "frequency"

/* \brief State enum for identifyin various parse stages */
typedef enum PXML_STATE
{
   /* \brief parsing outside */
   PXML_PARSE_DEFAULT,
   /* \brief parsing <package> element */
   PXML_PARSE_PACKAGE,
   /* \brief parsing <titles> inside <package> element */
   PXML_PARSE_PACKAGE_TITLES,
   /* \brief parsing <descriptions> inside <package> element */
   PXML_PARSE_PACKAGE_DESCRIPTIONS,
   /* \brief parsing <application> element */
   PXML_PARSE_APPLICATION,
   /* \brief parsin <titles> inside <application> element */
   PXML_PARSE_APPLICATION_TITLES,
   /* \brief parsing <descriptions> inside <application> element */
   PXML_PARSE_APPLICATION_DESCRIPTIONS,
   /* \brief parsing <licenses> inside <application> element */
   PXML_PARSE_APPLICATION_LICENSES,
   /* \brief parsing <previewpics> inside <application> element */
   PXML_PARSE_APPLICATION_PREVIEWPICS,
   /* \brief parsing <categories> inside <application> element */
   PXML_PARSE_APPLICATION_CATEGORIES,
   /* \brief parsing <category> inside <categories> element which resides inside <application> */
   PXML_PARSE_APPLICATION_CATEGORY,
   /* \brief parsting <associations> inside <application> element */
   PXML_PARSE_APPLICATION_ASSOCIATIONS
} PXML_STATE;

/* \brief struct which holds our userdata while parsing */
typedef struct pxml_parse
{
   PXML_STATE           state;
   pndman_package       *pnd;
   pndman_application   *app;
   char                 *data;
   int                  bckward_title;
   int                  bckward_desc;
} pxml_parse;

/* \brief
 * Convert string to uppercase
 * Returns allocated string, so remember free it */
static char *upstr(char *s)
{
   int i; char* p = malloc(strlen(s) + 1);
   if (!p) return NULL;

   strcpy( p, s ); i = 0;

   for (; i != strlen(p); i++)
      p[i] = (char)toupper(p[i]);

   return p;
}

/* \brief
 * Strcmp without case sensitivity */
static int _pxml_strcmp(char *s1, char *s2)
{
   int ret; char *u1, *u2;
   u1 = upstr(s1); if (!u1) return 1;
   u2 = upstr(s2); if (!u2) { free(u1); return 1; }

   ret = memcmp(u1, u2, strlen(u1));
   free(u1); free(u2);

   return ret;
}

/* \brief
 * Get PXML out of PND  */
static int _fetch_pxml_from_pnd(char *pnd_file, char *PXML, size_t *size)
{
   FILE     *pnd;
   char     s[LINE_MAX];
   size_t   pos;
   char     *ret;

   /* open PND */
   pnd = fopen(pnd_file, "rb");
   if (!pnd)
      return RETURN_FAIL;

   /* set && seek to end */
   memset(s,  0, LINE_MAX);
   fseek(pnd, 0, SEEK_END); pos = ftell(pnd) - 512; fseek(pnd, pos, SEEK_SET);
   for (; (ret = fgets(s, PXML_TAG_SIZE, pnd)) && memcmp(s, PXML_START_TAG, strlen(PXML_START_TAG));
          fseek(pnd, --pos, SEEK_SET));

   /* Yatta! PXML start found ^_^ */
   /* PXML does not define standard XML, so add those to not confuse expat */
   strncpy(PXML, XML_HEADER, PXML_MAX_SIZE-1);

   /* read until end tag */
   for (; ret && memcmp(s, PXML_END_TAG, strlen(PXML_END_TAG)); ret = fgets(s, LINE_MAX-1, pnd))
      strncat(PXML, s, PXML_MAX_SIZE-1);

   /* and we are done, was it so hard libpnd? */
   strncat(PXML, PXML_END_TAG, PXML_MAX_SIZE-1);

   /* store size */
   *size = strlen(PXML);

   /* close the file */
   fclose(pnd);

   return RETURN_OK;
}

/* \brief fills pndman_package's struct */
static void _pxml_pnd_package_tag(pndman_package *pnd, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <package id= */
      if (!memcmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR)))
         strncpy(pnd->id, attrs[++i], PND_ID-1);
   }
}

/* \brief fills pndman_package's struct */
static void _pxml_pnd_icon_tag(pndman_package *pnd, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <icon src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(pnd->icon, attrs[++i], PND_PATH-1);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <application id= */
      if (!memcmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR)))
         strncpy(app->id, attrs[++i], PND_ID-1);
      /* <application appdata= */
      else if (!memcmp(attrs[i], PXML_APPDATA_ATTR, strlen(PXML_APPDATA_ATTR)))
         strncpy(app->appdata, attrs[++i], PND_PATH-1);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_icon_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <icon src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(app->icon, attrs[++i], PND_PATH-1);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_clockspeed_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <clockspeed frequency= */
      if (!memcmp(attrs[i], PXML_FREQUENCY_ATTR, strlen(PXML_FREQUENCY_ATTR)))
         strtol(attrs[++i], (char **) NULL, 10);
   }
}

/* \brief fills pndman_exec's structs */
static void _pxml_pnd_exec_tag(pndman_exec *exec, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <exec background= */
      if (!memcmp(attrs[i], PXML_BACKGROUND_ATTR, strlen(PXML_BACKGROUND_ATTR)))
      {
         if (!_pxml_strcmp(attrs[++i], PND_TRUE)) exec->background = 1;
      }
      /* <exec startdir= */
      else if (!memcmp(attrs[i], PXML_STARTDIR_ATTR, strlen(PXML_STARTDIR_ATTR)))
         strncpy(exec->startdir, attrs[++i], PND_PATH-1);
      /* <exec standalone= */
      else if (!memcmp(attrs[i], PXML_STANDALONE_ATTR, strlen(PXML_STANDALONE_ATTR)))
      {
         if (!memcmp(attrs[++i], PND_FALSE, strlen(PND_FALSE))) exec->standalone = 0;
      }
      /* <exec command= */
      else if (!memcmp(attrs[i], PXML_COMMAND_ATTR, strlen(PXML_COMMAND_ATTR)))
         strncpy(exec->command, attrs[++i], PND_PATH-1);
      /* <exec arguments= */
      else if (!memcmp(attrs[i], PXML_ARGUMENTS_ATTR, strlen(PXML_ARGUMENTS_ATTR)))
         strncpy(exec->arguments, attrs[++i], PND_STR-1);
      /* <exec x11= */
      else if (!memcmp(attrs[i], PXML_X11_ATTR, strlen(PXML_X11_ATTR)))
      {
         ++i;
         if (!_pxml_strcmp(attrs[i], PND_X11_REQ))          exec->x11 = PND_EXEC_REQ;
         else if (!_pxml_strcmp(attrs[i], PND_X11_STOP))    exec->x11 = PND_EXEC_STOP;
         else if (!_pxml_strcmp(attrs[i], PND_X11_IGNORE))  exec->x11 = PND_EXEC_IGNORE;
      }
   }
}

/* \brief fills pndman_info's struct */
static void _pxml_pnd_info_tag(pndman_info *info, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <info name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(info->name, attrs[++i], PND_NAME-1);
      /* <info type= */
      else if (!memcmp(attrs[i], PXML_TYPE_ATTR, strlen(PXML_TYPE_ATTR)))
         strncpy(info->type, attrs[++i], PND_SHRT_STR-1);
      /* <info src= */
      else if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(info->src, attrs[++i], PND_PATH-1);
   }
}

/* \brief fills pndman_license's struct */
static void _pxml_pnd_license_tag(pndman_license *lic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <license name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(lic->name, attrs[++i], PND_SHRT_STR-1);
      /* <license url= */
      else if (!memcmp(attrs[i], PXML_URL_ATTR, strlen(PXML_URL_ATTR)))
         strncpy(lic->url, attrs[++i], PND_STR-1);
      /* <license sourcecodeurl= */
      else if (!memcmp(attrs[i], PXML_SOURCECODE_ATTR, strlen(PXML_SOURCECODE_ATTR)))
         strncpy(lic->sourcecodeurl, attrs[++i], PND_STR-1);
   }
}

/* \brief fills pndman_previewpic's struct */
static void _pxml_pnd_pic_tag(pndman_previewpic *pic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <pic src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(pic->src, attrs[++i], PND_PATH-1);
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_category_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <category name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(cat->main, attrs[++i], PND_SHRT_STR-1);
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_subcategory_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <subcategory name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(cat->sub, attrs[++i], PND_SHRT_STR-1);
   }
}

/* \brief fills pndman_associations's struct */
static void _pxml_pnd_association_tag(pndman_association *assoc, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <association name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(assoc->name, attrs[++i], PND_STR-1);
      /* <association filetype= */
      else if (!memcmp(attrs[i], PXML_FILETYPE_ATTR, strlen(PXML_FILETYPE_ATTR)))
         strncpy(assoc->filetype, attrs[++i], PND_SHRT_STR-1);
      /* <association exec= */
      else if (!memcmp(attrs[i], PXML_EXEC_ATTR, strlen(PXML_EXEC_ATTR)))
         strncpy(assoc->exec, attrs[++i], PND_PATH-1);
   }
}

/* \brief fills pndman_translated's struct */
static void _pxml_pnd_translated_tag(pndman_translated *title, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <title/description lang= */
      if (!memcmp(attrs[i], PXML_LANG_ATTR, strlen(PXML_LANG_ATTR)))
         strncpy(title->lang, attrs[++i], PND_SHRT_STR-1);
   }
}

/* \brief fills pndman_author's structs */
static void _pxml_pnd_author_tag(pndman_author *author, char **attrs)
{
   int i = 0;
   for(; attrs[i]; ++i) {
      /* <author name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(author->name, attrs[++i], PND_NAME-1);
      /* <author website= */
      else if(!memcmp(attrs[i], PXML_WEBSITE_ATTR, strlen(PXML_WEBSITE_ATTR)))
         strncpy(author->website, attrs[++i], PND_STR-1);
      /* <author email= */
      else if(!memcmp(attrs[i], PXML_EMAIL_ATTR, strlen(PXML_EMAIL_ATTR)))
         strncpy(author->email, attrs[++i], PND_STR-1);
   }
}

/* \brief fills pndman_version's structs */
static void _pxml_pnd_version_tag(pndman_version *ver, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <osversion/version major= */
      if (!memcmp(attrs[i], PXML_MAJOR_ATTR, strlen(PXML_MAJOR_ATTR)))
         strncpy(ver->major, attrs[++i], PND_VER-1);
      /* <osversion/version minor= */
      else if (!memcmp(attrs[i], PXML_MINOR_ATTR, strlen(PXML_MINOR_ATTR)))
         strncpy(ver->minor, attrs[++i], PND_VER-1);
      /* <osverion/version release= */
      else if (!memcmp(attrs[i], PXML_RELEASE_ATTR, strlen(PXML_RELEASE_ATTR)))
         strncpy(ver->release, attrs[++i], PND_VER-1);
      /* <osversion/version build= */
      else if (!memcmp(attrs[i], PXML_BUILD_ATTR, strlen(PXML_BUILD_ATTR)))
         strncpy(ver->build, attrs[++i], PND_VER-1);
      else if (!memcmp(attrs[i], PXML_TYPE_ATTR, strlen(PXML_TYPE_ATTR)))
      {
         ++i;
         if (!_pxml_strcmp(PND_TYPE_RELEASE, attrs[i]))    ver->type = PND_VERSION_RELEASE;
         else if (!_pxml_strcmp(PND_TYPE_BETA, attrs[i]))  ver->type = PND_VERSION_BETA;
         else if (!_pxml_strcmp(PND_TYPE_ALPHA, attrs[i])) ver->type = PND_VERSION_ALPHA;
      }
   }
}

/* \brief Start element tag */
static void _pxml_pnd_start_tag(void *data, char *tag, char** attrs)
{
   unsigned int       *parse_state  = &((pxml_parse*)data)->state;
   pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   pndman_application *app          = ((pxml_parse*)data)->app;

   /* some variables used for parsing */
   pndman_translated  *title, *desc;
   pndman_license     *license;
   pndman_previewpic  *previewpic;
   pndman_category    *category;
   pndman_association *association;

   //DEBUGP("Found start : %s [%s, %s]\n", tag, attrs[0], attrs[1]);

   /* check parse state, so we don't parse wrong stuff */
   switch (*parse_state) {
      /* parse all outer elements */
      case PXML_PARSE_DEFAULT:
         /* <package */
         if (!memcmp(tag, PXML_PACKAGE_TAG, strlen(PXML_PACKAGE_TAG)))
         {
            _pxml_pnd_package_tag(pnd, attrs);
            *parse_state = PXML_PARSE_PACKAGE;
         }
         /* <application */
         else if (!memcmp(tag, PXML_APPLICATION_TAG, strlen(PXML_APPLICATION_TAG)))
         {
            if ((app = _pndman_package_new_application(pnd)) != NULL)
            {
               ((pxml_parse*)data)->app = app;
               _pxml_pnd_application_tag(app, attrs);
               *parse_state = PXML_PARSE_APPLICATION;
            }
         }
      break;
      /* parse <titles> inside <package> */
      case PXML_PARSE_PACKAGE_TITLES:
         /* <title */
         if (!memcmp(tag, PXML_TITLE_TAG, strlen(PXML_TITLE_TAG)))
         {
            if ((title = _pndman_package_new_title(pnd)))
            {
               _pxml_pnd_translated_tag(title, attrs);
               ((pxml_parse*)data)->data = title->string;
            }
         }
      break;
      /* parse <descriptions> inside <package> */
      case PXML_PARSE_PACKAGE_DESCRIPTIONS:
         /* <description */
         if (!memcmp(tag, PXML_DESCRIPTION_TAG, strlen(PXML_DESCRIPTION_TAG)))
         {
            if ((desc = _pndman_package_new_description(pnd)))
            {
               _pxml_pnd_translated_tag(desc, attrs);
               ((pxml_parse*)data)->data = desc->string;
            }
         }
      break;
      /* parse inside <package> */
      case PXML_PARSE_PACKAGE:
         /* <author */
         if (!memcmp(tag, PXML_AUTHOR_TAG, strlen(PXML_AUTHOR_TAG)))
            _pxml_pnd_author_tag(&pnd->author, attrs);
         /* <version */
         else if (!memcmp(tag, PXML_VERSION_TAG, strlen(PXML_VERSION_TAG)))
            _pxml_pnd_version_tag(&pnd->version, attrs);
         /* <icon */
         else if (!memcmp(tag, PXML_ICON_TAG, strlen(PXML_ICON_TAG)))
            _pxml_pnd_icon_tag(pnd, attrs);
         /* <titles */
         else if (!memcmp(tag, PXML_TITLES_TAG, strlen(PXML_TITLES_TAG)))
            *parse_state = PXML_PARSE_PACKAGE_TITLES;
         /* <descriptions */
         else if (!memcmp(tag, PXML_DESCRIPTIONS_TAG, strlen(PXML_DESCRIPTIONS_TAG)))
            *parse_state = PXML_PARSE_PACKAGE_DESCRIPTIONS;
      break;
      /* parse <titles> inside <application> */
      case PXML_PARSE_APPLICATION_TITLES:
         assert(app);
         /* <title */
         if (!memcmp(tag, PXML_TITLE_TAG, strlen(PXML_TITLE_TAG)))
         {
            if ((title = _pndman_application_new_title(app)))
            {
               _pxml_pnd_translated_tag(title, attrs);
               ((pxml_parse*)data)->data = title->string;
            }
         }
      break;
      /* parse <descriptions> inside <application> */
      case PXML_PARSE_APPLICATION_DESCRIPTIONS:
         assert(app);
         /* <description */
         if (!memcmp(tag, PXML_DESCRIPTION_TAG, strlen(PXML_DESCRIPTION_TAG)))
         {
            if ((desc = _pndman_application_new_description(app)))
            {
               _pxml_pnd_translated_tag(desc, attrs);
               ((pxml_parse*)data)->data = desc->string;
            }
         }
      break;
      /* parse <licenses> inside <application> */
      case PXML_PARSE_APPLICATION_LICENSES:
         assert(app);
         /* <license */
         if (!memcmp(tag, PXML_LICENSE_TAG, strlen(PXML_LICENSE_TAG)))
         {
            if ((license = _pndman_application_new_license(app)))
               _pxml_pnd_license_tag(license, attrs);
         }
      break;
      /* parse <previewpics> inside <application> */
      case PXML_PARSE_APPLICATION_PREVIEWPICS:
         assert(app);
         /* <pic */
         if (!memcmp(tag, PXML_PIC_TAG, strlen(PXML_PIC_TAG)))
         {
            if ((previewpic = _pndman_application_new_previewpic(app)))
               _pxml_pnd_pic_tag(previewpic, attrs);
         }
      break;
      /* parse <categories> inside <application> */
      case PXML_PARSE_APPLICATION_CATEGORIES:
         assert(app);
         /* <category */
         if (!memcmp(tag, PXML_CATEGORY_TAG, strlen(PXML_LICENSE_TAG)))
         {
            if ((category = _pndman_application_new_category(app)))
               _pxml_pnd_category_tag(category, attrs);
            *parse_state = PXML_PARSE_APPLICATION_CATEGORY;
         }
      break;
      /* parse <category> inside <categories> */
      case PXML_PARSE_APPLICATION_CATEGORY:
         assert(app);
         /* <subcategory */
         if (!memcmp(tag, PXML_SUBCATEGORY_TAG, strlen(PXML_SUBCATEGORY_TAG)))
         {
            category = app->category;
            for (; category && category->next; category = category->next);
            if (category)
               _pxml_pnd_subcategory_tag(category, attrs);
         }
      break;
      /* parse <associations> inside <application> */
      case PXML_PARSE_APPLICATION_ASSOCIATIONS:
         assert(app);
         /* <association */
         if (!memcmp(tag, PXML_ASSOCIATION_TAG, strlen(PXML_ASSOCIATION_TAG)))
         {
            if (app && (association = _pndman_application_new_association(app)))
               _pxml_pnd_association_tag(association, attrs);
         }
      break;
      /* parse inside <application> */
      case PXML_PARSE_APPLICATION:
         assert(app);
         /* <exec */
         if (!memcmp(tag, PXML_EXEC_TAG, strlen(PXML_EXEC_TAG)))
            _pxml_pnd_exec_tag(&app->exec, attrs);
         /* <author */
         else if (!memcmp(tag, PXML_AUTHOR_TAG, strlen(PXML_AUTHOR_TAG)))
            _pxml_pnd_author_tag(&app->author, attrs);
         /* <osversion */
         else if (!memcmp(tag, PXML_OSVERSION_TAG, strlen(PXML_OSVERSION_TAG)))
            _pxml_pnd_version_tag(&app->osversion, attrs);
         /* <version */
         else if (!memcmp(tag, PXML_VERSION_TAG, strlen(PXML_VERSION_TAG)))
            _pxml_pnd_version_tag(&app->version, attrs);
         /* <icon */
         else if (!memcmp(tag, PXML_ICON_TAG, strlen(PXML_ICON_TAG)))
            _pxml_pnd_application_icon_tag(app, attrs);
         /* <info */
         else if (!memcmp(tag, PXML_INFO_TAG, strlen(PXML_INFO_TAG)))
            _pxml_pnd_info_tag(&app->info, attrs);
         /* <clockspeed */
         else if (!memcmp(tag, PXML_CLOCKSPEED_TAG, strlen(PXML_CLOCKSPEED_TAG)))
            _pxml_pnd_application_clockspeed_tag(app, attrs);
         /* <titles */
         else if (!memcmp(tag, PXML_TITLES_TAG, strlen(PXML_TITLES_TAG)))
         {
            if (app->title) _pndman_application_free_titles(app);
            ((pxml_parse*)data)->bckward_title = 0;
            *parse_state = PXML_PARSE_APPLICATION_TITLES;
         }
         /* <descriptions */
         else if (!memcmp(tag, PXML_DESCRIPTIONS_TAG, strlen(PXML_DESCRIPTIONS_TAG)))
         {
            if (app->description) _pndman_application_free_descriptions(app);
            ((pxml_parse*)data)->bckward_desc = 0;
            *parse_state = PXML_PARSE_APPLICATION_DESCRIPTIONS;
         }
         /* <licenses */
         else if (!memcmp(tag, PXML_LICENSES_TAG, strlen(PXML_LICENSES_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_LICENSES;
         /* <previewpics */
         else if (!memcmp(tag, PXML_PREVIEWPICS_TAG, strlen(PXML_PREVIEWPICS_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_PREVIEWPICS;
         /* <categories */
         else if (!memcmp(tag, PXML_CATEGORIES_TAG, strlen(PXML_CATEGORIES_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_CATEGORIES;
         /* <associations */
         else if (!memcmp(tag, PXML_ASSOCIATIONS_TAG, strlen(PXML_ASSOCIATIONS_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_ASSOCIATIONS;

         /*                                 */
         /* === BACKWARDS COMPATIBILITY === */
         /* TODO: These are okay to ignore if new methods are used */
         /* <title */
         else if (((pxml_parse*)data)->bckward_title)
         {
            if (!memcmp(tag, PXML_TITLE_TAG, strlen(PXML_TITLE_TAG)))
            {
               if ((title = _pndman_application_new_title(app)))
               {
                  _pxml_pnd_translated_tag(title, attrs);
                  ((pxml_parse*)data)->data = title->string;
               }
            }
         }
         /* <description */
         else if (((pxml_parse*)data)->bckward_desc)
         {
            if (!memcmp(tag, PXML_DESCRIPTION_TAG, strlen(PXML_DESCRIPTION_TAG)))
            {
               if ((desc = _pndman_application_new_description(app)))
               {
                  _pxml_pnd_translated_tag(desc, attrs);
                  ((pxml_parse*)data)->data = desc->string;
               }
            }
         }
      break;
   }
}

/* \brief Text data */
static void _pxml_pnd_data(void *data, char *text, int len)
{
   // unsigned int       *parse_state  = &((pxml_parse*)data)->state;
   // pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   // pndman_application *app          = ((pxml_parse*)data)->app;
   char                  *ptr          = ((pxml_parse*)data)->data;

   if (!ptr) return;
   if (len < PND_STR) memcpy(ptr, text, len);
   ((pxml_parse*)data)->data = NULL;
}

/* \brief End element tag */
static void _pxml_pnd_end_tag(void *data, char *tag)
{
   //DEBUGP("Found end : %s\n", tag);

   unsigned int          *parse_state  = &((pxml_parse*)data)->state;
   // pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   // pndman_application *app          = ((pxml_parse*)data)->app;

   /* </package> */
   if (!memcmp(tag, PXML_PACKAGE_TAG, strlen(PXML_PACKAGE_TAG)))
      *parse_state = PXML_PARSE_DEFAULT;
   /* </application */
   else if (!memcmp(tag, PXML_APPLICATION_TAG, strlen(PXML_APPLICATION_TAG)))
   {
      *parse_state = PXML_PARSE_PACKAGE;
      ((pxml_parse*)data)->app = NULL;
   }
   /* </titles> */
   else if (!memcmp(tag, PXML_TITLES_TAG, strlen(PXML_TITLES_TAG)))
   {
      if (*parse_state == PXML_PARSE_PACKAGE_TITLES)
         *parse_state = PXML_PARSE_PACKAGE;
      else if (*parse_state == PXML_PARSE_APPLICATION_TITLES)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </descriptions> */
   else if (!memcmp(tag, PXML_DESCRIPTIONS_TAG, strlen(PXML_DESCRIPTIONS_TAG)))
   {
      if (*parse_state == PXML_PARSE_PACKAGE_DESCRIPTIONS)
         *parse_state = PXML_PARSE_PACKAGE;
      else if (*parse_state == PXML_PARSE_APPLICATION_DESCRIPTIONS)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </licenses> */
   else if (!memcmp(tag, PXML_LICENSES_TAG, strlen(PXML_LICENSES_TAG)))
   {
      if (*parse_state == PXML_PARSE_APPLICATION_LICENSES)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </licenses> */
   else if (!memcmp(tag, PXML_PREVIEWPICS_TAG, strlen(PXML_PREVIEWPICS_TAG)))
   {
      if (*parse_state == PXML_PARSE_APPLICATION_PREVIEWPICS)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </categories> */
   else if (!memcmp(tag, PXML_CATEGORIES_TAG, strlen(PXML_CATEGORIES_TAG)))
   {
      if (*parse_state == PXML_PARSE_APPLICATION_CATEGORIES)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </associations> */
   else if (!memcmp(tag, PXML_ASSOCIATIONS_TAG, strlen(PXML_ASSOCIATIONS_TAG)))
   {
      if (*parse_state == PXML_PARSE_APPLICATION_ASSOCIATIONS)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
}

/* \brief
 * Parse the PXML data and fill the package structs */
static int _pxml_pnd_parse(pxml_parse *data, char *PXML, size_t size)
{
   XML_Parser xml;
   int ret;

   /* try it */
   xml = XML_ParserCreate(NULL);

   /* xml sucks */
   if (!xml)
      return RETURN_FAIL;

   /* set userdata */
   XML_SetUserData(xml, data);

   /* set handlers */
   XML_SetElementHandler(xml,
         (XML_StartElementHandler)&_pxml_pnd_start_tag,
         (XML_EndElementHandler)&_pxml_pnd_end_tag);
   XML_SetCharacterDataHandler(xml,
         (XML_CharacterDataHandler)&_pxml_pnd_data);

   /* parse XML */
   ret = RETURN_OK;
   if (XML_Parse(xml, PXML, size, 1) == XML_STATUS_ERROR)
      ret = RETURN_FAIL;

   if (ret == RETURN_FAIL) {
      DEBUG(PXML);
      exit(EXIT_FAILURE);
   }

   /* free the parser */
   XML_ParserFree(xml);

   return ret;
}

/* \brief Post-process pndman_package
 * Some PND's including milkyhelper ironically,
 * don't have a <package> element.
 *
 * For these PNDs we need to copy stuff from first application */
static void _pxml_pnd_post_process(pndman_package *pnd)
{
   pndman_translated *t, *tc;
   pndman_license *l, *lc;
   pndman_previewpic *p, *pc;
   pndman_category *c, *cc;

   /* no need to do anything */
   if (!pnd->app)
      return;

   /* header */
   if (!strlen(pnd->id) && strlen(pnd->app->id))
      memcpy(pnd->id, pnd->app->id, PND_ID);
   if (!strlen(pnd->icon) && strlen(pnd->app->icon))
      memcpy(pnd->icon, pnd->app->icon, PND_PATH);

   /* author */
   if (!strlen(pnd->author.name) && strlen(pnd->app->author.name))
      memcpy(pnd->author.name, pnd->app->author.name, PND_NAME);
   if (!strlen(pnd->author.website) && strlen(pnd->app->author.website))
      memcpy(pnd->author.website, pnd->app->author.website, PND_STR);

   /* version */
   if (!strlen(pnd->version.major) && strlen(pnd->app->version.major))
      memcpy(pnd->version.major, pnd->app->version.major, PND_VER);
   if (!strlen(pnd->version.minor) && strlen(pnd->app->version.minor))
      memcpy(pnd->version.minor, pnd->app->version.minor, PND_VER);
   if (!strlen(pnd->version.release) && strlen(pnd->app->version.release))
      memcpy(pnd->version.release, pnd->app->version.release, PND_VER);
   if (!strlen(pnd->version.build) && strlen(pnd->app->version.build))
      memcpy(pnd->version.build, pnd->app->version.build, PND_VER);

   /* titles */
   if (!pnd->title && pnd->app->title) {
      t = pnd->app->title;
      for (; t; t = t->next) {
         if ((tc = _pndman_package_new_title(pnd))) {
            memcpy(tc->lang, t->lang, PND_SHRT_STR);
            memcpy(tc->string, t->string, PND_STR);
         }
      }
   }

   /* descriptions */
   if (!pnd->description && pnd->app->description) {
      t = pnd->app->description;
      for (; t; t = t->next) {
         if ((tc = _pndman_package_new_description(pnd))) {
            memcpy(tc->lang, t->lang, PND_SHRT_STR);
            memcpy(tc->string, t->string, PND_STR);
         }
      }
   }

   /* licenses */
   if (!pnd->license && pnd->app->license) {
      l = pnd->app->license;
      for (; l; l = l->next) {
         if ((lc = _pndman_package_new_license(pnd))) {
            memcpy(lc->name, l->name, PND_SHRT_STR);
            memcpy(lc->url, l->url, PND_STR);
            memcpy(lc->sourcecodeurl, l->sourcecodeurl, PND_STR);
         }
      }
   }

   /* previewpics */
   if (!pnd->previewpic && pnd->app->previewpic) {
      p = pnd->app->previewpic;
      for (; p; p = p->next) {
         if ((pc = _pndman_package_new_previewpic(pnd))) {
            memcpy(pc->src, p->src, PND_PATH);
         }
      }
   }

   /* categories */
   if (!pnd->category && pnd->app->category) {
      c = pnd->app->category;
      for (; c; c = c->next) {
         if ((cc = _pndman_package_new_category(pnd))) {
            memcpy(cc->main, c->main, PND_SHRT_STR);
            memcpy(cc->sub, c->sub, PND_SHRT_STR);
         }
      }
   }
}

/* \brief Process crawl */
static int _pndman_crawl_process(char *pnd_file, pxml_parse *data)
{
   char PXML[PXML_MAX_SIZE];
   char *md5;
   size_t size;
   assert(pnd_file && data);

   if (_fetch_pxml_from_pnd(pnd_file, PXML, &size) != RETURN_OK)
      return RETURN_FAIL;

   /* parse */
   if (_pxml_pnd_parse(data, PXML, size) != RETURN_OK)
      return RETURN_FAIL;
   _pxml_pnd_post_process(data->pnd);

   /* add path to the pnd */
   strncpy(data->pnd->path, pnd_file, PATH_MAX-1);

   /* add md5 to the pnd */
   md5 = _pndman_md5(pnd_file);
   if (md5) {
      strncpy(data->pnd->md5, md5, PND_MD5);
      free(md5);
   }

   return RETURN_OK;
}

/* \brief Crawl directory */
static int _pndman_crawl_dir(char *path, pndman_package *list)
{
   char tmp[PATH_MAX];
   pxml_parse data;
   pndman_package *pnd, *p;
   int ret;

#ifdef __linux__
   DIR *dp;
   struct dirent *ep;
#elif __WIN32__
   WIN32_FIND_DATA dp;
   HANDLE hFind;
#endif

   assert(path && list);

   /* init parse data */
   data.bckward_title = 1; /* backwards compatibility with PXML titles */
   data.bckward_desc  = 1; /* backwards compatibility with PXML descriptions */
   pnd = NULL; p = NULL;
   ret = 0;

#ifdef __linux__
   dp = opendir(path);
   if (!dp) return 0;

   while (ep = readdir(dp)) {
      if (!strstr(ep->d_name, ".pnd")) continue;
      strcpy(tmp, path);
      strncat(tmp, "/", PATH_MAX-1);
      strncat(tmp, ep->d_name, PATH_MAX-1);

      /* create pnd */
      pnd = _pndman_new_pnd();
      if (!pnd) continue;

      /* crawl */
      data.pnd   = pnd;
      data.app   = NULL;
      data.data  = NULL;
      data.state = PXML_PARSE_DEFAULT;
      if (_pndman_crawl_process(tmp, &data) != RETURN_OK) {
         _pndman_free_pnd(pnd);
         continue;
      }

      /* assign pnd to list */
      if (!p) {
         _pndman_copy_pnd(list, pnd);
         _pndman_free_pnd(pnd);
         p = list;
      } else {
         for (p = list; p && p->next; p = p->next);
         p->next = pnd;
      }
      ++ret;
   }
   closedir(dp);
#elif __WIN32__
   strcpy(tmp, path);
   strncat(tmp, "/*.pnd", PATH_MAX-1);

   if ((hFind = FindFirstFile(tmp, &dp)) == INVALID_HANDLE_VALUE)
      return 0;

   do {
      strcpy(tmp, path);
      strncat(tmp, "/", PATH_MAX-1);
      strncat(tmp, dp.cFileName, PATH_MAX-1);

      /* create pnd */
      pnd = _pndman_new_pnd();
      if (!pnd) continue;

      /* crawl */
      data.pnd   = pnd;
      data.app   = NULL;
      data.data  = NULL;
      data.state = PXML_PARSE_DEFAULT;
      if (_pndman_crawl_process(tmp, &data) != RETURN_OK) {
         _pndman_free_pnd(pnd);
         continue;
      }

      /* assign pnd to list */
      if (!p) {
         _pndman_copy_pnd(list, pnd);
         p = list;
      } else {
         for (; p && p->next; p = p->next);
         p->next = pnd;
      }
      ++ret;
   } while (FindNextFile(hFind, &dp));
   FindClose(hFind);
#endif

   return ret;
}

/* \brief Crawl to pndman_package list */
static int _pndman_crawl_to_pnd_list(pndman_device *device, pndman_package *list)
{
   char tmp[PATH_MAX];
   char tmp2[PATH_MAX];
   int ret = 0;
   assert(device && list);

   memset(tmp,  0, PATH_MAX);

   /* pandora root */
   strncpy(tmp, device->mount,   PATH_MAX-1);
   strncat(tmp, "/pandora",      PATH_MAX-1);

   /* can't access */
   if (access(tmp, F_OK) != 0)
      return RETURN_FAIL;

   /* desktop */
   strcpy(tmp2, tmp);
   strncat(tmp2, "/desktop", PATH_MAX-1);
   ret += _pndman_crawl_dir(tmp2, list);

   /* menu */
   strcpy(tmp2, tmp);
   strncat(tmp2, "/menu", PATH_MAX-1);
   ret += _pndman_crawl_dir(tmp2, list);

   /* apps */
   strcpy(tmp2, tmp);
   strncat(tmp2, "/apps", PATH_MAX-1);
   ret += _pndman_crawl_dir(tmp2, list);

   return ret;
}

/* \brief crawl to repository, return number of pnd's found, and -1 on error */
static int _pndman_crawl_to_repository(pndman_device *device, pndman_repository *local)
{
   pndman_package *list, *p, *n, *pnd;
   int ret;
   assert(device && local);

   list = _pndman_new_pnd();
   if (!list)
      return RETURN_FAIL;

   /* crawl pnds to list */
   ret = _pndman_crawl_to_pnd_list(device, list);

   /* either crawl failed, or no pnd's found */
   if (ret <= 0) {
      for (p = list; p; p = n) {
         n = p->next;
         _pndman_free_pnd(p);
      }
      return ret;
   }

   /* merge pnd's to repo */
   for (p = list; p; p = n) {
      pnd = _pndman_repository_new_pnd_check(p->id, p->path, local);
      if (!pnd) continue;
      _pndman_copy_pnd(pnd, p);
      pnd->flags = PND_INSTALLED; /* it's obiviously installed :) */

      /* free */
      n = p->next;
      _pndman_free_pnd(p);
   }

   return ret;
}

/* API */

/* \brief crawl pnds to local repository, returns number of pnd's found, and -1 on error */
int pndman_crawl(pndman_device *device, pndman_repository *local)
{
   if (!device) return RETURN_FAIL;
   if (!local)  return RETURN_FAIL;
   local = _pndman_repository_first(local);
   return _pndman_crawl_to_repository(device, local);
}

/* \brief
 * Stub test function */
int pnd_do_something(char *pnd_file)
{
   char PXML[PXML_MAX_SIZE];
   char *type, *x11; size_t size = 0;
   pndman_package       *test;
   pndman_application   *app;
   pndman_translated    *t;
   pndman_license       *l;
   pndman_category      *c;
   pndman_previewpic    *p;
   pndman_association   *a;

   memset(PXML, 0, PXML_MAX_SIZE);
   _fetch_pxml_from_pnd(pnd_file, PXML, &size);
   test = _pndman_new_pnd();
   if (!test) return RETURN_FAIL;

   pxml_parse data;
   data.pnd   = test;
   data.app   = NULL;
   data.data  = NULL;
   data.bckward_title = 1;
   data.bckward_desc  = 1;
   data.state = PXML_PARSE_DEFAULT;

   /* parse */
   if (_pxml_pnd_parse(&data, PXML, size) != RETURN_OK) {
      DEBUG("Your code sucks, fix it!");
      _pndman_free_pnd(test);
      exit(EXIT_FAILURE);
   }
   _pxml_pnd_post_process(test);

   /* debug filled PND */
   if (test->version.type == PND_VERSION_RELEASE)     type = PND_TYPE_RELEASE;
   else if (test->version.type == PND_VERSION_BETA)   type = PND_TYPE_BETA;
   else if (test->version.type == PND_VERSION_ALPHA)  type = PND_TYPE_ALPHA;

   DEBUG("");
   DEBUGP("ID:       %s\n", test->id);
   if (!strlen(test->id)) {
      DEBUG("Your code sucks, fix it!");
      _pndman_free_pnd(test);
      exit(EXIT_FAILURE);
   }

   /* titles */
   DEBUG("TITLE(S):");
   t = test->title;
   for (; t; t = t->next)
      DEBUGP("  %s:    %s\n", t->lang, t->string);

   /* descriptions */
   DEBUG("DESCRIPTION(S):");
   t = test->description;
   for (; t; t = t->next)
      DEBUGP("  %s:    %s\n", t->lang, t->string);

   DEBUGP("ICON:     %s\n", test->icon);
   DEBUGP("AUTHOR:   %s\n", test->author.name);
   DEBUGP("WEBSITE:  %s\n", test->author.website);
   DEBUGP("VERSION:  %s.%s.%s.%s %s\n", test->version.major, test->version.minor,
         test->version.release, test->version.build, type);

   app = test->app;
   for (; app; app = app->next) {
      if (app->exec.x11 == PND_EXEC_REQ)          x11 = PND_X11_REQ;
      else if (app->exec.x11 == PND_EXEC_STOP)    x11 = PND_X11_STOP;
      else if (app->exec.x11 == PND_EXEC_IGNORE)  x11 = PND_X11_IGNORE;

      if (app->version.type == PND_VERSION_RELEASE)     type = PND_TYPE_RELEASE;
      else if (app->version.type == PND_VERSION_BETA)   type = PND_TYPE_BETA;
      else if (app->version.type == PND_VERSION_ALPHA)  type = PND_TYPE_ALPHA;

      DEBUG("");
      DEBUGP("ID:       %s\n", app->id);

      /* titles */
      DEBUG("TITLE(S):");
      t = app->title;
      for (; t; t = t->next)
         DEBUGP("  %s:    %s\n", t->lang, t->string);

      /* descriptions */
      DEBUG("DESCRIPTION(S):");
      t = app->description;
      for (; t; t = t->next)
         DEBUGP("  %s:    %s\n", t->lang, t->string);

      DEBUGP("ICON:     %s\n", app->icon);
      DEBUGP("AUTHOR:   %s\n", app->author.name);
      DEBUGP("WEBSITE:  %s\n", app->author.website);
      DEBUGP("VERSION:  %s.%s.%s.%s %s\n", app->version.major, app->version.minor,
            app->version.release, app->version.build, type);
      DEBUGP("OSVER:    %s.%s.%s.%s\n", app->osversion.major, app->osversion.minor,
            app->osversion.release, app->osversion.build);

      DEBUGP("BKGRND:   %s\n", app->exec.background ? PND_TRUE : PND_FALSE);
      DEBUGP("STARTDIR: %s\n", app->exec.startdir);
      DEBUGP("STNDLONE: %s\n", app->exec.standalone ? PND_TRUE : PND_FALSE);
      DEBUGP("COMMAND:  %s\n", app->exec.command);
      DEBUGP("ARGS:     %s\n", app->exec.arguments);
      DEBUGP("X11:      %s\n", x11);
      DEBUGP("FREQ:     %d\n", app->frequency);

      /* previewpics */
      DEBUG("LICENSE(S):");
      l = app->license;
      for (; l; l = l->next)
         DEBUGP("  %s, %s, %s\n", l->name, l->url, l->sourcecodeurl);

      /* previewpics */
      DEBUG("ASSOCIATION(S):");
      a = app->association;
      for (; a; a = a->next)
         DEBUGP("  %s, %s, %s\n", a->name, a->filetype, a->exec);

      /* categories */
      DEBUG("CATEGORIE(S):");
      c = app->category;
      for (; c; c = c->next)
         DEBUGP("  %s, %s\n", c->main, c->sub);

      /* previewpics */
      DEBUG("PREVIEWPIC(S):");
      p = app->previewpic;
      for (; p; p = p->next)
         DEBUGP("  %s\n", p->src);
   }
   DEBUG("");
   _pndman_free_pnd(test);

   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
