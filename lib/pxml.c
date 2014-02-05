#include "internal.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <expat.h>
#include <assert.h>
#include <ctype.h>

#ifndef _WIN32
#  include <sys/stat.h>
#  include <dirent.h>
#endif

#define PND_WINDOW         4096
#define PND_WINDOW_FRACT   PND_WINDOW-10
#define CHCKBUF(z)      \
   if (pos+z > bufsize) { \
      if (!(buffer = _realloc(buffer, bufsize, bufsize+PND_WINDOW))) { \
         goto buffer_fail; \
      } else { bufsize += PND_WINDOW; } \
   }

#define PNG_HEADER         "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"
#define PNG_END            "\x49\x45\x4E\x44"
#define PNGSTART()         pos = 0;
#define PNGCOPY(x,y,z) \
   CHCKBUF(z); memcpy(buffer+x,y,z); pos+=z;

#define PXML_START_TAG     "<PXML"
#define PXML_END_TAG       "</PXML>"
#define XML_HEADER         "<?xml version=\"1.1\" encoding=\"UTF-8\"?>"
#define XMLSTART()         pos = 0;
#define XMLCOPY(x,y,z) \
   CHCKBUF(z); memcpy(buffer+x,y,z); pos+=z;

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
   char                 **data;
   int                  bckward_title;
   int                  bckward_desc;
} pxml_parse;

/* \brief useful realloc wrapper */
static void* _realloc(void *ptr, size_t osize, size_t nsize)
{
   void *ptr2;
   if (!(ptr2 = realloc(ptr, nsize))) {
      if (!(ptr2 = malloc(nsize))) {
         free(ptr);
         return NULL;
      }
      memcpy(ptr2, ptr, osize);
      free(ptr);
   }
   return ptr2;
}

/* \brief search tag from string. NOTE: we do correct incorrect XML too so string changes */
static char* _match_tag(char *s, size_t len, char *tag, size_t *p)
{
   unsigned int nlen;
   char *end;

   nlen = strlen(tag);
   end  = s + len - nlen;
   *p   = 0;

   while (s < end) {
      /* don't even try unprintable characters */
      if (isprint(*s)) {
         /* tag found */
         if (!_strnupcmp(s, tag, nlen)) return &*s;
         else if (*s == '&') *s = ' ';
      }
      ++s; ++*p;
   }
   return NULL;
}

/* \brief get PXML out of pnd */
static char* _fetch_pxml_from_pnd(const char *pnd_file, size_t *size)
{
   FILE     *pnd;
   char     s[PND_WINDOW];
   char     *match, *buffer = NULL;
   size_t   pos, len, ret, read, stag, etag, bufsize;

   /* open PND */
   if (!(pnd = fopen(pnd_file, "rb")))
      goto read_fail;

   /* seek to end, and see if we should seek back */
   memset(s,  0, PND_WINDOW);
   fseek(pnd, 0, SEEK_END);
   if ((len = ftell(pnd)) > PND_WINDOW) {
      pos  = len - PND_WINDOW;
      read = PND_WINDOW;
   } else {
      pos  = 0;
      read = len;
   }

   /* allocate our buffer */
   if (!(buffer = malloc(read)))
      goto buffer_fail;
   memset(buffer, 0, (bufsize = read));

   /* hackety, hackety, hack */
   while (1) {
      /* seek */
      fseek(pnd, pos, SEEK_SET);

      /* read until start tag */
      if ((ret = fread(s, 1, read, pnd)))
         if ((match = _match_tag(s, read, PXML_START_TAG, &stag)))
         { ret = 1; break; }

      if (!ret) break; ret = 0;              /* below breaks are failures */
      if (!pos) break;                       /* nothing more to read */
      else if (pos > PND_WINDOW_FRACT) {     /* seek back */
         pos -= PND_WINDOW_FRACT;
         read = PND_WINDOW;
         if (len - pos > (500*1024)) break;
      } else {                               /* read final segment */
         read = PND_WINDOW - pos;
         memset(s + pos, 0, PND_WINDOW - pos);
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
   if (!_match_tag(match+strlen(PXML_START_TAG), read-stag-strlen(PXML_START_TAG), PXML_END_TAG, &etag)) {
      /* nope, copy first buffer and read to end */
      XMLCOPY(pos, match, read-stag);
      while ((ret = fread(s+strlen(PXML_END_TAG), 1, read-strlen(PXML_END_TAG), pnd))) {
         match = s+strlen(PXML_END_TAG);
         memcpy(s, buffer+pos-strlen(PXML_END_TAG), strlen(PXML_END_TAG));
         if (_match_tag(s, read, PXML_END_TAG, &etag)) { ret = 1; etag -= strlen(PXML_END_TAG); break; }
         XMLCOPY(pos, match, read-strlen(PXML_END_TAG));
      }
   } else etag += strlen(PXML_START_TAG);

   /* read fail */
   if (!ret)
      goto fail_end;

   /* finish */
   XMLCOPY(pos, match, etag+strlen(PXML_END_TAG));
   XMLCOPY(pos, "\0", 1);

   /* store size */
   *size = pos-1;

   /* close the file */
   fclose(pnd);
   return buffer;

read_fail:
   DEBFAIL(READ_FAIL, pnd_file);
   goto fail;
buffer_fail:
   DEBFAIL(OUT_OF_MEMORY);
   goto fail;
fail_start:
   DEBFAIL("%s: %s", pnd_file, PXML_START_TAG_FAIL);
   goto fail;
fail_end:
   DEBFAIL("%s: %s", pnd_file, PXML_END_TAG_FAIL);
   goto fail;
fail:
   IFDO(fclose, pnd);
   IFDO(free, buffer);
   return NULL;
}

/* \brief match png start or ending */
static char* _match_png(char *s, size_t len, char *tag, size_t *p)
{
   unsigned int nlen;
   char *end;

   nlen = strlen(tag);
   end  = s + len;
   *p   = 0;

   while (s < end) {
      /* tag found */
      if (!strncmp(s, tag, nlen)) return &*s;
      ++s; ++*p;
   }
   return NULL;
}

/* \brief fetch png icon from pnd */
static char* _fetch_png_from_pnd(const char *pnd_file, size_t *size)
{
   FILE     *pnd;
   char     s[PND_WINDOW];
   char     *match, *buffer = NULL;
   size_t   pos, len, ret, read, stag, etag, bufsize;

   /* open PND */
   if (!(pnd = fopen(pnd_file, "rb")))
      goto read_fail;

   /* seek to end, and see if we should seek back */
   memset(s,  0, PND_WINDOW);
   fseek(pnd, 0, SEEK_END);
   if ((len = ftell(pnd)) > PND_WINDOW) {
      pos  = len - PND_WINDOW;
      read = PND_WINDOW;
   } else {
      pos  = 0;
      read = len;
   }

   /* allocate our buffer */
   if (!(buffer = malloc(read)))
      goto buffer_fail;
   memset(buffer, 0, (bufsize = read));

   /* hackety, hackety, hack */
   while (1) {
      /* seek */
      fseek(pnd, pos, SEEK_SET);

      /* read until start tag */
      if ((ret = fread(s, 1, read, pnd)))
         if ((match = _match_png(s, read, PNG_HEADER, &stag))) { ret = 1; break; }

      if (!ret) break; ret = 0;              /* below breaks are failures */
      if (!pos) break;                       /* nothing more to read */
      else if (pos > PND_WINDOW_FRACT) {     /* seek back */
         pos -= PND_WINDOW_FRACT;
         read = PND_WINDOW;
         if (len - pos > (500*1024)) break;
      } else {                               /* read final segment */
         read = PND_WINDOW - pos;
         memset(s + pos, 0, PND_WINDOW - pos);
         pos = 0;
      }
   }

   /* found png? */
   if (!ret)
      goto png_not_found;

   PNGSTART();
   /* check if end tag is on the same buffer as start tag */
   if (!_match_png(match+strlen(PNG_HEADER), read-stag-strlen(PNG_HEADER), PNG_END, &etag)) {
      /* nope, copy first buffer and read to end */
      PNGCOPY(pos, match, read-stag);
      while ((ret = fread(s, 1, read, pnd))) {
         match = s;
         if (_match_png(s, read, PNG_END, &etag)) { ret = 1; break; }
         PNGCOPY(pos, s, read);
      }
   } else etag += strlen(PNG_HEADER);

   /* read fail */
   if (!ret)
      goto png_not_found;

   /* finish */
   PNGCOPY(pos, match, etag+strlen(PNG_END)+4);

   /* store size */
   *size = pos;

   fclose(pnd);
   return buffer;

read_fail:
   DEBFAIL(READ_FAIL, pnd_file);
   goto fail;
buffer_fail:
   DEBFAIL(OUT_OF_MEMORY);
   goto fail;
png_not_found:
   DEBFAIL(PXML_PNG_NOT_FOUND, pnd_file);
   goto fail;
fail:
   IFDO(fclose, pnd);
   IFDO(free, buffer);
   return NULL;
}

/* \brief fills pndman_package's struct */
static void _pxml_pnd_package_tag(pndman_package *pnd, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <package id= */
      if (!memcmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR))) {
         IFDO(free, pnd->id);
         pnd->id = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_package's struct */
static void _pxml_pnd_icon_tag(pndman_package *pnd, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <icon src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR))) {
         IFDO(free, pnd->icon);
         pnd->icon = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <application id= */
      if (!memcmp(attrs[i], PXML_ID_ATTR, strlen(PXML_ID_ATTR))) {
         IFDO(free, app->id);
         app->id = strdup(attrs[++i]);
      }
      /* <application appdata= */
      else if (!memcmp(attrs[i], PXML_APPDATA_ATTR, strlen(PXML_APPDATA_ATTR))) {
         IFDO(free, app->appdata);
         app->appdata = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_icon_tag(pndman_application *app, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <icon src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR))) {
         IFDO(free, app->icon);
         app->icon = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_application's struct */
static void _pxml_pnd_application_clockspeed_tag(pndman_application *app, char **attrs)
{
   (void)app;
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
      else if (!memcmp(attrs[i], PXML_STARTDIR_ATTR, strlen(PXML_STARTDIR_ATTR))) {
         IFDO(free, exec->startdir);
         exec->startdir = strdup(attrs[++i]);
      }
      /* <exec standalone= */
      else if (!memcmp(attrs[i], PXML_STANDALONE_ATTR, strlen(PXML_STANDALONE_ATTR)))
      {
         if (!memcmp(attrs[++i], PNDMAN_FALSE, strlen(PNDMAN_FALSE))) exec->standalone = 0;
      }
      /* <exec command= */
      else if (!memcmp(attrs[i], PXML_COMMAND_ATTR, strlen(PXML_COMMAND_ATTR))) {
         IFDO(free, exec->command);
         exec->command = strdup(attrs[++i]);
      }
      /* <exec arguments= */
      else if (!memcmp(attrs[i], PXML_ARGUMENTS_ATTR, strlen(PXML_ARGUMENTS_ATTR))) {
         IFDO(free, exec->arguments);
         exec->arguments = strdup(attrs[++i]);
      }
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
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR))) {
         IFDO(free, info->name);
         info->name = strdup(attrs[++i]);
      }
      /* <info type= */
      else if (!memcmp(attrs[i], PXML_TYPE_ATTR, strlen(PXML_TYPE_ATTR))) {
         IFDO(free, info->type);
         info->type = strdup(attrs[++i]);
      }
      /* <info src= */
      else if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR))) {
         IFDO(free, info->src);
         info->src = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_license's struct */
static void _pxml_pnd_license_tag(pndman_license *lic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <license name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR))) {
         IFDO(free, lic->name);
         lic->name = strdup(attrs[++i]);
      }
      /* <license url= */
      else if (!memcmp(attrs[i], PXML_URL_ATTR, strlen(PXML_URL_ATTR))) {
         IFDO(free, lic->url);
         lic->url = strdup(attrs[++i]);
      }
      /* <license sourcecodeurl= */
      else if (!memcmp(attrs[i], PXML_SOURCECODE_ATTR, strlen(PXML_SOURCECODE_ATTR))) {
         IFDO(free, lic->sourcecodeurl);
         lic->sourcecodeurl = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_previewpic's struct */
static void _pxml_pnd_pic_tag(pndman_previewpic *pic, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <pic src= */
      if (!memcmp(attrs[i], PXML_SRC_ATTR, strlen(PXML_SRC_ATTR))) {
         IFDO(free, pic->src);
         pic->src = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_category_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <category name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR))) {
         IFDO(free, cat->main);
         cat->main = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_category's struct */
static void _pxml_pnd_subcategory_tag(pndman_category *cat, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <subcategory name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR))) {
         IFDO(free, cat->sub);
         cat->sub = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_associations's struct */
static void _pxml_pnd_association_tag(pndman_association *assoc, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <association name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR))) {
         IFDO(free, assoc->name);
         assoc->name = strdup(attrs[++i]);
      }
      /* <association filetype= */
      else if (!memcmp(attrs[i], PXML_FILETYPE_ATTR, strlen(PXML_FILETYPE_ATTR))) {
         IFDO(free, assoc->filetype);
         assoc->filetype = strdup(attrs[++i]);
      }
      /* <association exec= */
      else if (!memcmp(attrs[i], PXML_EXEC_ATTR, strlen(PXML_EXEC_ATTR))) {
         IFDO(free, assoc->exec);
         assoc->exec = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_translated's struct */
static void _pxml_pnd_translated_tag(pndman_translated *title, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <title/description lang= */
      if (!memcmp(attrs[i], PXML_LANG_ATTR, strlen(PXML_LANG_ATTR))) {
         IFDO(free, title->lang);
         title->lang = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_author's structs */
static void _pxml_pnd_author_tag(pndman_author *author, char **attrs)
{
   int i = 0;
   for(; attrs[i]; ++i) {
      /* <author name= */
      if (!memcmp(attrs[i], PXML_NAME_ATTR, strlen(PXML_NAME_ATTR))) {
         IFDO(free, author->name);
         author->name = strdup(attrs[++i]);
      }
      /* <author website= */
      else if(!memcmp(attrs[i], PXML_WEBSITE_ATTR, strlen(PXML_WEBSITE_ATTR))) {
         IFDO(free, author->website);
         author->website = strdup(attrs[++i]);
      }
      /* <author email= */
      else if(!memcmp(attrs[i], PXML_EMAIL_ATTR, strlen(PXML_EMAIL_ATTR))) {
         IFDO(free, author->email);
         author->email = strdup(attrs[++i]);
      }
   }
}

/* \brief fills pndman_version's structs */
static void _pxml_pnd_version_tag(pndman_version *ver, char **attrs)
{
   int i = 0;
   for (; attrs[i]; ++i) {
      /* <osversion/version major= */
      if (!memcmp(attrs[i], PXML_MAJOR_ATTR, strlen(PXML_MAJOR_ATTR))) {
         IFDO(free, ver->major);
         ver->major = strdup(attrs[++i]);
      }
      /* <osversion/version minor= */
      else if (!memcmp(attrs[i], PXML_MINOR_ATTR, strlen(PXML_MINOR_ATTR))) {
         IFDO(free, ver->minor);
         ver->minor = strdup(attrs[++i]);
      }
      /* <osverion/version release= */
      else if (!memcmp(attrs[i], PXML_RELEASE_ATTR, strlen(PXML_RELEASE_ATTR))) {
         IFDO(free, ver->release);
         ver->release = strdup(attrs[++i]);
      }
      /* <osversion/version build= */
      else if (!memcmp(attrs[i], PXML_BUILD_ATTR, strlen(PXML_BUILD_ATTR))) {
         IFDO(free, ver->build);
         ver->build = strdup(attrs[++i]);
      }
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
               ((pxml_parse*)data)->data = &title->string;
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
               ((pxml_parse*)data)->data = &desc->string;
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
               ((pxml_parse*)data)->data = &title->string;
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
               ((pxml_parse*)data)->data = &desc->string;
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
                  ((pxml_parse*)data)->data = &title->string;
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
                  ((pxml_parse*)data)->data = &desc->string;
               }
            }
         }
      break;
   }
}

/* \brief copy string with special care, returns number of characters copied */
static char* _cstrdup(char *src, int len)
{
   int i, p, nospace;
   char *dst;
   assert(src);

   if (!len) return NULL;
   if (!(dst = malloc(len+1)))
      return NULL;

   p = 0; nospace = 0;
   for (i = 0; i < len; ++i) {
      if (!isspace(src[i])) nospace = 1;
      if (isprint(src[i]) && src[i] != '\n' &&
          src[i] != '\r'  && src[i] != '\t' && nospace) {
         dst[p++] = src[i];
      }
   }
   dst[p++] = 0;

   char *fdst;
   if ((fdst = strdup(dst))) {
      free(dst);
      dst = fdst;
   }
   return dst;
}

/* \brief Text data */
static void _pxml_pnd_data(void *data, char *text, int len)
{
   // unsigned int       *parse_state  = &((pxml_parse*)data)->state;
   // pndman_package     *pnd          = ((pxml_parse*)data)->pnd;
   // pndman_application *app          = ((pxml_parse*)data)->app;
   char                 **ptr          = ((pxml_parse*)data)->data;

   if (!ptr) return;
   IFDO(free, *ptr);
   *ptr = _cstrdup(text, len);
   ((pxml_parse*)data)->data = NULL;
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
   size_t i;

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

   if (ret == RETURN_FAIL) {
      DEBUG(PNDMAN_LEVEL_WARN, PXML_INVALID_XML, XML_ErrorString(XML_GetErrorCode(xml)));
      if (pndman_get_verbose() >= PNDMAN_LEVEL_WARN) {
         printf("-----\n");
         for (i = 0; i != size; ++i) printf("%c", PXML[i]);
         printf("\n-----\n");
      }
   }

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
   pndman_application *a;

   /* no need to do anything */
   if (!pnd->app)
      return;

   /* header */
   if (!pnd->id && pnd->app->id)
      pnd->id = strdup(pnd->app->id);
   if (!pnd->icon && pnd->app->icon)
      pnd->icon = strdup(pnd->app->icon);

   /* author */
   if (!pnd->author.name && pnd->app->author.name)
      pnd->author.name = strdup(pnd->app->author.name);
   if (!pnd->author.website && pnd->app->author.website)
      pnd->author.website = strdup(pnd->app->author.website);

   /* version */
   if (!pnd->version.major && pnd->app->version.major)
      pnd->version.major = strdup(pnd->app->version.major);
   if (!pnd->version.minor && pnd->app->version.minor)
      pnd->version.minor = strdup(pnd->app->version.minor);
   if (!pnd->version.release && pnd->app->version.release)
      pnd->version.release = strdup(pnd->app->version.release);
   if (!pnd->version.build && pnd->app->version.build)
      pnd->version.build = strdup(pnd->app->version.build);
   if (pnd->version.type != pnd->app->version.type && pnd->version.type == PND_VERSION_RELEASE)
      pnd->version.type = pnd->app->version.type;

   /* titles */
   if (!pnd->title && pnd->app->title) {
      t = pnd->app->title;
      for (; t; t = t->next) {
         if ((tc = _pndman_package_new_title(pnd))) {
            if (t->lang) tc->lang = strdup(t->lang);
            if (t->string) tc->string = strdup(t->string);
         }
      }
   }

   /* descriptions */
   if (!pnd->description && pnd->app->description) {
      t = pnd->app->description;
      for (; t; t = t->next) {
         if ((tc = _pndman_package_new_description(pnd))) {
            if (t->lang) tc->lang = strdup(t->lang);
            if (t->string) tc->string = strdup(t->string);
         }
      }
   }

   /* licenses */
   if (!pnd->license && pnd->app->license) {
      l = pnd->app->license;
      for (; l; l = l->next) {
         if ((lc = _pndman_package_new_license(pnd))) {
            if (l->name) lc->name = strdup(l->name);
            if (l->url) lc->url = strdup(l->url);
            if (l->sourcecodeurl) lc->sourcecodeurl = strdup(l->sourcecodeurl);
         }
      }
   }

   /* previewpics */
   if (!pnd->previewpic && pnd->app->previewpic) {
      p = pnd->app->previewpic;
      for (; p; p = p->next) {
         if ((pc = _pndman_package_new_previewpic(pnd))) {
            if (p->src) pc->src = strdup(p->src);
         }
      }
   }

   /* categories */
   if (!pnd->category && pnd->app->category) {
      c = pnd->app->category;
      for (; c; c = c->next) {
         if ((cc = _pndman_package_new_category(pnd))) {
            if (c->main) cc->main = strdup(c->main);
            if (c->sub) cc->sub = strdup(c->sub);
         }
      }
   }

   /* check for null appdata */
   for (a = pnd->app; a; a = a->next) {
      if (!a->appdata) a->appdata = strdup(a->id);
   }
}

/* \brief Process crawl */
static int _pndman_crawl_process(const char *path, const char *relative, pxml_parse *data)
{
   char *PXML = NULL, *full_path = NULL;
   size_t size = 0;
   FILE *f;
   assert(path && relative && data);

   int len = snprintf(NULL, 0, "%s/%s", path, relative)+1;
   if (!(full_path = malloc(len))) goto fail;
   sprintf(full_path, "%s/%s", path, relative);

   if (!(PXML = _fetch_pxml_from_pnd(full_path, &size)))
      goto fail;

   /* reset some stuff before crawling for post process */
   IFDO(free, data->pnd->version.major);
   IFDO(free, data->pnd->version.minor);
   IFDO(free, data->pnd->version.release);
   IFDO(free, data->pnd->version.build);

   /* parse */
   if (_pxml_pnd_parse(data, PXML, size) != RETURN_OK)
      goto parse_fail;

   /* we don't need this anymore */
   free(PXML);

   /* post process */
   _pxml_pnd_post_process(data->pnd);

   /* add size to the pnd */
   if ((f = fopen(full_path, "rb"))) {
      fseek(f, 0, SEEK_END);
      data->pnd->size = ftell(f);
      fclose(f);
   }

   NULLDO(free, full_path);

   /* add path to the pnd */
   char *copy = strdup(relative);
   IFDO(free, data->pnd->path);
   data->pnd->path = copy;
   return RETURN_OK;

parse_fail:
   DEBFAIL(PXML_PND_PARSE_FAIL, relative);
fail:
   IFDO(free, full_path);
   IFDO(free, PXML);
   return RETURN_FAIL;
}

/* \brief Crawl directory */
static int _pndman_crawl_dir(const char *path, const char *relative, pndman_package *list)
{
   char *tmp;
   pxml_parse data;
   pndman_package *pnd, *p;
   int ret;

#ifndef _WIN32
   DIR *dp;
   struct dirent *ep;
#else
   WIN32_FIND_DATA dp;
   HANDLE hFind;
#endif

   assert(path && list);

   /* init parse data */
   pnd = NULL; p = NULL;
   ret = 0;

   /* copy full */
   int size = snprintf(NULL, 0, "%s/%s", path, relative)+1;
   if (!(tmp = malloc(size))) return RETURN_FAIL;
   sprintf(tmp, "%s/%s", path, relative);

   /* jump to end of list */
   if (list->id) for (p = list; p && p->next; p = p->next);

#ifndef _WIN32 /* POSIX */
   dp = opendir(tmp);
   if (!dp) {
      free(tmp);
      return RETURN_FAIL;
   }

   while ((ep = readdir(dp))) {
      if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")) continue; /* no we don't want this! */
      /* recrusive */
      if (ep->d_type == DT_DIR) {
         char *relav;
         int size2 = snprintf(NULL, 0, "%s/%s", relative, ep->d_name)+1;
         if (!(relav = malloc(size2))) continue;
         sprintf(relav, "%s/%s", relative, ep->d_name);
         ret += _pndman_crawl_dir(path, relav, list);
         free(relav);
         continue;
      }
#if 1 /* skip hidden . (dot) files? */
      if (ep->d_name[0] == '.') continue;
#endif
      if (!_strupstr(ep->d_name, ".pnd")) continue;     /* we only want .pnd files */

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

      char *relav;
      int size2 = snprintf(NULL, 0, "%s/%s", relative, ep->d_name)+1;
      if (!(relav = malloc(size2))) continue;
      sprintf(relav, "%s/%s", relative, ep->d_name);
      if (_pndman_crawl_process(path, relav, &data) != RETURN_OK) {
         free(relav);
         while ((pnd = _pndman_free_pnd(pnd)));
         continue;
      }
      free(relav);

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
   char *tmp2;
   size += strlen("/*")+1;
   if (!(tmp2 = malloc(size))) {
      free(tmp);
      return RETURN_FAIL;
   }
   snprintf(tmp2, size-1, "%s/*", tmp);
   if ((hFind = FindFirstFile(tmp2, &dp)) == INVALID_HANDLE_VALUE)
      return 0;
   free(tmp2);

   do {
      if (!strcmp(dp.cFileName, ".") || !strcmp(dp.cFileName, "..")) continue; /* we don't want this */
      /* recrusive */
      if (dp.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
         char *relav;
         int size2 = snprintf(NULL, 0, "%s/%s", relative, dp.cFileName)+1;
         if (!(relav = malloc(size2))) continue;
         sprintf(relav, "%s/%s", relative, dp.cFileName);
         ret += _pndman_crawl_dir(path, relav, list);
         free(relav);
         continue;
      }
#if 1 /* skip hidden . (dot) files? */
      if (ep->d_name[0] == '.') continue;
#endif
      if (!_strupstr(dp.cFileName, ".pnd")) continue; /* we only want .pnd files */

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

      char *relav;
      int size2 = snprintf(NULL, 0, "%s/%s", relative, dp.cFileName)+1;
      if (!(relav = malloc(size2))) continue;
      sprintf(relav, "%s/%s", relative, dp.cFileName);
      if (_pndman_crawl_process(path, relav, &data) != RETURN_OK) {
         free(relav);
         while ((pnd = _pndman_free_pnd(pnd)));
         continue;
      }
      free(relav);

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

   free(tmp);
   return ret;
}

/* \brief Crawl to pndman_package list */
static int _pndman_crawl_to_pnd_list(pndman_device *device, pndman_package *list)
{
   int ret = 0;
   assert(device && list);

   /* can't access */
   if (!device->mount || access(device->mount, F_OK) != 0)
      goto fail;

   /* desktop */
   ret += _pndman_crawl_dir(device->mount, "pandora/desktop", list);

   /* menu */
   ret += _pndman_crawl_dir(device->mount, "pandora/menu", list);

   /* apps */
   ret += _pndman_crawl_dir(device->mount, "pandora/apps", list);

   return ret;

fail:
   DEBFAIL(ACCESS_FAIL, device->mount);
   return RETURN_FAIL;
}

/* \brief crawl to repository, return number of pnd's found, and -1 on error */
static int _pndman_crawl_to_repository(int full, pndman_device *device, pndman_repository *local)
{
   pndman_package *list, *p, *n = NULL, *pnd;
   int ret;
#ifdef _WIN32
   /* TODO: win32 implementation */
#else
   struct stat st;
#endif
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
   for (p = list, ret = 0; p; p = n) {
      pnd = _pndman_repository_new_pnd_check(p, p->path, device->mount, local);
      if (pnd) {
         /* copy needed stuff over */
         _pndman_copy_pnd(pnd, p);
         if (!full) _pndman_package_free_applications(pnd);
         IFDO(free, pnd->mount);
         pnd->mount = strdup(device->mount);
         pnd->repositoryptr = local;
         pnd->modified_time = 0;

         /* the md5 might not be correct anymore
          * we don't fill it again, since it takes lots of time */
         IFDO(free, pnd->md5);

         /* stat for modified time */
         if (!pnd->modified_time) {
            char *path;
            if ((path = _pndman_pnd_get_path(pnd))) {
#ifdef _WIN32
               /* TODO: win32 implementation */
#else
               if (stat(path, &st) == 0)
                  pnd->local_modified_time = st.st_mtime;
#endif
               free(path);
            }
         }

         /* count again */
         ++ret;
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
PNDMANAPI int pndman_package_crawl(int full_crawl, pndman_device *device, pndman_repository *local)
{
   CHECKUSE(device);
   CHECKUSE(local);
   local = _pndman_repository_first(local);
   return _pndman_crawl_to_repository(full_crawl, device, local);
}

/* \brief fill single PND's data fully by crawling it locally */
PNDMANAPI int pndman_package_crawl_single_package(int full_crawl, pndman_package *pnd)
{
   pxml_parse data;
#ifdef _WIN32
   /* TODO: win32 implementation */
#else
   struct stat st;
#endif
   CHECKUSE(pnd);

   data.pnd   = pnd;
   data.app   = NULL;
   data.data  = NULL;
   data.bckward_title = 1; /* backwards compatibility with PXML titles */
   data.bckward_desc  = 1; /* backwards compatibility with PXML descriptions */
   data.state = PXML_PARSE_DEFAULT;

   if (_pndman_crawl_process(pnd->mount, pnd->path, &data) != RETURN_OK)
      goto fail;

   if (!full_crawl) _pndman_package_free_applications(pnd);

   /* stat for modified time */
   if (!pnd->modified_time) {
      char *path;
      if ((path = _pndman_pnd_get_path(pnd))) {
#ifdef _WIN32
         /* TODO: win32 implementation */
#else
         if (stat(path, &st) == 0)
            pnd->local_modified_time = st.st_mtime;
#endif
         free(path);
      }
   }

   /* fill md5 of single pnd crawl */
   pndman_package_fill_md5(pnd);
   return RETURN_OK;

fail:
   return RETURN_FAIL;
}

/* \brief get embedded png from pnd
 * returns number of bytes copied on success and 0 on failure */
PNDMANAPI size_t pndman_package_get_embedded_png(pndman_package *pnd, char *buffer, size_t buflen)
{
   char *PNG = NULL;
   size_t size;

   CHECKUSE(pnd);
   CHECKUSE(buffer);

   /* fill our buffer */
   char *path = _pndman_pnd_get_path(pnd);
   if (!path) goto fail;
   if (!(PNG = _fetch_png_from_pnd(path, &size)))
      goto fail;

   /* our buffer is too big. */
   if (size > buflen)
      goto too_big;

   memset(buffer, 0, buflen);
   memcpy(buffer, PNG, size);

   /* free buffer */
   free(PNG);
   return size;

too_big:
   DEBFAIL(PXML_PNG_BUFFER_TOO_BIG);
fail:
   IFDO(free, PNG);
   return 0;
}

/* \brief internal test function */
PNDMANAPI int pndman_pxml_test(const char *file)
{
   char *PXML, *PNG;
   char *type, *x11; size_t size = 0;
   FILE *f;
   pndman_package       *test;
   pndman_application   *app;
   pndman_translated    *t;
   pndman_license       *l;
   pndman_category      *c;
   pndman_previewpic    *p;
   pndman_association   *a;

   /* write PNG to file */
   if ((PNG = _fetch_png_from_pnd(file, &size))) {
      if ((f = fopen("test.png", "wb"))) {
         fwrite(PNG, size, 1, f);
         fflush(f);
         fclose(f);
      }
      free(PNG);
   }

   if (!(PXML = _fetch_pxml_from_pnd(file, &size)))
      return RETURN_FAIL;

   test = _pndman_new_pnd();
   if (!test) return RETURN_FAIL;

   pxml_parse data;
   data.pnd   = test;
   data.app   = NULL;
   data.data  = NULL;
   data.bckward_title = 1;
   data.bckward_desc  = 1;
   data.state = PXML_PARSE_DEFAULT;

   IFDO(free, data.pnd->version.major);
   IFDO(free, data.pnd->version.minor);
   IFDO(free, data.pnd->version.release);
   IFDO(free, data.pnd->version.build);

   /* parse */
   if (_pxml_pnd_parse(&data, PXML, size) != RETURN_OK) {
      DEBFAIL("Your code sucks, fix it!");
      _pndman_free_pnd(test);
      free(PXML);
      exit(EXIT_FAILURE);
   }

   free(PXML);
   _pxml_pnd_post_process(test);

   /* debug filled PND */
   type = PND_TYPE_RELEASE;
   if (test->version.type == PND_VERSION_BETA)        type = PND_TYPE_BETA;
   else if (test->version.type == PND_VERSION_ALPHA)  type = PND_TYPE_ALPHA;

   /* get embedded png */
   {
      size_t size = 1150663564;
      void *data = malloc(size);
      if (data) {
         pndman_package_get_embedded_png(test, data, size);
         free(data);
      }
   }

   puts("");
   printf("ID:       %s\n", test->id);
   if (!test->id || !strlen(test->id)) {
      DEBFAIL("Your code sucks, fix it!");
      while ((test = _pndman_free_pnd(test)));
      exit(EXIT_FAILURE);
   }

   /* titles */
   printf("TITLE(S):\n");
   t = test->title;
   for (; t; t = t->next)
      printf("  %s:    %s\n", t->lang, t->string);

   /* descriptions */
   printf("DESCRIPTION(S):\n");
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
      printf("TITLE(S):\n");
      t = app->title;
      for (; t; t = t->next)
         printf("  %s:    %s\n", t->lang, t->string);

      /* descriptions */
      printf("DESCRIPTION(S):\n");
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
      printf("LICENSE(S):\n");
      l = app->license;
      for (; l; l = l->next)
         printf("  %s, %s, %s\n", l->name, l->url, l->sourcecodeurl);

      /* previewpics */
      printf("ASSOCIATION(S):\n");
      a = app->association;
      for (; a; a = a->next)
         printf("  %s, %s, %s\n", a->name, a->filetype, a->exec);

      /* categories */
      printf("CATEGORIE(S):\n");
      c = app->category;
      for (; c; c = c->next)
         printf("  %s, %s\n", c->main, c->sub);

      /* previewpics */
      printf("PREVIEWPIC(S):\n");
      p = app->previewpic;
      for (; p; p = p->next)
         printf("  %s\n", p->src);
   }
   puts("");
   while ((test = _pndman_free_pnd(test)));

   return RETURN_OK;
}

/* vim: set ts=8 sw=3 tw=0 :*/
