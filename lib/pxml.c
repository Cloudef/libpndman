#include <stdio.h>
#include <string.h>
#include <expat.h>
#include <assert.h>
#include "pndman.h"
#include "package.h"

#define PXML_START_TAG "<PXML "
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
} pxml_parse;

/* \brief
 * Get PXML out of PND
 * TODO: Make it start seeking the start from bottom, so shit is faster for bigger PNDs */
static int _fetch_pxml_from_pnd(char *pnd_file, char *PXML, size_t *size)
{
   FILE *pnd;
   char s[LINE_MAX];
   char *ret = "\0";

   /* open PND */
   pnd = fopen(pnd_file, "rb");
   if (!pnd)
      return RETURN_FAIL;

   /* set */
   memset(s, 0, LINE_MAX);

   for (; ret && strncmp(s, PXML_START_TAG, strlen(PXML_START_TAG)); ret = fgets(s, PXML_TAG_SIZE, pnd));

   /* Yatta! PXML start found ^_^ */
   /* PXML does not define standard XML, so add those to not confuse expat */
   strncpy(PXML, XML_HEADER, PXML_MAX_SIZE);

   /* read until end tag */
   for (; ret && strncmp(s, PXML_END_TAG, strlen(PXML_END_TAG)); ret = fgets(s, LINE_MAX, pnd))
      strncat(PXML, s, PXML_MAX_SIZE);

   /* and we are done, was it so hard libpnd? */
   strncat(PXML, s, PXML_MAX_SIZE);

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
   for (; attrs[i]; ++i)
   {
      /* <package id= */
      if (!strncmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR)))
         strncpy(pnd->id, attrs[++i], PND_ID);
   }
}

/* \brief fills pndman_package's struct */
static void _pxml_pnd_icon_tag(pndman_package *pnd, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <icon src= */
      if (!strncmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(pnd->icon, attrs[++i], PND_PATH);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <application id= */
      if (!strncmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR)))
         strncpy(app->id, attrs[++i], PND_ID);
      /* <application appdata= */
      else if (!strncmp(attrs[i], PXML_APPDATA_ATTR, strlen(PXML_APPDATA_ATTR)))
         strncpy(app->appdata, attrs[++i], PND_PATH);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_icon_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <icon src= */
      if (!strncmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(app->icon, attrs[++i], PND_PATH);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_clockspeed_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <clockspeed frequency= */
      if (!strncmp(attrs[i], PXML_FREQUENCY_ATTR, strlen(PXML_FREQUENCY_ATTR)))
         ;
   }
}

/* \brief fills pndman_exec's structs */
static void _pxml_pnd_exec_tag(pndman_exec *exec, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <exec background= */
      if (!strncmp(attrs[i], PXML_BACKGROUND_ATTR, strlen(PXML_BACKGROUND_ATTR)))
         ;
      /* <exec startdir= */
      else if (!strncmp(attrs[i], PXML_STARTDIR_ATTR, strlen(PXML_STARTDIR_ATTR)))
         ;
      /* <exec standalone= */
      else if (!strncmp(attrs[i], PXML_STANDALONE_ATTR, strlen(PXML_STANDALONE_ATTR)))
         ;
      /* <exec command= */
      else if (!strncmp(attrs[i], PXML_COMMAND_ATTR, strlen(PXML_COMMAND_ATTR)))
         ;
      /* <exec arguments= */
      else if (!strncmp(attrs[i], PXML_ARGUMENTS_ATTR, strlen(PXML_ARGUMENTS_ATTR)))
         ;
      /* <exec x11= */
      else if (!strncmp(attrs[i], PXML_X11_ATTR, strlen(PXML_X11_ATTR)))
         ;

   }
}

/* \brief fills pndman_info's struct */
static void _pxml_pnd_info_tag(pndman_info *info, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <info name= */
      if (!strncmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         ;
      /* <info type= */
      else if (!strncmp(attrs[i], PXML_TYPE_ATTR, strlen(PXML_TYPE_ATTR)))
         ;
      /* <info src= */
      else if (!strncmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         ;
   }
}

/* \brief fills pndman_license's struct */
static void _pxml_pnd_application_license_tag(pndman_license *lic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <license name= */
      if (!strncmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         ;
      /* <license url= */
      else if (!strncmp(attrs[i], PXML_URL_ATTR, strlen(PXML_URL_ATTR)))
         ;
      /* <license sourcecodeurl= */
      else if (!strncmp(attrs[i], PXML_SOURCECODE_ATTR, strlen(PXML_SOURCECODE_ATTR)))
         ;
   }
}

/* \brief fills pndman_previewpic's struct */
static void _pxml_pnd_pic_tag(pndman_previewpic *pic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <pic src= */
      if (!strncmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         ;
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_category_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <category name= */
      if (!strncmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         ;
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_subcategory_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <subcategory name= */
      if (!strncmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         ;
   }
}

/* \brief fills pndman_associations's struct */
static void _pxml_pnd_association_tag(pndman_association *assoc, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <association name= */
      if (!strncmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         ;
      /* <association filetype= */
      else if (!strncmp(attrs[i], PXML_FILETYPE_ATTR, strlen(PXML_FILETYPE_ATTR)))
         ;
      /* <association exec= */
      else if (!strncmp(attrs[i], PXML_EXEC_ATTR, strlen(PXML_EXEC_ATTR)))
         ;
   }
}

/* \brief fills pndman_translated's struct */
static void _pxml_pnd_title_tag(pndman_translated *title, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <title lang= */
      if (!strncmp(attrs[i], PXML_LANG_ATTR, strlen(PXML_LANG_ATTR)))
         ;
   }
}

/* \brief fills pndman_translated's struct */
static void _pxml_pnd_description_tag(pndman_translated *desc, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <description lang= */
      if (!strncmp(attrs[i], PXML_LANG_ATTR, strlen(PXML_LANG_ATTR)))
         ;
   }
}

/* \brief fills pndman_author's structs */
static void _pxml_pnd_author_tag(pndman_author *author, char **attrs)
{
   int i = 0;
   for(; attrs[i]; ++i)
   {
      /* <author name= */
      if (!strncmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(author->name, attrs[++i], PND_NAME);
      else if(!strncmp(attrs[i], PXML_WEBSITE_ATTR, strlen(PXML_WEBSITE_ATTR)))
         strncpy(author->website, attrs[++i], PND_STR);
   }
}

/* \brief fills pndman_version's structs */
static void _pxml_pnd_version_tag(pndman_version *ver, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i)
   {
      /* <osversion/version major= */
      if (!strncmp(attrs[i], PXML_MAJOR_ATTR, strlen(PXML_MAJOR_ATTR)))
         strncpy(ver->major, attrs[++i], PND_VER);
      /* <osversion/version minor= */
      else if (!strncmp(attrs[i], PXML_MINOR_ATTR, strlen(PXML_MINOR_ATTR)))
         strncpy(ver->minor, attrs[++i], PND_VER);
      /* <osverion/version release= */
      else if (!strncmp(attrs[i], PXML_RELEASE_ATTR, strlen(PXML_RELEASE_ATTR)))
         strncpy(ver->release, attrs[++i], PND_VER);
      /* <osversion/version build= */
      else if (!strncmp(attrs[i], PXML_BUILD_ATTR, strlen(PXML_BUILD_ATTR)))
         strncpy(ver->build, attrs[++i], PND_VER);
      else if (!strncmp(attrs[i], PXML_TYPE_ATTR, strlen(PXML_TYPE_ATTR)))
         strncpy(ver->type, attrs[++i], PND_SHRT_STR);
   }
}

/* \brief Start element tag */
static void _pxml_pnd_start_tag(void *data, char *tag, char** attrs)
{
   unsigned int       *parse_state  = &((pxml_parse*)data)->state;
   pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   pndman_application *app          = ((pxml_parse*)data)->app;

   //printf("Found start : %s [%s, %s]\n", tag, attrs[0], attrs[1]);

   /* check parse state, so we don't parse wrong stuff */
   switch (*parse_state)
   {
      /* parse all outer elements */
      case PXML_PARSE_DEFAULT:
         /* <package */
         if (!strncmp(tag, PXML_PACKAGE_TAG, strlen(PXML_PACKAGE_TAG)))
         {
            _pxml_pnd_package_tag(pnd, attrs);
            *parse_state = PXML_PARSE_PACKAGE;
         }
         /* <application */
         else if (!strncmp(tag, PXML_APPLICATION_TAG, strlen(PXML_APPLICATION_TAG)))
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
         if (!strncmp(tag, PXML_TITLE_TAG, strlen(PXML_TITLE_TAG)))
         {
            /* new title here */
            // _pxml_pnd_title_tag(pnd, attrs);
         }
      break;
      /* parse <descriptions> inside <package> */
      case PXML_PARSE_PACKAGE_DESCRIPTIONS:
         /* <description */
         if (!strncmp(tag, PXML_DESCRIPTION_TAG, strlen(PXML_DESCRIPTION_TAG)))
         {
            /* new description here */
            // _pxml_pnd_description_tag(desc, attrs);
         }
      break;
      /* parse inside <package> */
      case PXML_PARSE_PACKAGE:
         /* <author */
         if (!strncmp(tag, PXML_AUTHOR_TAG, strlen(PXML_AUTHOR_TAG)))
            _pxml_pnd_author_tag(&pnd->author, attrs);
         /* <version */
         else if (!strncmp(tag, PXML_VERSION_TAG, strlen(PXML_VERSION_TAG)))
            _pxml_pnd_version_tag(&pnd->version, attrs);
         /* <icon */
         else if (!strncmp(tag, PXML_ICON_TAG, strlen(PXML_ICON_TAG)))
            _pxml_pnd_icon_tag(pnd, attrs);
         /* <titles */
         else if (!strncmp(tag, PXML_TITLES_TAG, strlen(PXML_TITLES_TAG)))
            *parse_state = PXML_PARSE_PACKAGE_TITLES;
         /* <descriptions */
         else if (!strncmp(tag, PXML_DESCRIPTIONS_TAG, strlen(PXML_DESCRIPTIONS_TAG)))
            *parse_state = PXML_PARSE_PACKAGE_DESCRIPTIONS;
      break;
      /* parse <titles> inside <application> */
      case PXML_PARSE_APPLICATION_TITLES:
         assert(app);
         /* <title */
         if (!strncmp(tag, PXML_TITLE_TAG, strlen(PXML_TITLE_TAG)))
         {
            /* create new title here */
            // _pxml_pnd_title_tag(title, attrs);
         }
      break;
      /* parse <descriptions> inside <application> */
      case PXML_PARSE_APPLICATION_DESCRIPTIONS:
         assert(app);
         /* <description */
         if (!strncmp(tag, PXML_DESCRIPTION_TAG, strlen(PXML_DESCRIPTION_TAG)))
         {
            /* create new description here */
            // _pxml_pnd_description_tag(desc, attrs);
         }
      break;
      /* parse <licenses> inside <application> */
      case PXML_PARSE_APPLICATION_LICENSES:
         assert(app);
         /* <license */
         if (!strncmp(tag, PXML_LICENSE_TAG, strlen(PXML_LICENSE_TAG)))
         {
            /* create new license here */
            // _pxml_pnd_license_tag(license, attrs);
         }
      break;
      /* parse <previewpics> inside <application> */
      case PXML_PARSE_APPLICATION_PREVIEWPICS:
         assert(app);
         /* <pic */
         if (!strncmp(tag, PXML_PIC_TAG, strlen(PXML_PIC_TAG)))
         {
            /* create new previewpic here */
            // _pxml_pnd_pic_tag(pic, attrs);
         }
      break;
      /* parse <categories> inside <application> */
      case PXML_PARSE_APPLICATION_CATEGORIES:
         assert(app);
         /* <category */
         if (!strncmp(tag, PXML_CATEGORY_TAG, strlen(PXML_LICENSE_TAG)))
         {
            /* create new category here */
            // _pxml_pnd_category_tag(cat, attrs);
            *parse_state = PXML_PARSE_APPLICATION_CATEGORY;
         }
      break;
      /* parse <category> inside <categories> */
      case PXML_PARSE_APPLICATION_CATEGORY:
         assert(app);
         /* <subcategory */
         if (!strncmp(tag, PXML_SUBCATEGORY_TAG, strlen(PXML_SUBCATEGORY_TAG)))
            // _pxml_pnd_subcategory_tag(cat, attrs);
      break;
      /* parse <associations> inside <application> */
      case PXML_PARSE_APPLICATION_ASSOCIATIONS:
         assert(app);
         /* <association */
         if (!strncmp(tag, PXML_ASSOCIATION_TAG, strlen(PXML_ASSOCIATION_TAG)))
         {
            /* create new association here */
            // _pxml_pnd_association_tag(assoc, attrs);
         }
      break;
      /* parse inside <application> */
      case PXML_PARSE_APPLICATION:
         assert(app);
         /* <exec */
         if (!strncmp(tag, PXML_EXEC_TAG, strlen(PXML_EXEC_TAG)))
            _pxml_pnd_exec_tag(&app->exec, attrs);
         /* <author */
         else if (!strncmp(tag, PXML_AUTHOR_TAG, strlen(PXML_AUTHOR_TAG)))
            _pxml_pnd_author_tag(&app->author, attrs);
         /* <osversion */
         else if (!strncmp(tag, PXML_OSVERSION_TAG, strlen(PXML_OSVERSION_TAG)))
            _pxml_pnd_version_tag(&app->osversion, attrs);
         /* <version */
         else if (!strncmp(tag, PXML_VERSION_TAG, strlen(PXML_VERSION_TAG)))
            _pxml_pnd_version_tag(&app->version, attrs);
         /* <icon */
         else if (!strncmp(tag, PXML_ICON_TAG, strlen(PXML_ICON_TAG)))
            _pxml_pnd_application_icon_tag(app, attrs);
         /* <info */
         else if (!strncmp(tag, PXML_INFO_TAG, strlen(PXML_INFO_TAG)))
            _pxml_pnd_info_tag(&app->info, attrs);
         /* <clockspeed */
         else if (!strncmp(tag, PXML_CLOCKSPEED_TAG, strlen(PXML_CLOCKSPEED_TAG)))
            _pxml_pnd_application_clockspeed_tag(app, attrs);
         /* <titles */
         else if (!strncmp(tag, PXML_TITLES_TAG, strlen(PXML_TITLES_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_TITLES;
         /* <descriptions */
         else if (!strncmp(tag, PXML_DESCRIPTIONS_TAG, strlen(PXML_DESCRIPTIONS_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_DESCRIPTIONS;
         /* <licenses */
         else if (!strncmp(tag, PXML_LICENSES_TAG, strlen(PXML_LICENSES_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_LICENSES;
         /* <previewpics */
         else if (!strncmp(tag, PXML_PREVIEWPICS_TAG, strlen(PXML_PREVIEWPICS_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_PREVIEWPICS;
         /* <categories */
         else if (!strncmp(tag, PXML_CATEGORIES_TAG, strlen(PXML_CATEGORIES_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_CATEGORIES;
         /* <associations */
         else if (!strncmp(tag, PXML_ASSOCIATIONS_TAG, strlen(PXML_ASSOCIATIONS_TAG)))
            *parse_state = PXML_PARSE_APPLICATION_ASSOCIATIONS;
         /* === BACKWARDS COMPATIBILITY === */
         /* These are okay to ignore if new methods are used */
         /* <title */
         else if (!strncmp(tag, PXML_TITLE_TAG, strlen(PXML_TITLE_TAG)))
         {
            /* new title here */
            // _pxml_pnd_title_tag(title, attrs);
         }
         /* <description */
         else if (!strncmp(tag, PXML_DESCRIPTION_TAG, strlen(PXML_DESCRIPTION_TAG)))
         {
            /* new description here */
            // _pxml_pnd_description_tag(desc, attrs);
         }
      break;
   }
}

/* \brief End element tag */
static void _pxml_pnd_end_tag(void *data, char *tag)
{
   //printf("Found end : %s\n", tag);

   unsigned int          *parse_state  = &((pxml_parse*)data)->state;
   // pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   // pndman_application *app          = ((pxml_parse*)data)->app;

   /* </package> */
   if (!strncmp(tag, PXML_PACKAGE_TAG, strlen(PXML_PACKAGE_TAG)))
      *parse_state = PXML_PARSE_DEFAULT;
   /* </application */
   else if (!strncmp(tag, PXML_APPLICATION_TAG, strlen(PXML_APPLICATION_TAG)))
   {
      *parse_state = PXML_PARSE_PACKAGE;
      ((pxml_parse*)data)->app = NULL;
   }
   /* </titles> */
   else if (!strncmp(tag, PXML_TITLES_TAG, strlen(PXML_TITLES_TAG)))
   {
      if (*parse_state == PXML_PARSE_PACKAGE_TITLES)
         *parse_state = PXML_PARSE_PACKAGE;
      else if (*parse_state == PXML_PARSE_APPLICATION_TITLES)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </descriptions> */
   else if (!strncmp(tag, PXML_DESCRIPTIONS_TAG, strlen(PXML_DESCRIPTIONS_TAG)))
   {
      if (*parse_state == PXML_PARSE_PACKAGE_DESCRIPTIONS)
         *parse_state = PXML_PARSE_PACKAGE;
      else if (*parse_state == PXML_PARSE_APPLICATION_DESCRIPTIONS)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </licenses> */
   else if (!strncmp(tag, PXML_LICENSES_TAG, strlen(PXML_LICENSES_TAG)))
   {
      if (*parse_state == PXML_PARSE_APPLICATION_LICENSES)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </licenses> */
   else if (!strncmp(tag, PXML_PREVIEWPICS_TAG, strlen(PXML_PREVIEWPICS_TAG)))
   {
      if (*parse_state == PXML_PARSE_APPLICATION_PREVIEWPICS)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </categories> */
   else if (!strncmp(tag, PXML_CATEGORIES_TAG, strlen(PXML_CATEGORIES_TAG)))
   {
      if (*parse_state == PXML_PARSE_APPLICATION_CATEGORIES)
         *parse_state = PXML_PARSE_APPLICATION;
      else
         *parse_state = PXML_PARSE_DEFAULT;
   }
   /* </associations> */
   else if (!strncmp(tag, PXML_ASSOCIATIONS_TAG, strlen(PXML_ASSOCIATIONS_TAG)))
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

   /* parse XML */
   ret = RETURN_OK;
   if (XML_Parse(xml, PXML, size, 1) == 0)
      ret = RETURN_FAIL;

   /* free the parser */
   XML_ParserFree(xml);

   return ret;
}

/* \brief Post-process pndman_package */
static void _pxml_pnd_post_process(pndman_package *pnd)
{
   /* no need to do anything */
   if (!pnd->app)
      return;

   /* header */
   if (!strlen(pnd->id) && strlen(pnd->app->id))
      strcpy(pnd->id, pnd->app->id);
   if (!strlen(pnd->icon) && strlen(pnd->app->icon))
      strcpy(pnd->icon, pnd->app->icon);

   /* author */
   if (!strlen(pnd->author.name) && strlen(pnd->app->author.name))
      strcpy(pnd->author.name, pnd->app->author.name);
   if (!strlen(pnd->author.website) && strlen(pnd->app->author.website))
      strcpy(pnd->author.website, pnd->app->author.website);

   /* version */
   if (!strlen(pnd->version.major) && strlen(pnd->app->version.major))
      strcpy(pnd->version.major, pnd->app->version.major);
   if (!strlen(pnd->version.minor) && strlen(pnd->app->version.minor))
      strcpy(pnd->version.minor, pnd->app->version.minor);
   if (!strlen(pnd->version.release) && strlen(pnd->app->version.release))
      strcpy(pnd->version.release, pnd->app->version.release);
   if (!strlen(pnd->version.build) && strlen(pnd->app->version.build))
      strcpy(pnd->version.build, pnd->app->version.build);
   if (!strlen(pnd->version.type) && strlen(pnd->app->version.type))
      strcpy(pnd->version.type, pnd->app->version.type);
}

/* \brief
 * Stub test function */
int pnd_do_something(char *pnd_file)
{
   char PXML[PXML_MAX_SIZE];
   size_t size = 0;

   _fetch_pxml_from_pnd(pnd_file, PXML, &size);

   puts(PXML);
   puts("======");

   pndman_package *test;
   test = _pndman_new_pnd();

   pxml_parse data;
   data.pnd   = test;
   data.app   = NULL;
   data.state = PXML_PARSE_DEFAULT;

   /* parse */
   _pxml_pnd_parse(&data, PXML, size);
   _pxml_pnd_post_process(test);

   puts("======");

   /* debug filled PND */
   printf("ID:       %s\n", test->id);
   printf("ICON:     %s\n", test->icon);
   printf("AUTHOR:   %s\n", test->author.name);
   printf("WEBSITE:  %s\n", test->author.website);
   printf("VERSION:  %s.%s.%s.%s %s\n", test->version.major, test->version.minor,
         test->version.release, test->version.build, test->version.type);


   _pndman_free_pnd(test);

   return RETURN_OK;
}
