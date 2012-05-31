#include "internal.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <expat.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>

#ifndef _WIN32
#  include <sys/stat.h>
#  include <dirent.h>
#endif

#define PXML_START_TAG     "<PXML"
#define PXML_END_TAG       "</PXML>"
#define PXML_MAX_SIZE      1024 * 1024
#define PXML_WINDOW        4096
#define PXML_WINDOW_FRACT  PXML_WINDOW-10
#define XML_HEADER         "<?xml version=\"1.1\" encoding=\"UTF-8\"?>"
#define XMLSTART()         pos = 0; strip = '<';
#define XMLCOPY(x,y,z)     if (PXML_MAX_SIZE>pos+z) { memcpy(PXML+x,y,z); pos +=z; } else goto xml_too_big;

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

/* \brief search tag from string. NOTE: we do correct incorrect XML too so string changes */
static char* _match_tag(char *s, size_t len, char *tag, size_t *p, char *strip)
{
   unsigned int nlen;
   char *end;

   nlen = strlen(tag);
   end  = s + len - (strip?0:nlen);
   *p   = 0;

   while (s < end) {
      /* don't even try unprintable characters */
      if (isprint(*s)) {
         /* tag found */
         if (!_strnupcmp(s, tag, nlen)) return &*s;
         /* fugly stripping code, thank you everyone who are using invalid PXML in their PNDs :) */
         else if (strip) {
            if (!*strip || *strip == '?') {
               if (*strip == '?') {
                  if (*s == '!') {
                     *strip = '!';                                                        /* enter comment area */
                     if (*p) *(s-1) = ' '; *s = ' ';                                      /* strip check characters */
                  } else {
                     *strip = '<';                                                        /* enter tag area */
                     if (*s == '&' || *s == '>') *s = ' ';                                /* strip this */
                     else if (*s == '>') *strip = 0;                                      /* instantly leave */
                  }
               } else if (*s == '<' || *s == '>' || *s == '"' || *s == '\'') {
                  if (*s != '<') *strip = *s; else *strip = '?';                          /* enter strip area */
               }
            } else {
               /* strip comments */
               if (*strip == '!') {
                  if (*s == '-') *strip = '-';                                            /* end comment on -> */
                  *s = ' ';                                                               /* inside comments, strip everything */
               } else if (*strip == '-') {
                  /* check if comment ends */
                       if (*s == '>') *strip = 0;
                  else if (*s == '-') *strip = '-'; else *strip = '!';
                  *s = ' ';
               } else {
                  /* strip non comment */
                       if (*strip == '<') { if (*s == '>')  *strip = 0; }                 /* end bracket */
                  else if (*strip == '>') { if (*s == '<')  *strip = 0; }                 /* end bracket */
                  else if (*s == *strip || *s == '<')       *strip = 0;                   /* end quote */
                       if (*strip && (*s == '&' || *s == '>')) *s = ' ';                  /* get rid of bad XML */
               }
            }
         }
      }
      ++s; ++*p;
   }
   return NULL;
}

/* \brief
 * Get PXML out of PND  */
static int _fetch_pxml_from_pnd(const char *pnd_file, char *PXML, size_t *size)
{
   FILE     *pnd;
   char     s[PXML_WINDOW];
   char     *match, strip;
   size_t   pos, len, ret, read, stag, etag;

   /* open PND */
   if (!(pnd = fopen(pnd_file, "rb")))
      goto read_fail;

   /* seek to end, and see if we should seek back */
   memset(s,  0, PXML_WINDOW);
   fseek(pnd, 0, SEEK_END);
   if ((len = ftell(pnd)) > PXML_WINDOW) {
      pos  = len - PXML_WINDOW;
      read = PXML_WINDOW;
   } else {
      pos  = 0;
      read = len;
   }

   /* hackety, hackety, hack */
   while (1) {
      /* seek */
      fseek(pnd, pos, SEEK_SET);

      /* read until start tag */
      if ((ret = fread(s, 1, read, pnd))) {
         // if (!memcmp(s, PXML_START_TAG, strlen(PXML_START_TAG))) break;
         if ((match = _match_tag(s, read, PXML_START_TAG, &stag, NULL))) break;
      }

      if (!ret) break; ret = 0;              /* below breaks are failures */
      if (!pos) break;                       /* nothing more to read */
      else if (pos > PXML_WINDOW_FRACT) {    /* seek back */
         pos -= PXML_WINDOW_FRACT;
         read = PXML_WINDOW;
         if (len - pos > (500*1024)) break;
      } else {                               /* read final segment */
         read = PXML_WINDOW - pos;
         memset(s + pos, 0, PXML_WINDOW - pos);
         pos = 0;
      }
   }

   /* failure? */
   if (!ret)
      goto fail_start;

   /* Yatta! PXML start found ^_^ */
   /* PXML does not define standard XML, so add those to not confuse expat */
   XMLSTART();
   XMLCOPY(0, XML_HEADER, strlen(XML_HEADER));

   /* check if end tag is on the same buffer as start tag */
   if (!_match_tag(match+strlen(PXML_START_TAG), read-stag-strlen(PXML_START_TAG), PXML_END_TAG, &etag, &strip)) {
      /* nope, copy first buffer and read to end */
      XMLCOPY(pos, match, read-stag);
      while ((ret = fread(s, 1, read, pnd))) {
         match = s;
         if (_match_tag(s, read, PXML_END_TAG, &etag, &strip)) break;
         XMLCOPY(pos, s, read);
      }
   } else etag += strlen(PXML_START_TAG);

   /* read fail */
   if (!ret)
      goto fail_end;

   /* finish */
   XMLCOPY(pos, match, etag+strlen(PXML_END_TAG));

   /* store size */
   *size = pos;

   /* close the file */
   fclose(pnd);
   return RETURN_OK;

read_fail:
   DEBFAIL(READ_FAIL, pnd_file);
   goto fail;
fail_start:
   DEBFAIL("%s: %s", pnd_file, PXML_START_TAG_FAIL);
   goto fail;
fail_end:
   DEBFAIL("%s: %s", pnd_file, PXML_END_TAG_FAIL);
   goto fail;
xml_too_big:
   DEBFAIL("%s: %s", pnd_file, PXML_XML_CPY_FAIL);
fail:
   IFDO(fclose, pnd);
   return RETURN_FAIL;
}

/* \brief fills pndman_package's struct */
static void _pxml_pnd_package_tag(pndman_package *pnd, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <package id= */
      if (!memcmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR)))
         strncpy(pnd->id, attrs[++i], PNDMAN_ID-1);
   }
}

/* \brief fills pndman_package's struct */
static void _pxml_pnd_icon_tag(pndman_package *pnd, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <icon src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(pnd->icon, attrs[++i], PNDMAN_PATH-1);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <application id= */
      if (!memcmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR)))
         strncpy(app->id, attrs[++i], PNDMAN_ID-1);
      /* <application appdata= */
      else if (!memcmp(attrs[i], PXML_APPDATA_ATTR, strlen(PXML_APPDATA_ATTR)))
         strncpy(app->appdata, attrs[++i], PNDMAN_PATH-1);
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_icon_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <icon src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(app->icon, attrs[++i], PNDMAN_PATH-1);
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
         if (!_strupcmp(attrs[++i], PNDMAN_TRUE)) exec->background = 1;
      }
      /* <exec startdir= */
      else if (!memcmp(attrs[i], PXML_STARTDIR_ATTR, strlen(PXML_STARTDIR_ATTR)))
         strncpy(exec->startdir, attrs[++i], PNDMAN_PATH-1);
      /* <exec standalone= */
      else if (!memcmp(attrs[i], PXML_STANDALONE_ATTR, strlen(PXML_STANDALONE_ATTR)))
      {
         if (!memcmp(attrs[++i], PNDMAN_FALSE, strlen(PNDMAN_FALSE))) exec->standalone = 0;
      }
      /* <exec command= */
      else if (!memcmp(attrs[i], PXML_COMMAND_ATTR, strlen(PXML_COMMAND_ATTR)))
         strncpy(exec->command, attrs[++i], PNDMAN_PATH-1);
      /* <exec arguments= */
      else if (!memcmp(attrs[i], PXML_ARGUMENTS_ATTR, strlen(PXML_ARGUMENTS_ATTR)))
         strncpy(exec->arguments, attrs[++i], PNDMAN_STR-1);
      /* <exec x11= */
      else if (!memcmp(attrs[i], PXML_X11_ATTR, strlen(PXML_X11_ATTR)))
      {
         ++i;
         if (!_strupcmp(attrs[i], PND_X11_REQ))          exec->x11 = PND_EXEC_REQ;
         else if (!_strupcmp(attrs[i], PND_X11_STOP))    exec->x11 = PND_EXEC_STOP;
         else if (!_strupcmp(attrs[i], PND_X11_IGNORE))  exec->x11 = PND_EXEC_IGNORE;
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
         strncpy(info->name, attrs[++i], PNDMAN_NAME-1);
      /* <info type= */
      else if (!memcmp(attrs[i], PXML_TYPE_ATTR, strlen(PXML_TYPE_ATTR)))
         strncpy(info->type, attrs[++i], PNDMAN_SHRT_STR-1);
      /* <info src= */
      else if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(info->src, attrs[++i], PNDMAN_PATH-1);
   }
}

/* \brief fills pndman_license's struct */
static void _pxml_pnd_license_tag(pndman_license *lic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <license name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(lic->name, attrs[++i], PNDMAN_SHRT_STR-1);
      /* <license url= */
      else if (!memcmp(attrs[i], PXML_URL_ATTR, strlen(PXML_URL_ATTR)))
         strncpy(lic->url, attrs[++i], PNDMAN_STR-1);
      /* <license sourcecodeurl= */
      else if (!memcmp(attrs[i], PXML_SOURCECODE_ATTR, strlen(PXML_SOURCECODE_ATTR)))
         strncpy(lic->sourcecodeurl, attrs[++i], PNDMAN_STR-1);
   }
}

/* \brief fills pndman_previewpic's struct */
static void _pxml_pnd_pic_tag(pndman_previewpic *pic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <pic src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR)))
         strncpy(pic->src, attrs[++i], PNDMAN_PATH-1);
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_category_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <category name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(cat->main, attrs[++i], PNDMAN_SHRT_STR-1);
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_subcategory_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <subcategory name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(cat->sub, attrs[++i], PNDMAN_SHRT_STR-1);
   }
}

/* \brief fills pndman_associations's struct */
static void _pxml_pnd_association_tag(pndman_association *assoc, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <association name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(assoc->name, attrs[++i], PNDMAN_STR-1);
      /* <association filetype= */
      else if (!memcmp(attrs[i], PXML_FILETYPE_ATTR, strlen(PXML_FILETYPE_ATTR)))
         strncpy(assoc->filetype, attrs[++i], PNDMAN_SHRT_STR-1);
      /* <association exec= */
      else if (!memcmp(attrs[i], PXML_EXEC_ATTR, strlen(PXML_EXEC_ATTR)))
         strncpy(assoc->exec, attrs[++i], PNDMAN_PATH-1);
   }
}

/* \brief fills pndman_translated's struct */
static void _pxml_pnd_translated_tag(pndman_translated *title, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <title/description lang= */
      if (!memcmp(attrs[i], PXML_LANG_ATTR, strlen(PXML_LANG_ATTR)))
         strncpy(title->lang, attrs[++i], PNDMAN_SHRT_STR-1);
   }
}

/* \brief fills pndman_author's structs */
static void _pxml_pnd_author_tag(pndman_author *author, char **attrs)
{
   int i = 0;
   for(; attrs[i]; ++i) {
      /* <author name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR)))
         strncpy(author->name, attrs[++i], PNDMAN_NAME-1);
      /* <author website= */
      else if(!memcmp(attrs[i], PXML_WEBSITE_ATTR, strlen(PXML_WEBSITE_ATTR)))
         strncpy(author->website, attrs[++i], PNDMAN_STR-1);
      /* <author email= */
      else if(!memcmp(attrs[i], PXML_EMAIL_ATTR, strlen(PXML_EMAIL_ATTR)))
         strncpy(author->email, attrs[++i], PNDMAN_STR-1);
   }
}

/* \brief fills pndman_version's structs */
static void _pxml_pnd_version_tag(pndman_version *ver, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <osversion/version major= */
      if (!memcmp(attrs[i], PXML_MAJOR_ATTR, strlen(PXML_MAJOR_ATTR)))
         strncpy(ver->major, attrs[++i], PNDMAN_VERSION-1);
      /* <osversion/version minor= */
      else if (!memcmp(attrs[i], PXML_MINOR_ATTR, strlen(PXML_MINOR_ATTR)))
         strncpy(ver->minor, attrs[++i], PNDMAN_VERSION-1);
      /* <osverion/version release= */
      else if (!memcmp(attrs[i], PXML_RELEASE_ATTR, strlen(PXML_RELEASE_ATTR)))
         strncpy(ver->release, attrs[++i], PNDMAN_VERSION-1);
      /* <osversion/version build= */
      else if (!memcmp(attrs[i], PXML_BUILD_ATTR, strlen(PXML_BUILD_ATTR)))
         strncpy(ver->build, attrs[++i], PNDMAN_VERSION-1);
      else if (!memcmp(attrs[i], PXML_TYPE_ATTR, strlen(PXML_TYPE_ATTR)))
      {
         ++i;
         if (!_strupcmp(PND_TYPE_RELEASE, attrs[i]))    ver->type = PND_VERSION_RELEASE;
         else if (!_strupcmp(PND_TYPE_BETA, attrs[i]))  ver->type = PND_VERSION_BETA;
         else if (!_strupcmp(PND_TYPE_ALPHA, attrs[i])) ver->type = PND_VERSION_ALPHA;
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

   //DEBUG(PNDMAN_LEVEL_CRAP, "Found start : %s [%s, %s]", tag, attrs[0], attrs[1]);

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
         if (((pxml_parse*)data)->bckward_title)
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
         if (((pxml_parse*)data)->bckward_desc)
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

/* \brief copy string with special care, returns number of characters copied */
static int _cstrncpy(char *dst, char *src, int len)
{
   int i, p, nospace;
   assert(dst && src);
   if (!len) return 0;
   p = 0; nospace = 0;
   for (i = 0; i != len; ++i) {
      if (!isspace(src[i])) nospace = 1;
      if (isprint(src[i]) && src[i] != '\n' &&
          src[i] != '\r'  && src[i] != '\t' && nospace) {
         dst[p++] = src[i];
      }
   }
   return p;
}

/* \brief Text data */
static void _pxml_pnd_data(void *data, char *text, int len)
{
   // unsigned int       *parse_state  = &((pxml_parse*)data)->state;
   // pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   // pndman_application *app          = ((pxml_parse*)data)->app;
   char                  *ptr          = ((pxml_parse*)data)->data;

   if (!ptr) return;
   if (len < PNDMAN_STR)
      if (_cstrncpy(ptr, text, len)) ((pxml_parse*)data)->data = NULL;
}

/* \brief End element tag */
static void _pxml_pnd_end_tag(void *data, char *tag)
{
   // DEBUG(PNDMAN_LEVEL_CRAP, "Found end : %s", tag);

   unsigned int          *parse_state  = &((pxml_parse*)data)->state;
   // pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   // pndman_application *app          = ((pxml_parse*)data)->app;

   /* </package> */
   if (!memcmp(tag, PXML_PACKAGE_TAG, strlen(PXML_PACKAGE_TAG)))
      *parse_state = PXML_PARSE_DEFAULT;
   /* </application */
   else if (!memcmp(tag, PXML_APPLICATION_TAG, strlen(PXML_APPLICATION_TAG)))
   {
      *parse_state = PXML_PARSE_DEFAULT;
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
   if (!xml) goto fail;

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

   if (ret == RETURN_FAIL)
      DEBUG(PNDMAN_LEVEL_WARN, PXML_INVALID_XML,
            XML_ErrorString(XML_GetErrorCode(xml)), PXML);

   /* free the parser */
   XML_ParserFree(xml);

   return ret;

fail:
   DEBFAIL(PXML_EXPAT_FAIL);
   return RETURN_FAIL;
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
      memcpy(pnd->id, pnd->app->id, PNDMAN_ID);
   if (!strlen(pnd->icon) && strlen(pnd->app->icon))
      memcpy(pnd->icon, pnd->app->icon, PNDMAN_PATH);

   /* author */
   if (!strlen(pnd->author.name) && strlen(pnd->app->author.name))
      memcpy(pnd->author.name, pnd->app->author.name, PNDMAN_NAME);
   if (!strlen(pnd->author.website) && strlen(pnd->app->author.website))
      memcpy(pnd->author.website, pnd->app->author.website, PNDMAN_STR);

   /* version */
   if (!strlen(pnd->version.major) && strlen(pnd->app->version.major))
      memcpy(pnd->version.major, pnd->app->version.major, PNDMAN_VERSION);
   if (!strlen(pnd->version.minor) && strlen(pnd->app->version.minor))
      memcpy(pnd->version.minor, pnd->app->version.minor, PNDMAN_VERSION);
   if (!strlen(pnd->version.release) && strlen(pnd->app->version.release))
      memcpy(pnd->version.release, pnd->app->version.release, PNDMAN_VERSION);
   if (!strlen(pnd->version.build) && strlen(pnd->app->version.build))
      memcpy(pnd->version.build, pnd->app->version.build, PNDMAN_VERSION);
   if (pnd->version.type != pnd->app->version.type && pnd->version.type == PND_VERSION_RELEASE)
      pnd->version.type = pnd->app->version.type;

   /* titles */
   if (!pnd->title && pnd->app->title) {
      t = pnd->app->title;
      for (; t; t = t->next) {
         if ((tc = _pndman_package_new_title(pnd))) {
            memcpy(tc->lang, t->lang, PNDMAN_SHRT_STR);
            memcpy(tc->string, t->string, PNDMAN_STR);
         }
      }
   }

   /* descriptions */
   if (!pnd->description && pnd->app->description) {
      t = pnd->app->description;
      for (; t; t = t->next) {
         if ((tc = _pndman_package_new_description(pnd))) {
            memcpy(tc->lang, t->lang, PNDMAN_SHRT_STR);
            memcpy(tc->string, t->string, PNDMAN_STR);
         }
      }
   }

   /* licenses */
   if (!pnd->license && pnd->app->license) {
      l = pnd->app->license;
      for (; l; l = l->next) {
         if ((lc = _pndman_package_new_license(pnd))) {
            memcpy(lc->name, l->name, PNDMAN_SHRT_STR);
            memcpy(lc->url, l->url, PNDMAN_STR);
            memcpy(lc->sourcecodeurl, l->sourcecodeurl, PNDMAN_STR);
         }
      }
   }

   /* previewpics */
   if (!pnd->previewpic && pnd->app->previewpic) {
      p = pnd->app->previewpic;
      for (; p; p = p->next) {
         if ((pc = _pndman_package_new_previewpic(pnd))) {
            memcpy(pc->src, p->src, PNDMAN_PATH);
         }
      }
   }

   /* categories */
   if (!pnd->category && pnd->app->category) {
      c = pnd->app->category;
      for (; c; c = c->next) {
         if ((cc = _pndman_package_new_category(pnd))) {
            memcpy(cc->main, c->main, PNDMAN_SHRT_STR);
            memcpy(cc->sub, c->sub, PNDMAN_SHRT_STR);
         }
      }
   }
}

/* \brief Process crawl */
static int _pndman_crawl_process(const char *path,
      const char *relative, pxml_parse *data)
{
   char PXML[PXML_MAX_SIZE];
   char full_path[PNDMAN_PATH];
   size_t size = 0;
   FILE *f;
   assert(path && relative && data);

   memset(full_path, 0, PNDMAN_PATH-1);
   strncpy(full_path, path, PNDMAN_PATH-1);
   strncat(full_path, "/", PNDMAN_PATH-1);
   strncat(full_path, relative, PNDMAN_PATH-1);

   memset(PXML, 0, PXML_MAX_SIZE);
   if (_fetch_pxml_from_pnd(full_path, PXML, &size) != RETURN_OK)
      goto fail;

   /* reset some stuff before crawling for post process */
   memset(data->pnd->version.major,  0, PNDMAN_VERSION);
   memset(data->pnd->version.minor,  0, PNDMAN_VERSION);
   memset(data->pnd->version.release,0, PNDMAN_VERSION);
   memset(data->pnd->version.build,  0, PNDMAN_VERSION);

   /* parse */
   if (_pxml_pnd_parse(data, PXML, size) != RETURN_OK)
      goto parse_fail;

   _pxml_pnd_post_process(data->pnd);

   /* add size to the pnd */
   if ((f = fopen(full_path, "rb"))) {
      fseek(f, 0, SEEK_END);
      data->pnd->size = ftell(f);
      fclose(f);
   }

   /* add path to the pnd */
   strncpy(data->pnd->path, relative, PNDMAN_PATH-1);
   return RETURN_OK;

parse_fail:
   DEBFAIL(PXML_PND_PARSE_FAIL, relative);
fail:
   return RETURN_FAIL;
}

/* \brief Crawl directory */
static int _pndman_crawl_dir(const char *path,
      const char *relative, pndman_package *list)
{
   char tmp[PNDMAN_PATH], relav[PNDMAN_PATH];
   pxml_parse data;
   pndman_package *pnd, *p;
   int ret;

#ifdef __linux__
   DIR *dp;
   struct dirent *ep;
#elif _WIN32
   WIN32_FIND_DATA dp;
   HANDLE hFind;
#endif

   assert(path && list);

   /* init parse data */
   pnd = NULL; p = NULL;
   ret = 0;

   memset(relav, 0, PNDMAN_PATH);
   memset(tmp, 0, PNDMAN_PATH);

   /* copy full */
   strncpy(tmp, path, PNDMAN_PATH-1);
   strncat(tmp, "/", PNDMAN_PATH-1);
   strncat(tmp, relative, PNDMAN_PATH-1);

   /* jump to end of list */
   if (strlen(list->id)) for (p = list; p && p->next; p = p->next);

#ifndef _WIN32 /* POSIX */
   dp = opendir(tmp);
   if (!dp) return 0;

   while (ep = readdir(dp)) {
      if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")) continue; /* no we don't want this! */
      /* recrusive */
      if (ep->d_type == DT_DIR) {
         strncpy(relav, relative, PNDMAN_PATH-1);
         strncat(relav, "/", PNDMAN_PATH-1);
         strncat(relav, ep->d_name, PNDMAN_PATH-1);
         ret += _pndman_crawl_dir(path, relav, list);
         continue;
      }
      if (ep->d_type != DT_REG) continue;                /* we only want regular files */
      if (!_strupstr(ep->d_name, ".pnd")) continue;     /* we only want .pnd files */
      strncpy(relav, relative, PNDMAN_PATH-1);
      strncat(relav, "/", PNDMAN_PATH-1);
      strncat(relav, ep->d_name, PNDMAN_PATH-1);

      /* create pnd */
      pnd = _pndman_new_pnd();
      if (!pnd) continue;

      /* crawl */
      data.pnd   = pnd;
      data.app   = NULL;
      data.data  = NULL;
      data.bckward_title = 1; /* backwards compatibility with PXML titles */
      data.bckward_desc  = 1; /* backwards compatibility with PXML descriptions */
      data.state = PXML_PARSE_DEFAULT;
      if (_pndman_crawl_process(path, relav, &data) != RETURN_OK) {
         while ((pnd = _pndman_free_pnd(pnd)));
         continue;
      }

      /* assign pnd to list */
      if (!p) {
         _pndman_copy_pnd(list, pnd);
         while ((pnd = _pndman_free_pnd(pnd)));
         p = list;
      } else {
         for (p = list; p && p->next; p = p->next);
         p->next = pnd;
      }
      ++ret;
   }
   closedir(dp);
#else /* WIN32 */

   /* do this for wildcard search */
   strncat(tmp, "/*", PNDMAN_PATH-1);
   if ((hFind = FindFirstFile(tmp, &dp)) == INVALID_HANDLE_VALUE)
      return 0;

   /* copy full path again */
   strncpy(tmp, path, PNDMAN_PATH-1);
   strncat(tmp, "/", PNDMAN_PATH-1);
   strncat(tmp, relative, PNDMAN_PATH-1);

   do {
      if (!strcmp(dp.cFileName, ".") || !strcmp(dp.cFileName, "..")) continue; /* we don't want this */
      /* recrusive */
      if (dp.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
         strncpy(relav, relative, PNDMAN_PATH-1);
         strncat(relav, "/", PNDMAN_PATH-1);
         strncat(relav, dp.cFileName, PNDMAN_PATH-1);
         ret += _pndman_crawl_dir(path, relav, list);
         continue;
      }
      if (!_strupstr(dp.cFileName, ".pnd")) continue; /* we only want .pnd files */
      strncpy(relav, relative, PNDMAN_PATH-1);
      strncat(relav, "/", PNDMAN_PATH-1);
      strncat(relav, dp.cFileName, PNDMAN_PATH-1);

      /* create pnd */
      pnd = _pndman_new_pnd();
      if (!pnd) continue;

      /* crawl */
      data.pnd   = pnd;
      data.app   = NULL;
      data.data  = NULL;
      data.bckward_title = 1; /* backwards compatibility with PXML titles */
      data.bckward_desc  = 1; /* backwards compatibility with PXML descriptions */
      data.state = PXML_PARSE_DEFAULT;
      if (_pndman_crawl_process(path, relav, &data) != RETURN_OK) {
         while ((pnd = _pndman_free_pnd(pnd)));
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
   char tmp[PNDMAN_PATH];
   int ret = 0;
   assert(device && list);
   memset(tmp,  0, PNDMAN_PATH);

   /* pandora root */
   strncpy(tmp, device->mount, PNDMAN_PATH-1);

   /* can't access */
   if (access(tmp, F_OK) != 0)
      goto fail;

   /* desktop */
   ret += _pndman_crawl_dir(tmp, "pandora/desktop", list);

   /* menu */
   ret += _pndman_crawl_dir(tmp, "pandora/menu", list);

   /* apps */
   ret += _pndman_crawl_dir(tmp, "pnadora/apps", list);

   return ret;

fail:
   DEBFAIL(ACCESS_FAIL, tmp);
   return RETURN_FAIL;
}

/* \brief crawl to repository, return number of pnd's found, and -1 on error */
static int _pndman_crawl_to_repository(int full, pndman_device *device, pndman_repository *local)
{
   pndman_package *list, *p, *n = NULL, *pnd;
   char path[PNDMAN_PATH];
   struct stat st;
   int ret;
   assert(device && local);

   list = _pndman_new_pnd();
   if (!list) return RETURN_FAIL;

   /* crawl pnds to list */
   ret = _pndman_crawl_to_pnd_list(device, list);

   /* either crawl failed, or no pnd's found */
   if (ret <= 0) {
      for (p = list; p; p = n) {
         n = p->next;
         while ((p = _pndman_free_pnd(p)));
      }
      return ret;
   }

   /* merge pnd's to repo */
   for (p = list; p; p = n) {
      pnd = _pndman_repository_new_pnd_check(p, p->path, device->mount, local);
      if (pnd) {
         /* copy needed stuff over */
         if (!full) _pndman_package_free_applications(pnd);
         _pndman_copy_pnd(pnd, p);
         strcpy(pnd->mount, device->mount);
         _pndman_pnd_get_path(pnd, path);

         /* stat for modified time */
         if (!pnd->modified_time && stat(path, &st) == 0)
            pnd->modified_time = st.st_mtime;
      }

      /* free */
      n = p->next;
      while ((p = _pndman_free_pnd(p)));
   }

   return ret;
}

/* API */

/* \brief crawl pnds to local repository, returns number of pnd's found, and -1 on error
 * The full_crawl option allows you to specify wether to crawl application data from PND as well */
PNDMANAPI int pndman_package_crawl(int full_crawl,
      pndman_device *device, pndman_repository *local)
{
   CHECKUSE(device);
   CHECKUSE(local);
   local = _pndman_repository_first(local);
   return _pndman_crawl_to_repository(full_crawl, device, local);
}

/* \brief fill single PND's data fully by crawling it locally */
PNDMANAPI int pndman_package_crawl_single_package(int full_crawl,
      pndman_package *pnd)
{
   char path[PNDMAN_PATH];
   pxml_parse data;
   struct stat st;
   CHECKUSE(pnd);

   _pndman_pnd_get_path(pnd, path);
   data.pnd   = pnd;
   data.app   = NULL;
   data.data  = NULL;
   data.bckward_title = 1; /* backwards compatibility with PXML titles */
   data.bckward_desc  = 1; /* backwards compatibility with PXML descriptions */
   data.state = PXML_PARSE_DEFAULT;

   if (_pndman_crawl_process(path, pnd->path, &data)
         != RETURN_OK)
      goto fail;

   /* stat for modified time */
   if (!pnd->modified_time && stat(path, &st) == 0)
      pnd->modified_time = st.st_mtime;

   return RETURN_OK;

fail:
   return RETURN_FAIL;
}

/* \brief test function */
PNDMANAPI int pnd_do_something(const char *pnd_file)
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

   memset(data.pnd->version.major,  0, PNDMAN_VERSION);
   memset(data.pnd->version.minor,  0, PNDMAN_VERSION);
   memset(data.pnd->version.release,0, PNDMAN_VERSION);
   memset(data.pnd->version.build,  0, PNDMAN_VERSION);

   /* parse */
   if (_pxml_pnd_parse(&data, PXML, size) != RETURN_OK) {
      DEBFAIL("Your code sucks, fix it!");
      _pndman_free_pnd(test);
      exit(EXIT_FAILURE);
   }
   _pxml_pnd_post_process(test);

   /* debug filled PND */
   type = PND_TYPE_RELEASE;
   if (test->version.type == PND_VERSION_BETA)        type = PND_TYPE_BETA;
   else if (test->version.type == PND_VERSION_ALPHA)  type = PND_TYPE_ALPHA;

   printf(" ");
   printf("ID:       %s\n", test->id);
   if (!strlen(test->id)) {
      DEBFAIL("Your code sucks, fix it!");
      while ((test = _pndman_free_pnd(test)));
      exit(EXIT_FAILURE);
   }

   /* titles */
   printf("TITLE(S):");
   t = test->title;
   for (; t; t = t->next)
      printf("  %s:    %s\n", t->lang, t->string);

   /* descriptions */
   printf("DESCRIPTION(S):");
   t = test->description;
   for (; t; t = t->next)
      printf("  %s:    %s\n", t->lang, t->string);

   printf("ICON:     %s\n", test->icon);
   printf("AUTHOR:   %s\n", test->author.name);
   printf("WEBSITE:  %s\n", test->author.website);
   printf("VERSION:  %s.%s.%s.%s %s\n", test->version.major, test->version.minor,
         test->version.release, test->version.build, type);

   app = test->app;
   for (; app; app = app->next) {
      if (app->exec.x11 == PND_EXEC_REQ)          x11 = PND_X11_REQ;
      else if (app->exec.x11 == PND_EXEC_STOP)    x11 = PND_X11_STOP;
      else if (app->exec.x11 == PND_EXEC_IGNORE)  x11 = PND_X11_IGNORE;

      if (app->version.type == PND_VERSION_RELEASE)     type = PND_TYPE_RELEASE;
      else if (app->version.type == PND_VERSION_BETA)   type = PND_TYPE_BETA;
      else if (app->version.type == PND_VERSION_ALPHA)  type = PND_TYPE_ALPHA;

      printf(" ");
      printf("ID:       %s\n", app->id);

      /* titles */
      printf("TITLE(S):");
      t = app->title;
      for (; t; t = t->next)
         printf("  %s:    %s\n", t->lang, t->string);

      /* descriptions */
      printf("DESCRIPTION(S):");
      t = app->description;
      for (; t; t = t->next)
         printf("  %s:    %s\n", t->lang, t->string);

      printf("ICON:     %s\n", app->icon);
      printf("AUTHOR:   %s\n", app->author.name);
      printf("WEBSITE:  %s\n", app->author.website);
      printf("VERSION:  %s.%s.%s.%s %s\n", app->version.major, app->version.minor,
            app->version.release, app->version.build, type);
      printf("OSVER:    %s.%s.%s.%s\n", app->osversion.major, app->osversion.minor,
            app->osversion.release, app->osversion.build);

      printf("BKGRND:   %s\n", app->exec.background ? PNDMAN_TRUE : PNDMAN_FALSE);
      printf("STARTDIR: %s\n", app->exec.startdir);
      printf("STNDLONE: %s\n", app->exec.standalone ? PNDMAN_TRUE : PNDMAN_FALSE);
      printf("COMMAND:  %s\n", app->exec.command);
      printf("ARGS:     %s\n", app->exec.arguments);
      printf("X11:      %s\n", x11);
      printf("FREQ:     %d\n", app->frequency);

      /* previewpics */
      printf("LICENSE(S):");
      l = app->license;
      for (; l; l = l->next)
         printf("  %s, %s, %s\n", l->name, l->url, l->sourcecodeurl);

      /* previewpics */
      printf("ASSOCIATION(S):");
      a = app->association;
      for (; a; a = a->next)
         printf("  %s, %s, %s\n", a->name, a->filetype, a->exec);

      /* categories */
      printf("CATEGORIE(S):");
      c = app->category;
      for (; c; c = c->next)
         printf("  %s, %s\n", c->main, c->sub);

      /* previewpics */
      printf("PREVIEWPIC(S):");
      p = app->previewpic;
      for (; p; p = p->next)
         printf("  %s\n", p->src);
   }
   printf(" ");
   while ((test = _pndman_free_pnd(test)));

   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
