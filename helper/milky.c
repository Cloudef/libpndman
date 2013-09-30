#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include "pndman.h"

#ifdef WIN32
#  include <windows.h>
#else
#  include <limits.h>
#  include <pwd.h>
#  include <sys/stat.h>
#  include <dirent.h>
#  include <sys/select.h>
#endif

/*   ____ _____ _   _ _____ ____      _    _
 *  / ___| ____| \ | | ____|  _ \    / \  | |
 * | |  _|  _| |  \| |  _| | |_) |  / _ \ | |
 * | |_| | |___| |\  | |___|  _ <  / ___ \| |___
 *  \____|_____|_| \_|_____|_| \_\/_/   \_\_____|
*/

/* stores old root device here */
#define OLDROOT_FILE "milkyhelper.device"

/* stores configuration here */
#define CONFIG_FILE  "milkyhelper.config"

/* name of directory inside $XDG_CONFIG_HOME */
#define CFG_DIR      "milkyhelper"

/* max length of descriptions */
#define DESCRIPTION_LENGTH 74

/* log downloads? */
char _LOG_DL      = 0;

/* verbose level? */
char _VERBOSE     = 0;

/* quiet level? */
char _QUIET       = 0;

/* max concurrent downloads? */
unsigned int _QUEUE = 5;

/* use this to clear line. */
char _ERASE[120];

typedef enum _RETURN_STATUS
{
   RETURN_FAIL    = -1,
   RETURN_OK      =  0,
   RETURN_TRUE    =  1,
   RETURN_FALSE   = !RETURN_TRUE
} _RETURN_STATUS;

#define PND_ID 302
typedef struct _USR_TARGET
{
   char                 id[PND_ID];
   pndman_package       *pnd;
   struct _USR_TARGET   *next;
   struct _USR_TARGET   *prev;
} _USR_TARGET;

typedef struct _USR_IGNORE
{
   char                 id[PND_ID];
   struct _USR_IGNORE   *next;
   struct _USR_IGNORE   *prev;
} _USR_IGNORE;

typedef struct _USR_DATA
{
   char              *bin;
   unsigned long long flags;
   pndman_device     *dlist;
   pndman_device     *root;
   pndman_repository *rlist;
   _USR_TARGET       *tlist;
   _USR_IGNORE       *ilist;
   char              *no_action;
   char              syslc[6];
   double tdl, ttdl;
} _USR_DATA;

typedef struct _comment_data {
   char *username;
   char *comment;
   struct _comment_data *next;
} _comment_data;

typedef struct _comment_pull_struct {
   _comment_data *comment;
   pndman_package *pnd;
} _comment_pull_struct;

typedef struct _comment_rm_struct {
   const char *needle;
   _USR_DATA *data;
} _comment_rm_struct;

/* initialize _USR_DATA struct */
static void init_usrdata(_USR_DATA *data)
{
   char *LANG;
   assert(data);
   memset(data, 0, sizeof(_USR_DATA));
   data->bin = "milky";
   strncpy(data->syslc, "en_US", 5);
   if (!(LANG = getenv("LC_ALL"))  || !strlen(LANG))
      if (!(LANG = getenv("LANG")) || !strlen(LANG))
         return;
   strncpy(data->syslc, LANG, 5);
}

/*  ____ _____ ____  ___ _   _  ____ ____
 * / ___|_   _|  _ \|_ _| \ | |/ ___/ ___|
 * \___ \ | | | |_) || ||  \| | |  _\___ \
 *  ___) || | |  _ < | || |\  | |_| |___) |
 * |____/ |_| |_| \_\___|_| \_|\____|____/
 */

/* all strings expect HELP and VERSION strings are listed here
 * (* strings that are not considered in final product,
 * aren't here either)
 */
#define _D                       "\5-\2!\5- "
#define _DEVICE_CLEANED          _D"\2Cleaned libpndman database from: \5%s"
#define _PNDS_CRAWLED            _D"\4%d \2Packages were succesfully crawled."
#define _REPO_WONT_DO_ANYTHING   _D"\1There is only local repository available, \4%s \1operations won't do anything."
#define _NO_SYNCED_REPO          _D"\1None of repositories is synchorized. Synchorize the repositories first."
#define _REPO_SYNCED             _D"\2Synchorized repository: \4%s"
#define _REPO_SYNCED_NOT         _D"\1Failed to synchorize repository: \4%s"
#define _PND_NOT_FOUND           _D"\1Warning: No such package \4%s"
#define _PND_NO_UPDATE           _D"\1Warning: Package \4%s, has no update."
#define _PND_HAS_UPDATE          _D"\2Package \4%s \2has an update."
#define _PND_HAS_UPDATE_Q        _D"\4%d \2Packages have an update."
#define _PND_IS_CORRUPT          _D"\1Package \4%s \1is corrupt!"
#define _PND_MAY_CORRUPT         _D"\1Package \4%s \1may be corrupt!"
#define _PND_DIF_CORRUPT         _D"\1Package \4%s \1integrity differs beetwen repositories.\n"_PND_MAY_CORRUPT
#define _REMOVED_APPDATA         _D"\2Removed appdata: \5%s"
#define _APPDATA_FAIL            _D"\1Failed to remove appdata: \5%s"
#define _UPGRADE_DONE            _D"\2Upgraded package: \4%s"
#define _UPGRADE_FAIL            _D"\1Failed to upgrade \4%s"
#define _INSTALL_DONE            _D"\2Installed package: \4%s"
#define _INSTALL_FAIL            _D"\1Failed to install package: \4%s"
#define _REMOVE_DONE             _D"\2Removed package: \4%s"
#define _REMOVE_FAIL             _D"\1Failed to remove package: \4%s"
#define _CLEANING_DB             _D"\3Cleaning database files: \5%s"
#define _UNKNOWN_OPERATION       _D"\1unknown operation: \5%s"
#define _NO_X_SPECIFIED          _D"\1No \4%s \1specified."
#define _BAD_DEVICE_SELECTED     _D"\1Bad device selected, exiting.."
#define _PND_IGNORED_FORCE       _D"\3Package \4%s \3is being ignored. Do you want to force?"
#define _PND_IGNORED_FORCE_Q     _D"\4%d \3Packages are being ignored. Do you want to force?"
#define _PND_REINSTALL           _D"\3Package \4%s \3is already installed, reinstall?"
#define _PND_REINSTALL_Q         _D"\4%d \3Packages are already installed, reinstall?"
#define _NO_ROOT_YET             _D"\4Sir! You haven't selected your mount where to store packages yet."
#define _SELECT_REPOSITORY       _D"\4Select repository."
#define _WARNING_SKIPPING        _D"\1Warning: Skipping package \4%s"
#define _WARNING_UPDATE          _D"\1Warning: \4%s \1is up-to-date, skipping."
#define _WARNING_SKIPPING_Q      _D"\1Warning: Skipping \4%d \1packages."
#define _WARNING_UPDATE_Q        _D"\1Warning: \4%d \1packages are up-to-date, skipping."
#define _NOTHING_TO_DO           _D"\4There is nothing to do."
#define _FAILED_TO_DEVICES       _D"\1Failed to detect devices."
#define _FAILED_TO_REPOS         _D"\1Failed to initialize local repository.."
#define _COMMIT_FAIL             _D"\1Warning: Failed to commit repositories to \5%s"
#define _ROOT_FAIL               _D"\1Warning: Failed to store your current root device."
#define _ROOT_SET_FAIL           _D"\1Failed to set root."
#define _ROOT_SET                _D"\2Root was set."
#define _MKCONFIG                _D"\3Creating default configuration file in \5%s"
#define _CONFIG_READ_FAIL        _D"\1Warning: failed to read configuration from \5%s"
#define _INTERNAL_CURL_FAIL      _D"\1Internal curl failure!"
#define _REPO_API_KEY_ADDED      _D"\2Credentials were set succesfully!"
#define _REPO_API_KEY_ADD_USAGE  _D"\1usage: <username> <api key> <repository>"
#define _COULD_NOT_FIND_REPO     _D"\1No such repository: \5%s \1make sure it's in your repository list."
#define _NO_REMOTE_REPOS         _D"\1You have no remote repositories defined in configuration file."
#define _REPO_API_FORGOT_RATING  _D"\1You forgot to give your rating [1-5]"
#define _REPO_API_NO_PACKAGES    _D"\1No packages given to perform this action on."
#define _COMMENT_LENGTH_WARN     _D"\1Your comment is over 300 characters, it will be truncated."
#define _REPO_API_NO_COMMENT     _D"\1You din't specify a comment message."
#define _REPO_API_NO_COMMENTS    _D"\1No comments."
#define _REPO_API_NO_HISTORY     _D"\1No history."
#define _FOUND_COMMENT_MATCH     _D"\2Found match from package \4%s"
#define _DELETE_COMMENT          _D"\3Do you want to delete it?"
#define _COMMENTS_FOR_PACKAGE    _D"\2Comments for \4%s\5:"
#define _YAOURT_DIALOG           "\4Enter number of packages to be installed \5(\2ex: 1 2 3\5)"
#define _TARGET_MEDIA            "\4Target media  \5: %s"
#define _TARGET_PATH             "\4Target path   \5: %s"
#define _DOWNLOAD_SIZE           "\1Download size \5: %2.f %s"
#define _REMOVE_SIZE             "\1Removal size  \5: %2.f %s"
#define _CONTINUE_INSTALL        "\3Continue install?"
#define _CONTINUE_UPGRADE        "\3Continue upgrade?"
#define _CONTINUE_UNINSTALL      "\3Continue uninstall?"
#define _SEND_RATING             "\3Send rating for packages?"
#define _SEND_COMMENT            "\3Send comments for packages?"
#define _TARGET_LINE             "\2Targets \5(\4%d\5)   : "
#define _UNIT_KIB                "\5KiB"
#define _UNIT_MIB                "\5MiB"
#define _UNIT_BYTE               "\5B"
#define _INPUT_LINE              "> "
#define _YESNO                   "\5[\2Y\5/\1n\5]"
#define _NOYES                   "\5[\2y\5/\1N\5]"

/* other customizable */
#define NEWLINE()                puts("");
#define ERASE()                  printf(_ERASE); fflush(stdout);

/* printf alike wrapper for pndman_puts */
static void _printf(const char *fmt, ...)
{
   char buffer[LINE_MAX];
   va_list args;

   memset(buffer, 0, LINE_MAX);
   va_start(args, fmt);
   vsnprintf(buffer, LINE_MAX-1, fmt, args);
   va_end(args);

   pndman_puts(buffer);
}

/*  _   _ _____ ___ _
 * | | | |_   _|_ _| |
 * | | | | | |  | || |
 * | |_| | | |  | || |___
 *  \___/  |_| |___|_____|
 */

/* show hide terminal cursor */
void showcursor(int show)
{
#define CSI "\e["
  if (show) {
    fputs(CSI "?25h", stdout);
  }
  else {
    fputs(CSI "?25l", stdout);
  }
#undef CSI
}

/* trim string */
static size_t strtrim(char *str)
{
   size_t len;
   char *end, *pch = str;

   /* check if we need trimming */
   if (!str || *str == '\0') return 0;

   /* trim from left */
   while (isspace((unsigned char)*pch)) ++pch;
   if (pch != str) {
      if ((len = strlen(pch))) memmove(str, pch, len+1);
      else *str = '\0';
   }

   /* this string is empty, return */
   if (*str == '\0') return 0;

   /* trim from right */
   end = (str + (strlen(str) - 1));
   while (isspace((unsigned char)*end)) --end;

   *++end = '\0';
   return end - pch;
}

/* \brief strcmp strings in uppercase, NOTE: returns 0 on found else 1 (so you don't mess up with strcmp) */
static int strupcmp(const char *hay, const char *needle)
{
   size_t i, len;
   if ((len = strlen(hay)) != strlen(needle)) return RETURN_TRUE;
   for (i = 0; i != len; ++i)
      if (toupper(hay[i]) != toupper(needle[i])) return RETURN_TRUE;
   return RETURN_FALSE;
}

/* \brief strstr strings in uppercase */
static char* strupstr(const char *hay, const char *needle)
{
   size_t i, r, p, len, len2;
   if (!strupcmp(hay, needle)) return (char*)hay;
   if ((len = strlen(hay)) < (len2 = strlen(needle))) return NULL;
   for (p = 0, r = 0, i = 0; i != len; ++i) {
      if (p == len2) return (char*)&hay[r]; /* THIS IS IT! */
      if (toupper(hay[i]) == toupper(needle[p++])) {
         if (!r) r = i; /* could this be.. */
      } else { if (r) i = r; r = 0; p = 0; } /* ..nope, damn it! */
   }
   if (p == len2) return (char*)&hay[r]; /* THIS IS IT! */
   return NULL;
}

/* strip string from bad characters */
static char* strstrip(char *src)
{
   size_t size, i, p;
   char space = 0, *dst;

   i = 0; p = 0;
   if (!src)                     return NULL;
   if (!(size = strlen(src)))    return NULL;
   if (!(dst = malloc(size+1)))  return NULL;
   for(; i != size; ++i)
   {
      if (isspace(src[i])) { if(p) space = 1; }
      else if (src[i] == '\n' || src[i] == '\t' || src[i] == '\r') ;
      else { if (space) { dst[p++] = ' '; space = 0; } dst[p++] = src[i]; }
   }
   dst[p] = '\0';
   return dst;
}

/* win32 doesn't use below functions */
#if !defined(_WIN32)
/* strip leading slash from path */
static void stripslash(char *path)
{
   if (path[strlen(path)-1] == '/' || path[strlen(path)-1] == '\\')
      path[strlen(path)-1] = '\0';
}


/* mkdir if it doesn't exist */
static int mkdirexist(const char *path)
{
   if (access(path, F_OK) != 0) {
      if (errno == EACCES) return RETURN_FAIL;
#ifdef _WIN32
      if (mkdir(path) == -1) return RETURN_FAIL;
#else  /* _WIN32_ */
      if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
         return RETURN_FAIL;
#endif /* POSIX */
   }
   return RETURN_OK;
}
#endif

/* get $XDG_CONFIG_HOME/$CFG_DIR path, (create if doesn't exist) */
static int getcfgpath(char *path)
{
/* just dump to cwd on win32 */
#if !defined(_WIN32)
   const char* xdg_home;
   struct passwd *pw;
   if (!(xdg_home = getenv("XDG_CONFIG_HOME"))) {
      if (!(xdg_home = getenv("HOME"))) {
         if ((pw = getpwuid(getuid()))) xdg_home = pw->pw_dir;
         else return RETURN_FAIL;
      }
      strncpy(path, xdg_home, PATH_MAX-1);      /* $HOME */
      stripslash(path);                         /* make sure no leading slash */
      strncat(path, "/.config", PATH_MAX-1);    /* $HOME/.config */
      if (mkdirexist(path) != RETURN_OK)        /* mkdir if needed */
         return RETURN_FAIL;
   } else {
      strncpy(path, xdg_home, PATH_MAX-1);  /* $XDG_CONFIG_HOME */
      stripslash(path);                     /* make sure no leading slash */
   }
   strncat(path, "/",     PATH_MAX-1);
   strncat(path, CFG_DIR, PATH_MAX-1); /* $XDG_CONFIG_HOME/$CFG_DIR */
   return mkdirexist(path);
#endif /* !_WIN32 */
   return RETURN_OK;
}

/*  ____  _______     _____ ____ _____ ____
 * |  _ \| ____\ \   / /_ _/ ___| ____/ ___|
 * | | | |  _|  \ \ / / | | |   |  _| \___ \
 * | |_| | |___  \ V /  | | |___| |___ ___) |
 * |____/|_____|  \_/  |___\____|_____|____/
 */

/* root device where we perform all our operations */
static pndman_device* setroot(char *root, pndman_device *list)
{
   pndman_device *d;
   if (!root || !strlen(root)) return NULL;
   for (d = list; d; d = d->next)
      if (!strcmp(root, d->mount) || !strcmp(root, d->device)) return d;
   if ((d = pndman_device_add(root, list))) return d;
   return NULL;
}

/* get path of the old root "configuration" file */
static int oldrootcfgpath(char *path)
{
   if (getcfgpath(path) != RETURN_OK) return RETURN_FAIL;                  /* $XDG_CONFIG_HOME */
   strncat(path, strlen(path)?"/"OLDROOT_FILE:OLDROOT_FILE, PATH_MAX-1);   /* $XDG_CONFIG_HOME/$OLDROOT_FILE */
   return RETURN_OK;
}

/* get stored root */
static pndman_device* getroot(_USR_DATA *data)
{
   FILE *f;
   char path[PATH_MAX];
   char dev[LINE_MAX];

   memset(path, 0, PATH_MAX); memset(dev, 0, LINE_MAX);
   if (oldrootcfgpath(path) != RETURN_OK) return NULL;
   if (!(f = fopen(path, "r")))           return NULL;
   fgets(dev, LINE_MAX, f); fclose(f);
   if (dev[strlen(dev)-1] == '\n') dev[strlen(dev)-1] = '\0';
   data->root = setroot(dev, data->dlist);
   if (!data->dlist) data->dlist = data->root;
   return data->root;
}

/* save root */
static int saveroot(_USR_DATA *data)
{
   FILE *f;
   char path[PATH_MAX];

   if (!data->root) return RETURN_OK;
   memset(path, 0, PATH_MAX);
   if (oldrootcfgpath(path) != RETURN_OK) return RETURN_FAIL;
   if (!(f = fopen(path, "w")))           return RETURN_FAIL;
   fputs(data->root->mount, f); fclose(f);
   return RETURN_OK;
}

/*  _____  _    ____   ____ _____ _____ ____
 * |_   _|/ \  |  _ \ / ___| ____|_   _/ ___|
 *   | | / _ \ | |_) | |  _|  _|   | | \___ \
 *   | |/ ___ \|  _ <| |_| | |___  | |  ___) |
 *   |_/_/   \_\_| \_\\____|_____| |_| |____/
 */

/* add target package, which to perform operation on */
static _USR_TARGET* addtarget(const char *id, _USR_TARGET **list)
{
   _USR_TARGET *new, *t = NULL;
   assert(id);

   if (!id) return NULL;
   if (*list) {
      for (t = *list; t; t = t->next) if (!strcmp(t->id, id) ||
                                          !strcmp(t->pnd?t->pnd->id:t->id, id)) return NULL;
      for (t = *list; t && t->next; t = t->next);
      t->next = new = malloc(sizeof(_USR_TARGET));
   } else new = malloc(sizeof(_USR_TARGET));
   if (!new) return NULL;

   memset(new, 0, sizeof(_USR_TARGET));
   if (*list) new->prev = t;
   else     { new->prev = NULL; *list = new; }

   strncpy(new->id, id, PND_ID-1);
   return new;
}

/* remove target from list */
static _USR_TARGET* freetarget(_USR_TARGET *t)
{
   _USR_TARGET *f;
   assert(t);
   if (t->prev) t->prev->next = t->next;     /* point this prev to this next */
   if (t->next) t->next->prev = t->prev;     /* point this next to this prev */
   for (f = t; f && f->prev; f = f->prev);   /* get first */
   free(t);                                  /* free this */
   return f == t ? NULL : f;                 /* return first or null */
}

/* remove all targets */
static void freetarget_all(_USR_TARGET *t)
{
   _USR_TARGET *n;
   if (t) assert(!t->prev);
   for (; t; t = n) { n = t->next; free(t); } /* store next, and free this */
}

/*  ___ ____ _   _  ___  ____  _____ ____
 * |_ _/ ___| \ | |/ _ \|  _ \| ____/ ___|
 *  | | |  _|  \| | | | | |_) |  _| \___ \
 *  | | |_| | |\  | |_| |  _ <| |___ ___) |
 * |___\____|_| \_|\___/|_| \_\_____|____/
 */

/* add package which will be ignored from target list */
static _USR_IGNORE* addignore(char *id, _USR_IGNORE **list)
{
   _USR_IGNORE *new, *t = NULL;
   assert(id);

   if (!id) return NULL;
   if (*list) {
      for (t = *list; t; t = t->next) if (!strcmp(t->id, id)) return NULL;
      for (t = *list; t && t->next; t = t->next);
      t->next = new = malloc(sizeof(_USR_IGNORE));
   } else new = malloc(sizeof(_USR_IGNORE));
   if (!new) return NULL;

   memset(new, 0, sizeof(_USR_IGNORE));
   if (*list) new->prev = t;
   else     { new->prev = NULL; *list = new; }

   strncpy(new->id, id, PND_ID-1);
   return new;
}

#if 0 /* not used */
/* free ignored package from list */
static _USR_IGNORE* freeignore(_USR_IGNORE *t)
{
   _USR_IGNORE *f;
   assert(t);
   if (t->prev) t->prev->next = t->next;     /* point this prev to this next */
   if (t->next) t->next->prev = t->prev;     /* point this next to this prev */
   for (f = t; f && f->prev; f = f->prev);   /* get first */
   free(t);                                  /* free this */
   return f == t ? NULL : f;                 /* return first or null */
}
#endif

/* free all ignored packages */
static void freeignore_all(_USR_IGNORE *t)
{
   _USR_IGNORE *n;
   if (t) assert(!t->prev);
   for (; t; t = n) { n = t->next; free(t); } /* store next, and free this */
}

/* check if package id is being ignored */
static int checkignore(const char *id, _USR_DATA *data)
{
   _USR_IGNORE *i;
   for (i = data->ilist; i; i = i->next)
      if (!strcmp(i->id, id) ||
            !strcmp(i->id, id)) return RETURN_TRUE;
   return RETURN_FALSE;
}

/*  ____   _    ____  ____  _____
 * |  _ \ / \  |  _ \/ ___|| ____|
 * | |_) / _ \ | |_) \___ \|  _|
 * |  __/ ___ \|  _ < ___) | |___
 * |_| /_/   \_\_| \_\____/|_____|
 *
 * __        ______      _    ____  ____  _____ ____
 * \ \      / /  _ \    / \  |  _ \|  _ \| ____|  _ \
 *  \ \ /\ / /| |_) |  / _ \ | |_) | |_) |  _| | |_) |
 *   \ V  V / |  _ <  / ___ \|  __/|  __/| |___|  _ <
 *    \_/\_/  |_| \_\/_/   \_\_|   |_|   |_____|_| \_\
 */

/* addtarget wrapper for argument parsing */
static void _addtarget(char *id, _USR_DATA *data)
{
   assert(data);
   addtarget(id, &data->tlist);
}

/* setroot wrapper for argument parsing */
static void _setroot(char *root, _USR_DATA *data)
{
   assert(data);
   if (!root || !strlen(root)) { data->root = NULL; return; }
   data->root = setroot(root, data->dlist);
   if (!data->dlist) data->dlist = data->root;
   if (data->root) data->no_action = _ROOT_SET;
   else            data->no_action = _ROOT_SET_FAIL;
}

/*     _    ____   ____ _   _ __  __ _____ _   _ _____ ____
 *    / \  |  _ \ / ___| | | |  \/  | ____| \ | |_   _/ ___|
 *   / _ \ | |_) | |  _| | | | |\/| |  _| |  \| | | | \___ \
 *  / ___ \|  _ <| |_| | |_| | |  | | |___| |\  | | |  ___) |
 * /_/   \_\_| \_\\____|\___/|_|  |_|_____|_| \_| |_| |____/
 */

#define _PASSARG(x) { x(narg, data); *skipn = 1; return; }  /* pass next argument to function */
#define _PASSTHS(x) { x(arg, data); return; }               /* pass current argument to function */
#define _PASSFLG(x) { data->flags |= x; return; }           /* pass flag */
#define _SETFLG(x)  { data->flags  = x; return; }           /* set flag */
static const char* _G_ARG  = "fqrtv";        /* global arguments */
static const char* _OP_ARG = "hACPQRSUEV";   /* operations */
static const char* _S_ARG  = "abcdilmsuy";   /* sync operation arguments */
static const char* _R_ARG  = "n";            /* remove operation arguments */
static const char* _Q_ARG  = "cdilsu";       /* query operation arguments */
static const char* _P_ARG  = "cds";          /* crawl operation arguments */
static const char* _A_ARG  = "cdgp";         /* repo api operation arguments */

/* milkyhelper's operation flags */
typedef enum _HELPER_FLAGS
{
   GB_FORCE    = 0x000000001, OP_YAOURT    = 0x000000002,
   OP_SYNC     = 0x000000004, OP_UPGRADE   = 0x000000008,
   OP_REMOVE   = 0x000000010, OP_QUERY     = 0x000000020,
   OP_CLEAN    = 0x000000040, OP_VERSION   = 0x000000080,
   OP_HELP     = 0x000000100, OP_CRAWL     = 0x000000200,
   A_SEARCH    = 0x000000400, A_CATEGORY   = 0x000000800,
   A_INFO      = 0x000001000, A_LIST       = 0x000002000,
   A_REFRESH   = 0x000004000, A_UPGRADE    = 0x000008000,
   A_MENU      = 0x000010000, A_DESKTOP    = 0x000020000,
   A_APPS      = 0x000040000, A_NOSAVE     = 0x000080000,
   GB_NOMERGE  = 0x000100000, GB_NOCONFIRM = 0x000200000,
   GB_NEEDED   = 0x000400000, GB_NOBAR     = 0x000800000,
   A_BACKUP    = 0x001000000, A_ALL        = 0x002000000,
   A_INTEGRITY = 0x004000000, A_RM_CRPT    = 0x008000000,
   A_INST_CRPT = 0x010000000, A_QUERY_REPO = 0x020000000,
   OP_REPO_API = 0x040000000, A_COMMENT    = 0x080000000,
   A_RATE      = 0x100000000, A_DL_HISTORY = 0x200000000,
   A_RATE_GET  = 0x400000000, OP_EDIT      = 0x800000000,
} _HELPER_FLAGS;
typedef _HELPER_FLAGS (*_PARSE_FUNC)(char, char*, int*, _USR_DATA*); /* function prototype for parsing flags */

/* do we have operation? */
static int hasop(unsigned long long flags)
{
   return ((flags & OP_YAOURT)   || (flags & OP_SYNC)    ||
           (flags & OP_REMOVE)   || (flags & OP_UPGRADE) ||
           (flags & OP_CRAWL)    || (flags & OP_CLEAN)   ||
           (flags & OP_QUERY)    || (flags & OP_VERSION) ||
           (flags & OP_REPO_API) || (flags &OP_EDIT));
}

/* are we querying information? */
static int isquery(unsigned long long flags)
{
   return ((flags & OP_QUERY)   || (flags & A_SEARCH) ||
           (flags & A_CATEGORY) || (flags & A_LIST)   ||
           (flags & A_INFO)     || (flags & A_QUERY_REPO));
}

/* do we need targets for our operation? */
static int needtarget(unsigned long long flags)
{
   return  (!(flags & OP_CLEAN)  && !isquery(flags)        &&
            !(flags & A_REFRESH) && !(flags & OP_UPGRADE)  &&
            !(flags & A_UPGRADE) && !(flags & OP_CRAWL)    &&
            !(flags & A_ALL)     && !(flags & OP_REPO_API) &&
            !(flags & OP_EDIT));
}

/* set destination */
static unsigned long long setdest(_HELPER_FLAGS dest, _USR_DATA *data)
{
   data->flags = data->flags & ~A_MENU;
   data->flags = data->flags & ~A_DESKTOP;
   data->flags = data->flags & ~A_APPS;
   return dest;
}

/* parse global flags */
static _HELPER_FLAGS getglob(char c, char *arg, int *skipn, _USR_DATA *data)
{
   (void)arg, (void)skipn, (void)data;
   if (c == 'v')        ++_VERBOSE;
   else if (c == 'q')   ++_QUIET;
   else if (c == 'f')   return GB_FORCE;
   else if (c == 't')   pndman_set_color(0);
   else if (c == 'r')   _setroot(NULL, data);
   return 0;
}

/* parse operation flags */
static _HELPER_FLAGS getop(char c, char *arg, int *skipn, _USR_DATA *data)
{
   (void)arg, (void)skipn, (void)data;
   if (c == 'S')        return OP_SYNC;
   else if (c == 'U')   return OP_UPGRADE;
   else if (c == 'R')   return OP_REMOVE;
   else if (c == 'Q')   return OP_QUERY;
   else if (c == 'A')   return OP_REPO_API;
   else if (c == 'P')   return OP_CRAWL;
   else if (c == 'C')   return OP_CLEAN;
   else if (c == 'E')   return OP_EDIT;
   else if (c == 'V')   return OP_VERSION;
   else if (c == 'h')   return OP_HELP;
   return 0;
}

/* parse sync flags */
static _HELPER_FLAGS getsync(char c, char *arg, int *skipn, _USR_DATA *data)
{
   (void)arg, (void)skipn, (void)data;
   if (c == 's')        return A_SEARCH;
   else if (c == 'c')   return A_CATEGORY;
   else if (c == 'i')   return A_INFO;
   else if (c == 'l')   return A_LIST;
   else if (c == 'y')   return A_REFRESH;
   else if (c == 'u')   return A_UPGRADE;
   else if (c == 'm')   return setdest(A_MENU, data);
   else if (c == 'd')   return setdest(A_DESKTOP, data);
   else if (c == 'a')   return setdest(A_APPS, data);
   else if (c == 'b')   return A_BACKUP;
   return 0;
}

/* parse removal flags */
static _HELPER_FLAGS getremove(char c, char *arg, int *skipn, _USR_DATA *data)
{
   (void)arg, (void)skipn, (void)data;
   if (c == 'n')        return A_NOSAVE;
   else if (c == 's')   return OP_SYNC;
   else if (c == 'r')   return OP_REMOVE;
   return 0;
}

/* parse query flags */
static _HELPER_FLAGS getquery(char c, char *arg, int *skipn, _USR_DATA *data)
{
   (void)arg, (void)skipn, (void)data;
   if (c == 's')        return A_SEARCH;
   else if (c == 'c')   return A_CATEGORY;
   else if (c == 'i')   return A_INFO;
   else if (c == 'u')   return A_UPGRADE;
   else if (c == 'l')   return A_LIST;
   else if (c == 'd')   return A_QUERY_REPO;
   return 0;
}

/* parse repo api flags */
static _HELPER_FLAGS getrepoapi(char c, char *arg, int *skipn, _USR_DATA *data)
{
   (void)arg, (void)skipn, (void)data;
   if (c == 'c')        return A_COMMENT;
   else if (c == 'p')   return A_RATE;
   else if (c == 'g')   return A_RATE_GET;
   else if (c == 'd')   return A_DL_HISTORY;
   return 0;
}

/* parse crawl flags */
static _HELPER_FLAGS getcrawl(char c, char *arg, int *skipn, _USR_DATA *data)
{
   (void)arg, (void)skipn, (void)data;
   if (c == 'c')        return A_INTEGRITY;
   else if (c == 's')   return A_INST_CRPT;
   else if (c == 'd')   return A_RM_CRPT;
   return 0;
}

/* passes the information for correct flag parser function, and does extra handling if needed */
static void parse(_PARSE_FUNC func, const char *ref, char *arg, char *narg, int *skipn, _USR_DATA *data)
{
   unsigned int x, y;
   for (y = 0; y != strlen(ref); ++y)
      for (x = 0; x != strlen(arg); ++x)
         if (arg[x] == ref[y]) {
            data->flags |= func(ref[y], narg, skipn, data);
            if (func == getop && hasop(data->flags)) return; /* only one OP */
         }
}

/* determites which type of flags we need to parse */
static void parsearg(char *arg, char *narg, int *skipn, _USR_DATA *data)
{
   if (!strcmp(arg, "-r"))                _PASSARG(_setroot);     /* argument with argument */
   if (!strcmp(arg, "--root"))            _PASSARG(_setroot);     /* argument with argument */
   if (!strcmp(arg, "--help"))            _PASSFLG(OP_HELP);      /* another way of calling help */
   if (!strcmp(arg, "--edit"))            _PASSFLG(OP_EDIT);      /* another way of calling help */
   if (!strcmp(arg, "--version"))         _PASSFLG(OP_VERSION);   /* another way of calling version */
   if (!strcmp(arg, "--noconfirm"))       _PASSFLG(GB_NOCONFIRM); /* noconfirm option */
   if (!strcmp(arg, "--nomerge"))         _PASSFLG(GB_NOMERGE);   /* nomerge option */
   if (!strcmp(arg, "--needed"))          _PASSFLG(GB_NEEDED);    /* needed option */
   if (!strcmp(arg, "--nobar"))           _PASSFLG(GB_NOBAR);     /* nobar option */
   if (!strcmp(arg, "--nosave"))          _PASSFLG(A_NOSAVE);     /* nosave option */
   if (!strcmp(arg, "--backup"))          _PASSFLG(A_BACKUP);     /* backup option */
   if (!strcmp(arg, "--all"))             _PASSFLG(A_ALL);        /* all option */
   if (!strcmp(arg, "--check-integrity")) _PASSFLG(A_INTEGRITY);  /* -Pc alias */
   if (!strcmp(arg, "--install-corrupt")) _PASSFLG(A_INST_CRPT);  /* reinstall corrupt pnds */
   if (!strcmp(arg, "--remove-corrupt"))  _PASSFLG(A_RM_CRPT);    /* remove corrupt pnds */
   if (!strcmp(arg, "--add-key"))         _SETFLG(OP_REPO_API);   /* -A alias */
   if (arg[0] != '-')                     _PASSTHS(_addtarget);   /* not argument */
   parse(getglob, _G_ARG, arg, narg, skipn, data);
   if (!hasop(data->flags))         parse(getop, _OP_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_SYNC))     parse(getsync, _S_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_REMOVE))   parse(getremove, _R_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_QUERY))    parse(getquery, _Q_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_REPO_API)) parse(getrepoapi, _A_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_CRAWL))    parse(getcrawl, _P_ARG, arg, narg, skipn, data);
}

/* loop for argument parsing, handles argument skipping for arguments with arguments */
static void parseargs(int argc, char **argv, _USR_DATA *data)
{
   assert(data);
   int i, skipn = 0;
   for (i = 1; i != argc; ++i)
      if (!skipn) parsearg(argv[i], argc==i+1?NULL:argv[i+1][0]!='-'?argv[i+1]:NULL, &skipn, data);
      else skipn = 0;
}

/*   ____ ___  _   _ _____ ___ ____
 *  / ___/ _ \| \ | |  ___|_ _/ ___|
 * | |  | | | |  \| | |_   | | |  _
 * | |__| |_| | |\  |  _|  | | |_| |
 *  \____\___/|_| \_|_|   |___\____|
 *
 * __        ______      _    ____  ____  _____ ____
 * \ \      / /  _ \    / \  |  _ \|  _ \| ____|  _ \
 *  \ \ /\ / /| |_) |  / _ \ | |_) | |_) |  _| | |_) |
 *   \ V  V / |  _ <  / ___ \|  __/|  __/| |___|  _ <
 *    \_/\_/  |_| \_\/_/   \_\_|   |_|   |_____|_| \_\
 */

/* configuration wrapper for adding repository */
static int _addrepository(char **argv, int argc, _USR_DATA *data)
{
   assert(data);
   if (!argc) return RETURN_FAIL;
   if (!pndman_repository_add(argv[0], data->rlist))
      return RETURN_FAIL;
   return RETURN_OK;
}

/* configuration wrapper for adding device */
static int _adddevice(char **argv, int argc, _USR_DATA *data)
{
   pndman_device *newd;
   assert(data);
   if (!argc) return RETURN_FAIL;
   if (!(newd = pndman_device_add(argv[0], data->dlist)))
      return RETURN_FAIL;
   if (!data->dlist) data->dlist = newd;
   return RETURN_OK;
}

/* set destination */
static int _setdest(char **argv, int argc, _USR_DATA *data)
{
   assert(data);
   if (!argc) return RETURN_FAIL;
   if (!strupcmp(argv[0], "MENU"))         data->flags |= setdest(A_MENU, data);
   else if (!strupcmp(argv[0], "DESKTOP")) data->flags |= setdest(A_DESKTOP, data);
   else if (!strupcmp(argv[0], "APPS"))    data->flags |= setdest(A_APPS, data);
   return RETURN_OK;
}

/* always backup? */
static int _backup(char **argv, int argc, _USR_DATA *data)
{
   assert(data);
   if (!argc) return RETURN_FAIL;
   if (strtol(argv[0], (char **) NULL, 10))
      data->flags |= A_BACKUP;
   return RETURN_OK;
}

/* configuration wrapper for ignoring package */
static int _addignore(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   int i;
   assert(data);
   for (i = 0; i != argc; ++i) addignore(argv[i], &data->ilist);
   return RETURN_OK;
}

/* configuration wrapper for setting nomerge option */
static int _nomerge(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   assert(data);
   data->flags |= GB_NOMERGE;
   return RETURN_OK;
}

/* configuration wrapper for setting nobar option */
static int _nobar(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   assert(data);
   data->flags |= GB_NOBAR;
   return RETURN_OK;
}

/* configuration wrapper for setting queue limit */
static int _queue(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   assert(data);
   if (!argc) return RETURN_FAIL;
   if ((_QUEUE = strtol(argv[0], (char **) NULL, 10)) <= 0)
      _QUEUE = 5;
   return RETURN_OK;
}

/* configuration wrapper for setting quiet level */
static int _quiet(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   assert(data);
   if (!argc) return RETURN_FAIL;
   _QUIET = strtol(argv[0], (char **) NULL, 10);
   return RETURN_OK;
}

/* configuration wrapper for setting verbose level */
static int _verbose(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   assert(data);
   if (!argc) return RETURN_FAIL;
   _VERBOSE = strtol(argv[0], (char **) NULL, 10);
   return RETURN_OK;
}

/* configuration wrapper for setting color */
static int _color(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   assert(data);
   if (!argc) return RETURN_FAIL;
   pndman_set_color(strtol(argv[0], (char **) NULL, 10));
   return RETURN_OK;
}

/* configuration wrapper for logdl */
static int _logdl(char **argv, int argc, _USR_DATA *data)
{
   (void)argv, (void)argc;
   assert(data);
   _LOG_DL = 1;
   return RETURN_OK;
}

/*   ____ ___  _   _ _____ ___ ____
 *  / ___/ _ \| \ | |  ___|_ _/ ___|
 * | |  | | | |  \| | |_   | | |  _
 * | |__| |_| | |\  |  _|  | | |_| |
 *  \____\___/|_| \_|_|   |___\____|
 */

/* configuration syntax settings */
#define _CONFIG_SEPERATOR  ' '
#define _CONFIG_COMMENT    '#'
#define _CONFIG_QUOTE      '\"'
#define _CONFIG_MAX_VAR    24
#define _CONFIG_KEY_LEN    24
#define _CONFIG_ARG_LEN    256

typedef int (*_CONFIG_FUNC)(char **argv, int argc, _USR_DATA *data); /* setter prototype */
typedef struct _CONFIG_KEYS
{
   const char     key[_CONFIG_KEY_LEN];
   _CONFIG_FUNC   func;
} _CONFIG_KEYS;

/* configuration options */
static _CONFIG_KEYS _CONFIG_KEY[] = {
   { "repository",            _addrepository },
   { "device",                _adddevice },
   { "ignore",                _addignore },
   { "destination",           _setdest },
   { "backup",                _backup },
   { "nomerge",               _nomerge },
   { "nobar",                 _nobar },
   { "queue",                 _queue },
   { "quiet",                 _quiet },
   { "verbose",               _verbose },
   { "color",                 _color },
   { "logdl",                 _logdl },
   { "<this ends the list>",  NULL },
};

/* read arguments from key and pass to the function */
static int readconfig_arg(const char *key, _CONFIG_FUNC func, _USR_DATA *data, char *line)
{
   unsigned int i, p, argc, in_quote;
   char *argv[_CONFIG_MAX_VAR];

   /* reset args */
   for (i = 0; i != _CONFIG_MAX_VAR; ++i) {
      if (!(argv[i] = malloc(_CONFIG_ARG_LEN))) {
         for (; i > 0; --i) free(argv[i]);
         return RETURN_FAIL;
      }
      memset(argv[i], 0, _CONFIG_ARG_LEN);
   }

   /* check if we have any arguments to parse */
   i = strlen(key) + 1;
   if (strlen(line) < i) {
      func(argv, 0, data);
      for (i = 0; i != _CONFIG_MAX_VAR; ++i) free(argv[i]);
      return RETURN_OK;
   }

   /* get args */
   for (in_quote = 0, p = 0, argc = 0; i != strlen(line); ++i) {
      if (line[i] == _CONFIG_QUOTE) in_quote = !in_quote; /* check quote */
      /* check for seperator, and switch argument if:
       * we are not inside quote
       * we actually have something in our argument */
      else if (line[i] == _CONFIG_SEPERATOR && !in_quote && p) {
         if (++argc==_CONFIG_MAX_VAR) break;    /* next argument, or break on argument limit */
         p = 0;                                 /* flush */
      }
      /* read character to argument if:
       * it's not a seperator, expect if we are inside a quote */
      else if (line[i] != _CONFIG_SEPERATOR || in_quote)
         argv[argc][p++] = line[i];
   }
   if (p) ++argc; /* final argument */

   /* execute the function */
   func(argv, argc, data);
   for (i = 0; i != _CONFIG_MAX_VAR; ++i) free(argv[i]);
   return RETURN_OK;
}

/* create default configuration file */
static int mkconfig(const char *path)
{
   FILE *f;
   if (!(f = fopen(path, "w"))) return RETURN_FAIL;
   _printf(_MKCONFIG, path);
   fputs("#\n", f);
   fputs("# Configuration file for milkyhelper.\n", f);
   fputs("# 'Keys' can be listed multiple times, and they will be read just fine\n", f);
   fputs("#\n", f);
   fputs("\n# To add repositories, use 'repository' key\n", f);
   fputs("# repository \"<repository url>\"\n", f);
   fputs("\n# To add virual device (eg. directory), use 'device' key\n", f);
   fputs("# device \"<absolute path>\"\n", f);
   fputs("\n# To prevent packages from upgrading, use 'ignore' key\n", f);
   fputs("# ignore \"<pnd id>\" \"<pnd id>\" \"<pnd id>\" ...\n", f);
   fputs("\n# To change default install folder, use 'destination' key\n", f);
   fputs("# destination MENU DESKTOP APPS\n", f);
   fputs("\n# To disable repository merges when synchorizing, use 'nomerge' key\n", f);
   fputs("# nomerge\n", f);
   fputs("\n# To disable progress bar, use 'nobar' key\n", f);
   fputs("# nobar\n", f);
   fputs("\n# To allow repository to gather download history, use 'logdl' key\n", f);
   fputs("# logdl\n", f);
   fputs("\n# Queue limit for concurrent downloads\n", f);
   fputs("# queue 5\n", f);
   fputs("\n# Quiet level (0 = default, 1 = truncated, 2 = silent)\n", f);
   fputs("# quiet 0\n", f);
   fputs("\n# Verbose level (0 = none, 1 = fail, 2 = warn, 3 = crap)\n", f);
   fputs("# verbose 0\n", f);
   fputs("\n# Colored output\n", f);
   fputs("# color 1\n", f);
   fputs("\n# milkshake's repo is enabled by default\n", f);
   fputs("repository \"http://repo.openpandora.org/client/masterlist?com=true&bzip=true\"\n",f );
   fclose(f);
   return RETURN_OK;
}

/* read keys from configuration file */
static int readconfig(const char *path, _USR_DATA *data)
{
   unsigned int i;
   char line[LINE_MAX];
   FILE *f;
   assert(path && data);

   if (!(f = fopen(path, "r"))) {
      if (mkconfig(path) != RETURN_OK)
         return RETURN_FAIL;
      else if (!(f = fopen(path, "r")))
         return RETURN_FAIL;
   }

   while (fgets(line, LINE_MAX, f)) {
      if (!strtrim(line) || line[0] == _CONFIG_COMMENT) continue; /* trim and check if skip on comment */
      for (i = 0; _CONFIG_KEY[i].func; ++i)
         if (!strncmp(_CONFIG_KEY[i].key, line, strlen(_CONFIG_KEY[i].key))) {
            readconfig_arg(_CONFIG_KEY[i].key, _CONFIG_KEY[i].func, data, line);
            break;
         }
   }
   fclose(f);
   return RETURN_OK;
}

/* get path to milkyhelper's configuration file */
static int getconfigpath(char *path)
{
   if (getcfgpath(path) != RETURN_OK) return RETURN_FAIL;               /* $XDG_CONFIG_HOME */
   strncat(path, strlen(path)?"/"CONFIG_FILE:CONFIG_FILE, PATH_MAX-1);  /* $XDG_CONFIG_HOME/$CONFIG_FILE */
   return RETURN_OK;
}

/*  ___ _   _ ____  _   _ _____
 * |_ _| \ | |  _ \| | | |_   _|
 *  | ||  \| | |_) | | | | | |
 *  | || |\  |  __/| |_| | | |
 * |___|_| \_|_|    \___/  |_|
 */

/* check if pnd is installed */
static int pndinstalled(pndman_package *pnd, _USR_DATA *data)
{
   pndman_package *p;
   assert(pnd && data);
   for (p = data->rlist->pnd; p; p = p->next)
      if (pnd->id && p->id && !strcmp(pnd->id, p->id)) return RETURN_TRUE;
   return RETURN_FALSE;
}

/* get pnd title */
static const char* pndtitle(pndman_package *pnd, const char *lang)
{
   pndman_translated *t;
   for (t = pnd->title; t; t = t->next)
      if (t->lang && lang && !strupcmp(t->lang, lang)) return t->string?t->string:NULL;
   return NULL;
}

/* get pnd description */
static const char* pnddesc(pndman_package *pnd, const char *lang)
{
   pndman_translated *t;
   for (t = pnd->description; t; t = t->next)
      if (t->lang && lang && !strupcmp(t->lang, lang)) return t->string?t->string:NULL;
   return NULL;
}

/* get application title */
static const char* apptitle(pndman_application *app, const char *lang)
{
   pndman_translated *t;
   for (t = app->title; t; t = t->next)
      if (t->lang && lang && !strupcmp(t->lang, lang)) return t->string?t->string:NULL;
   return NULL;
}

/* get human readable elapsed time since */
static char* gettime(time_t unixtime)
{
   time_t nao;
   unsigned int seconds, minutes, hours, days, months, years;
   unsigned int rseconds, rminutes, rhours, rdays, rmonths, ryears;
   char *str;

   if (!(str = malloc(4+7+2+8+2+6+2+7+2+9+2+8+1)))
      return NULL;

   nao = time(0);
   rseconds  = seconds = (unsigned int)(nao - unixtime);
   rminutes  = minutes = seconds / 60;
   rhours    = hours   = minutes / 60;
   rdays     = days    = hours   / 24;
   rmonths   = months  = days    / 31;
   ryears    = years   = months  / 12;

   if (years)   rmonths   = months  - years   * 12;
   if (months)  rdays     = days    - months  * 31;
   if (days)    rhours    = hours   - days    * 24;
   if (hours)   rminutes  = minutes - hours   * 60;
   if (minutes) rseconds  = seconds - minutes * 60;

   memset(str, 0, 4+7+2+8+2+6+2+7+2+9+2+8+1);
   sprintf(str, "%.0d%s%.0d%s%.0d%s%.0d%s%.0d%s%.0d%s",
         ryears,  years    ? " years "   : "",
         rmonths, months   ? " months "  : "",
         rdays,   days     ? " days "    : "",
         rhours,  hours    ? " hours "   : "",
         rminutes, minutes ? " minutes " : "",
         rseconds, seconds ? " seconds"  : "");
   return str;
}

/* fill default title style */
static void filltitle(pndman_package *pnd, char *buffer)
{
   memset(buffer, 0, LINE_MAX-1);
   if (!pnd) return;
   snprintf(buffer, LINE_MAX-1, "\4%s\5/%s%s\7",
         pnd->category ? pnd->category->sub ?
         pnd->category->sub : pnd->category->main : "nogroup",
         pnd->commercial?"\1":"\2", pnd->id);
}

/* repoinfo */
static void repoinfo(pndman_repository *repo, _USR_DATA *data)
{
   (void)data;
   char *synced = NULL;
   unsigned int packages = 0;
   pndman_package *p;
   assert(repo);

   for (p = repo->pnd; p; p = p->next) ++packages;
   _printf(_D"\1Repository with \4%d \1packages.", packages);
   _printf("\1─┬─\4%s", repo->url);
   _printf("\1 └─┬──\2%s [\3%s\5]", repo->name, repo->version);
   if (repo->updates)  _printf("\1   ├──\5%s", repo->updates);
   if (repo->api.root) _printf("\1   ├─%s─\5%s",
         (repo->api.username||repo->api.key)?"┬":"",repo->api.root);
   if (repo->api.username) _printf("\1   │ %s─\3%s",repo->api.key?"├":"└", repo->api.username);
   if (repo->api.key)      _printf("\1   │ └─\3%s", repo->api.key);
   if (repo->timestamp) synced = gettime(repo->timestamp);
   _printf("\1   └──\5%s%s%s", synced?"\2Synced ":"",synced?synced:"\3Not synced!", synced?" ago":"");
   if (synced) free(synced);
}

/* pndinfo */
static void pndinfo(pndman_package *pnd, _USR_DATA *data, size_t longest_title)
{
   pndman_category *c;
   pndman_license  *l;
   pndman_application *a;
   char *title, *desc, *info, *time;
   char buffer[LINE_MAX];
   size_t i;
   assert(pnd);

   /* get description */
   if (!(desc = strstrip((char*)pnddesc(pnd, data->syslc))))
      desc = strstrip((char*)pnddesc(pnd, "en_US")); /* fallback locale */

   /* strip description to maximum length */
   if (!(data->flags & A_INFO) && desc && strlen(desc) > DESCRIPTION_LENGTH)
   {
      desc[DESCRIPTION_LENGTH-3] = '.';
      desc[DESCRIPTION_LENGTH-2] = '.';
      desc[DESCRIPTION_LENGTH-1] = '.';
      desc[DESCRIPTION_LENGTH]   = '\0';
   }

   /* info style */
   if (data->flags & A_INFO) {
      _printf("\2ID            \5: %s%s", pnd->commercial?"\1":"\2", pnd->id);
      _printf("\2Installed     \5: %s", pndinstalled(pnd, data)?"Yes":"No");
      if ((data->flags & OP_QUERY)) {
         _printf("\2Path          \5: %s", pnd->path);
      }
      _printf("\2Repository    \5: %s", pnd->repository?pnd->repository:"Foreign");
      _printf("\2Update        \5: %s", pnd->update?"Yes":"No");
      if ((time = gettime(pnd->modified_time))) {
         _printf("\2Modified      \5: %s ago", time);
         free(time);
      }
      if (pnd->category) {
         _printf("\2Categories    \5: \7");
         for (c = pnd->category; c; c = c->next)
            printf("%s%s%s ", c->main?c->main:"", c->sub?" ":"", c->sub?c->sub:"");
         puts("");
      }
      if (pnd->license) {
         _printf("\2Licenses      \5: \7");
         for (l = pnd->license; l; l = l->next)
            printf("%s ", l->name?l->name:"");
         puts("");
      }
      if (!(title = strstrip((char*)pndtitle(pnd, data->syslc))))
         title = strstrip((char*)pndtitle(pnd, "en_US")); /* fallback locale */
      if (title) {
         _printf("\2Title         \5: %s", title?title:pnd->id);
         free(title);
      }
      _printf("\2Size          \5: %.2f MiB", (float)pnd->size/1048576);
      _printf("\2Version       \5: %s.%s.%s.%s %s",
            pnd->version.major,
            pnd->version.minor,
            pnd->version.release,
            pnd->version.build,
            pnd->version.type==PND_VERSION_BETA?"Beta":
            pnd->version.type==PND_VERSION_ALPHA?"Alpha":"Release");
      _printf("\2Rating        \5: %d", pnd->rating);
      if (pnd->url) _printf("\2URL           \5: %s", pnd->url);
      if (pnd->md5) _printf("\2MD5           \5: %s", pnd->md5);
      if (pnd->author.name) _printf("\2Author        \5: %s", pnd->author.name);
      if (pnd->author.website) _printf("\2Author page   \5: %s", pnd->author.website);
      if (pnd->vendor) _printf("\2Vendor        \5: %s", pnd->vendor);
      if (pnd->icon) _printf("\2Icon          \5: %s", pnd->icon);
      if (desc) {
         NEWLINE();
         _printf("\3Description   \5:\n%s", desc);
      }
      if ((info = strstrip((char*)pnd->info))) {
         NEWLINE();
         _printf("\3Information   \5:\n%s", info);
         free(info);
        }
   } else if (((data->flags & A_SEARCH) && !(data->flags & A_LIST))
          || (data->flags & OP_YAOURT)) {
      /* default style */
      filltitle(pnd, buffer);
      _printf(buffer);
      for (i = longest_title; i > strlen(buffer); --i) printf(" ");
      _printf(" %s%s\5[\3%d\5]\5[\4%s.%s.%s.%s%s\5]",
                      pndinstalled(pnd, data)?"\5[\2I\5]":"   ",
                      pnd->update?"\5[\1U\5]":"   ", pnd->rating,
                      pnd->version.major,   pnd->version.minor,
                      pnd->version.release, pnd->version.build,
                      pnd->version.type==PND_VERSION_BETA?" \1β":
                      pnd->version.type==PND_VERSION_ALPHA?" \3α":"");
      _printf("    \5%s\7", desc?desc:"...");
   } else {
      _printf("\2%s \4%s.%s.%s.%s%s\7", pnd->id,
            pnd->version.major,   pnd->version.minor,
            pnd->version.release, pnd->version.build,
            pnd->version.type==PND_VERSION_BETA?" beta":
            pnd->version.type==PND_VERSION_ALPHA?" alpha":"");
      if ((data->flags & OP_QUERY) && (data->flags & A_LIST)) {
         pndman_package_crawl_single_package(1, pnd);
         if (!(title = strstrip((char*)pndtitle(pnd, data->syslc))))
            title = strstrip((char*)pndtitle(pnd, "en_US")); /* fallback locale */
         _printf("\n"
                 "\1─┬─\4%s", (title && strlen(title))?title:pnd->id);
         _printf("\1 └─%s─\5%s/%s", pnd->app?"┬":"",
               pnd->mount, pnd->path);
         if (title) free(title);
         for (a = pnd->app; a; a = a->next) {
            if (!(title = strstrip((char*)apptitle(a, data->syslc))))
               title = strstrip((char*)apptitle(a, data->syslc));
            _printf("   \1%s─┬─\5%s", a->next?"├":"└", (title && strlen(title))?title:a->id);
            _printf("   \1%s └─\5%s/pandora/appdata/%s", a->next?"│":" ", pnd->mount, a->appdata);
            if (title) free(title);
         }
      }
   }
   if (desc) free(desc);
}

/* dialog for repository selection */
static pndman_repository* repositorydialog(_USR_DATA *data)
{
   unsigned int i, s;
   char response[32];
   pndman_repository *r;
   assert(data);

   if (!data->rlist->next)
      return NULL;

   i = 0;
   fflush(stdout);
   _printf(_SELECT_REPOSITORY);
   for (r = data->rlist->next; r; r = r->next)
      _printf("\4%d. \5%s", ++i, r->name?r->name:r->url);
   _printf(_INPUT_LINE"\7");
   fflush(stdout);

   if (!fgets(response, sizeof(response), stdin))
      return NULL;

   if (!strtrim(response)) return NULL;
   if ((s = strtol(response, (char **) NULL, 10)) <= 0)
      return NULL;

   /* good answer got, return repository */
   for (i = 0, r = data->rlist->next; r; r = r->next)
      if (++i==s) return r;

   return NULL;
}

/* dialog for asking root device where to perform operations on, when no such device isn't available yet */
static int rootdialog(_USR_DATA *data)
{
   unsigned int i, s;
   char response[32];
   pndman_device *d;
   assert(data);

   i = 0;
   fflush(stdout);
   _printf(_NO_ROOT_YET);
   for (d = data->dlist; d; d = d->next) _printf("\4%d. \5%s", ++i, d->mount);
   _printf(_INPUT_LINE"\7");
   fflush(stdout);

   if (!fgets(response, sizeof(response), stdin))
      return RETURN_FAIL;

   if (!strtrim(response)) return RETURN_FAIL;
   if ((s = strtol(response, (char **) NULL, 10)) <= 0)
      return RETURN_FAIL;

   /* good answer got, set root */
   for (i = 0, d = data->dlist; d; d = d->next)
      if (++i==s) {
         data->root = d;
         if (data->root) data->no_action = _ROOT_SET;
         return RETURN_OK;
      }

   return RETURN_FAIL;
}

static size_t getlongtarget(_USR_DATA *data)
{
   pndman_repository *r;
   pndman_package *p;
   char buffer[LINE_MAX];
   size_t longest_title = 0, len;

   for (r = data->rlist; r; r = r->next)
      for (p = r->pnd; p; p = p->next) {
         filltitle(p, buffer);
         if ((len = strlen(buffer)) > longest_title)
            longest_title = len;
      }
   return longest_title>54?54:longest_title;
}

/* yaourt mode dialog */
static int yaourtdialog(_USR_DATA *data)
{
   char response[1024], num[5];
   unsigned int i, c, match;
   size_t longest_title;
   _USR_TARGET *t, *nt, *nlist = NULL;

   if (!data->tlist) return RETURN_FALSE;

   /* get longest title */
   longest_title = getlongtarget(data);

   /* output query */
   for (i = 0, t = data->tlist; t; t = t->next) {
      _printf("\5%3d. \7", ++i);
      pndinfo(t->pnd, data, longest_title);
      NEWLINE();
   }

   /* output question */
   _printf(_YAOURT_DIALOG);
   _printf(_INPUT_LINE"\7");
   fflush(stdout);

   /* get number */
   if (!fgets(response, sizeof(response), stdin))
      return RETURN_FALSE;

   /* everything non number as first character cancels */
   if (response[0]  <  48 ||
       response[0]  >  57)
      return RETURN_FALSE;

   /* go through all numbers */
   memset(num, 0, 5);
   for (i = 0, c = 0; i != strlen(response); ++i) {
      if (!isspace(response[i])) num[c++] = response[i];
      else {
         if (!(match = strtol(num, (char **) NULL, 10)))
         { c = 0; continue; }
         c = 1; t = data->tlist;
         while (c != match) {
            if (t) t = t->next;
            ++c;
         }
         memset(num, 0, 5); c = 0;
         if (t && (nt = addtarget(t->pnd->id, &nlist))) {
            if (!nlist) nlist = nt;
            nt->pnd = t->pnd;
         }
      }
   }
   NEWLINE();

   /* swap target lists */
   freetarget_all(data->tlist);
   data->tlist = nlist;
   return RETURN_TRUE;
}

/* Yes/No question dialog */
static int _yesno_dialog(int noconfirm, char yn, char *fmt, va_list args)
{
   char buffer[LINE_MAX];
   char response[32];

   fflush(stdout);
   memset(buffer, 0, LINE_MAX);
   vsnprintf(buffer, LINE_MAX-1, fmt, args);
   _printf("%s %s %s\7", buffer, yn?_YESNO:_NOYES, noconfirm?"\n":"");
   if (noconfirm) return yn;

   fflush(stdout);
   if (!fgets(response, sizeof(response), stdin))
      return RETURN_FALSE;

   if (!strtrim(response)) return yn;
   if (!strupcmp(response, "Y") || !strupcmp(response, "YES"))   return RETURN_TRUE;
   if (!strupcmp(response, "N") || !strupcmp(response, "NO"))    return RETURN_FALSE;
   return RETURN_FALSE;
}

#define yesno(c, x, ...) _yesno_base((c->flags & GB_NOCONFIRM), 1, x, ##__VA_ARGS__) /* shows [Y/n] dialog, Y = default */
#define noyes(c, x, ...) _yesno_base((c->flags & GB_NOCONFIRM), 0, x, ##__VA_ARGS__) /* shows [y/N] dialog, N = default */

/* base for macro */
static int _yesno_base(int noconfirm, int yn, char *fmt, ...)
{
   int ret; va_list args;
   va_start(args, fmt);
   ret = _yesno_dialog(noconfirm, yn, fmt, args);
   va_end(args);
   return ret;
}

/* show progressbar, shows dots instead when total_to_download is not known */
static void progressbar(double downloaded, double total_to_download)
{
   size_t dots, i, pwdt;
   double fraction;

   pwdt = 40; /* width */
   if (total_to_download <= downloaded) total_to_download = downloaded + 1;
   fraction = downloaded/total_to_download;
   dots     = round(fraction * pwdt);

   _printf("\4%3.0f%% \5[\7", fraction * 100);
   for (i = 0; i != dots; ++i) _printf("\1=\7");
   for (     ; i != pwdt; ++i) printf(" ");
   _printf("\5] \1%-6.2f \5MiB / \2%-6.2f \5MiB\r\7",
         downloaded/1048576, total_to_download/1048567);
   fflush(stdout);
}

/* dialog shown before performing action */
static int pre_op_dialog(_USR_DATA *data)
{
   _USR_TARGET *t, *tn;
   unsigned int count, update, skipping, ignore, reinstall;
   double size;
   char *unit, gotdialog = 0;
   assert(data);

   /* init */
   count = update = skipping = ignore = reinstall = 0;

   /* ask for ignores */
   for (t = data->tlist; t; t = tn) {
      tn = t->next; ++count;
      if (!pndinstalled(t->pnd, data)) {
         if (checkignore((t->pnd?t->pnd->id:t->id), data)) {
            if ((_QUIET && ++ignore==0) ||
                (!ignore && !yesno(data, _PND_IGNORED_FORCE, t->pnd->id))) {
               if (!_QUIET) _printf(_WARNING_SKIPPING, t->pnd->id);
               data->tlist = freetarget(t);
               --count; gotdialog = 1;
               ++skipping;
            }
         }
      } else {
         if ((data->flags & GB_NEEDED)    ||
             (!(data->flags & A_UPGRADE)  &&
              !(data->flags & OP_UPGRADE) &&
              !(data->flags & OP_REMOVE)  &&
              !t->pnd->update             &&
              ((_QUIET && ++reinstall==0) ||
              (!reinstall && !yesno(data, _PND_REINSTALL, t->pnd->id))))) {
            if ((data->flags & GB_NEEDED)) {
               if (!_QUIET) _printf(_WARNING_UPDATE, t->pnd->id);
               else update++;
            }
            data->tlist = freetarget(t);
            --count; gotdialog = 1;
         }
      }
   }

   /* count questions on all quiet levels */
   if (_QUIET) {
      if (ignore)
         if (!yesno(data, _PND_IGNORED_FORCE_Q, ignore)) {
            for (t = data->tlist; t; t = tn) {
               if (!pndinstalled(t->pnd, data)) {
                  if (checkignore((t->pnd?t->pnd->id:t->id), data))
                     data->tlist = freetarget(t);
               }
            }
         }
      if (reinstall)
         if (!yesno(data, _PND_REINSTALL_Q, reinstall)) {
            for (t = data->tlist; t; t = tn) {
               if (pndinstalled(t->pnd, data)  &&
                  ((data->flags & GB_NEEDED)   ||
                  (!(data->flags & A_UPGRADE)  &&
                   !(data->flags & OP_UPGRADE) &&
                   !(data->flags & OP_REMOVE)  &&
                   !t->pnd->update))) {
                  data->tlist = freetarget(t);
                  skipping = reinstall;
               }
            }
        }
   }

   /* print count warnings on quiet
    * level 1 */
   if (_QUIET == 1) {
      if (skipping)  _printf(_WARNING_SKIPPING_Q, skipping);
      if (update)    _printf(_WARNING_UPDATE_Q, update);
   }

   /* nothing to perform action on */
   if (!count || !data->tlist) {
      _printf(_NOTHING_TO_DO);
      return RETURN_FALSE;
   }

   /* formatting */
   if (gotdialog) NEWLINE();

   /* show target line */
   _printf(_TARGET_LINE"\7", count);
   for (size = 0, t = data->tlist; t; t = t->next) {
      if (_QUIET < 2) _printf("\4%s\5%s\7", t->pnd->id, t->next?", ":"");
      size += t->pnd->size;
   }
   NEWLINE();

   /* get size unit, and divide bytes */
   if (size < 1024) {
      unit = _UNIT_BYTE;
   } else if (size < 1024*1024) {
      size /= 1024;
      unit = _UNIT_KIB;
   } else {
      size /= 1024*1024;
      unit = _UNIT_MIB;
   }

   /* print action target paths */
   if ((data->flags & OP_SYNC) && !(data->flags & A_UPGRADE))
   {
      _printf(_TARGET_MEDIA, data->root->mount);
      _printf(_TARGET_PATH, (data->flags & A_APPS)?"/apps":
                            (data->flags & A_DESKTOP)?"/desktop":
                            (data->flags & A_MENU)?"/menu":"default/reinstall");
   }
   NEWLINE();

   /* print action size */
   _printf((data->flags & OP_REMOVE)?_REMOVE_SIZE:_DOWNLOAD_SIZE, size, unit);
   if (!yesno(data, (data->flags & OP_REMOVE)?_CONTINUE_UNINSTALL:
                    (data->flags & A_UPGRADE)||(data->flags & OP_UPGRADE)?
                    _CONTINUE_UPGRADE:_CONTINUE_INSTALL))
      return RETURN_FALSE;

   NEWLINE();
   return RETURN_TRUE;
}

/*  ____  ____   ___   ____ _____ ____ ____
 * |  _ \|  _ \ / _ \ / ___| ____/ ___/ ___|
 * | |_) | |_) | | | | |   |  _| \___ \___ \
 * |  __/|  _ <| |_| | |___| |___ ___) |__) |
 * |_|   |_| \_\\___/ \____|_____|____/____/
 */

/* synchorize callback */
static void synccallback(pndman_curl_code code, pndman_sync_handle *handle)
{
   _USR_DATA *data = (_USR_DATA*)handle->user_data;

   if (code == PNDMAN_CURL_PROGRESS) {
      if (handle->progress.done == 2) return; /* silly hack */
      data->tdl  += handle->progress.download;
      data->ttdl += handle->progress.total_to_download;
      handle->progress.done = 2; /* mark as processed */
   }

   if (code != PNDMAN_CURL_FAIL &&
       code != PNDMAN_CURL_DONE) return;

   ERASE();
        if (code == PNDMAN_CURL_DONE) _printf(_REPO_SYNCED, handle->repository->name);
   else if (code == PNDMAN_CURL_FAIL) {
      _printf(_REPO_SYNCED_NOT, handle->repository->name?handle->repository->name:handle->repository->url);
      _printf(_D"\1%s", handle->error);
   }

   /* we can free this already */
   pndman_sync_handle_free(handle);
}

/* synchorize repositories */
static int syncrepos(pndman_repository *rs, _USR_DATA *data)
{
   pndman_repository *r;
   unsigned int c = 0;
   int ret;

   data->tdl = 0; data->ttdl = 0;
   for (r = rs; r; r = r->next) ++c;
   pndman_sync_handle handle[c]; r = rs;
   for (c = 0; r; r = r->next) {
      pndman_sync_handle_init(&handle[c]);
      handle[c].callback   = synccallback;
      handle[c].user_data  = data;
      handle[c].flags      = (data->flags & GB_NOMERGE)?PNDMAN_SYNC_FULL:0;
      handle[c].repository = r;
      pndman_sync_handle_perform(&handle[c++]);
   }

   if (!(data->flags & GB_NOBAR)) showcursor(0);
   while ((ret = pndman_curl_process(0, 1000) > 0)) {
      for (r = rs, c = 0; r; r = r->next) {
         if (handle[c].progress.done == 2) handle[c].progress.done = 0; /* clear our hack */
         if (handle[c].progress.done) data->tdl += handle[c].progress.total_to_download;
      }
      if (!(data->flags & GB_NOBAR)) progressbar(data->tdl, data->ttdl);
      data->tdl = 0; data->ttdl = 0;
   }
   ERASE();
   if (!(data->flags & GB_NOBAR)) showcursor(1);
   if (ret == -1) _printf(_INTERNAL_CURL_FAIL);

   for (r = rs, c = 0; r; r = r->next) {
      pndman_sync_handle_free(&handle[c]);
      ++c;
   }

   return ret!=-1?RETURN_OK:RETURN_FAIL;
}

/* returns true or false depending if query type matches */
static int matchquery(pndman_package *p, _USR_TARGET *t, _USR_DATA *data)
{
   pndman_translated *s;
   pndman_category   *c;
   assert(p && t && data);

   /* compary category? */
   if ((data->flags & A_CATEGORY)) {
      for (c = p->category; c; c = c->next)
         if (c->main && t->id && !strupcmp(c->main, t->id)) return RETURN_TRUE;
         else if (c->sub && t->id && !strupcmp(c->sub, t->id)) return RETURN_TRUE;
      return RETURN_FALSE;
   }

   if (p->id && t->id && strupstr(p->id, t->id)) return RETURN_TRUE;
   for (s = p->title; s; s = s->next)
      if (s->string && t->id && strupstr(s->string, t->id)) return RETURN_TRUE;
   for (s = p->description; s; s = s->next)
      if (s->string && t->id && strupstr(s->string, t->id)) return RETURN_TRUE;

   return RETURN_FALSE;
}

/* search repository */
static int searchrepo(pndman_repository *rs, _USR_DATA *data)
{
   pndman_repository *r;
   char donl = 0;
   assert(rs && data);

   /* lame search */
   for (r = rs->prev?rs:rs->next; r; r = r->next) {
      if (donl) NEWLINE();
      repoinfo(r, data);
      donl = 1;
   }

   return RETURN_OK;
}

/* search pnd from repository */
static int searchpnd(pndman_repository *r, _USR_DATA *data, size_t longest_title)
{
   pndman_package *p, *pp;
   _USR_TARGET *t;
   char donl = 0;
   assert(r && data);

   /* lame search for now */
   if (!data->tlist)
      for (p = r->pnd; p; p = p->next) {
         if ((data->flags & A_UPGRADE) && !p->update)
            continue;
         for (pp = p->next_installed; pp; pp = pp->next_installed) {
            pndinfo(pp, data, longest_title);
            if (pp->next_installed) NEWLINE();
            donl = 1;
         }
         pndinfo(p, data, longest_title);
         if (p->next) NEWLINE();
         donl = 1;
      }
   else
      for (t = data->tlist; t; t = t->next)
         for (p = r->pnd; p; p = p->next) {
            if ((data->flags & A_UPGRADE) && !p->update)
               continue;
            if (matchquery(p, t, data)) {
               if (donl) NEWLINE();
               pndinfo(p, data, longest_title);
               donl = 1;
            }
         }

   /* formatting */
   if (!(data->flags & A_INFO) && donl) NEWLINE();
   return RETURN_OK;
}

/* get pnd from target id */
static int targetpnd(pndman_repository *rs, _USR_DATA *data, int up)
{
   pndman_package    *p;
   pndman_repository *r;
   _USR_TARGET       *t, *tn, *ts = NULL;
   assert(rs && data);

   /* try cmp first */
   for (t = data->tlist; t && t != ts; t = t->next) {
      if (!(data->flags & OP_YAOURT) && t->pnd) continue;
      for (r = rs; r; r = r->next) {
         for (p = r->pnd; p; p = p->next)
            if (!strcmp(p->id, t->id)) {
               if (_VERBOSE >= 1) _printf(_D"\2A: \4%s %s", t->id, p->id);
               if (t->pnd && (tn = addtarget(p->id, &data->tlist))) {
                  if (!ts) ts = tn;
                  tn->pnd = p;
               } else if (!t->pnd)
                  t->pnd = p;
               if (!(data->flags & OP_YAOURT)) break;
            }
         if (rs == data->rlist) break; /* we only want local repository */
      }
   }

   /* then strstr */
   for (t = data->tlist; t && t != ts; t = t->next) {
      if (!(data->flags & OP_YAOURT) && t->pnd) continue;
      for (r = rs; r; r = r->next) {
         for (p = r->pnd; p; p = p->next)
            if (strstr(p->id, t->id)) {
               if (_VERBOSE >= 1) _printf(_D"\2A: \4%s %s", t->id, p->id);
               if (t->pnd && (tn = addtarget(p->id, &data->tlist))) {
                  if (!ts) ts = tn;
                  tn->pnd = p;
               } else if (!t->pnd)
                  t->pnd = p;
               if (!(data->flags & OP_YAOURT)) break;
            }
         if (rs == data->rlist) break; /* we only want local repository */
      }
   }

   /* clean pnd's which wasn't found */
   for (t = data->tlist; t && t != ts; t = tn) {
      tn = t->next;
      if (!t->pnd) {
         _printf(_PND_NOT_FOUND, t->id);
         data->tlist = freetarget(t);
         continue;
      }
      if (up && !t->pnd->update) {
         if (!_QUIET) _printf(_PND_NO_UPDATE, t->id);
         data->tlist = freetarget(t);
         continue;
      }
   }

   /* return true if something found, else false */
   return data->tlist ? RETURN_TRUE : RETURN_FALSE;
}

/* make pnd's which have update a target */
static void targetup(pndman_repository *r, _USR_DATA *data)
{
   pndman_package *p;
   assert(r && data);
   for (; r; r = r->next)
      for (p = r->pnd; p; p = p->next)
         if (p->update && !checkignore(p->id, data)) {
            if (_VERBOSE >= 1) puts(p->id);
            addtarget(p->id, &data->tlist);
         }
}

/* list updates in local repository */
static void listupdate(_USR_DATA *data)
{
   pndman_package *p;
   unsigned int count = 0;
   assert(data);
   for (p = data->rlist->pnd; p; p = p->next)
      if (p->update) {
         if (!_QUIET) _printf(_PND_HAS_UPDATE, p->id);
         ++count;
      }
   if (_QUIET == 1 && count) _printf(_PND_HAS_UPDATE_Q, count);
}

/* get handle flags from milkyhelper's flags */
static unsigned long long handleflagsfromflags(pndman_package *p, unsigned long long flags)
{
   unsigned int hflags = 0;
   if ((flags & OP_SYNC))           hflags |= PNDMAN_PACKAGE_INSTALL;
   else if ((flags & OP_REMOVE))    hflags |= PNDMAN_PACKAGE_REMOVE;
   if ((flags & GB_FORCE))          hflags |= PNDMAN_PACKAGE_FORCE;

   /* don't assing destination on upgrade */
   if (!(flags & OP_UPGRADE) && !(flags & A_UPGRADE)) {
      if ((flags & A_DESKTOP))         hflags |= PNDMAN_PACKAGE_INSTALL_DESKTOP;
      else if ((flags & A_APPS))       hflags |= PNDMAN_PACKAGE_INSTALL_APPS;
      else if ((flags & A_MENU))       hflags |= PNDMAN_PACKAGE_INSTALL_MENU;
      else if (p->path && strstr(p->path, "pandora/desktop"))
         hflags |= PNDMAN_PACKAGE_INSTALL_DESKTOP;
      else if (p->path && strstr(p->path, "pandora/apps"))
         hflags |= PNDMAN_PACKAGE_INSTALL_APPS;
      else hflags |= PNDMAN_PACKAGE_INSTALL_MENU; /* default to menu */
   }

   /* create backup of old file */
   if (flags & A_BACKUP) hflags |= PNDMAN_PACKAGE_BACKUP;
   if (_LOG_DL)          hflags |= PNDMAN_PACKAGE_LOG_HISTORY;
   return hflags;
}

/* get operation string from milkyhelper's flags */
static const char* opstrfromflags(char done, unsigned long long flags)
{
   if ((flags & A_UPGRADE) || (flags & OP_UPGRADE))
                                 return done?_UPGRADE_DONE:_UPGRADE_FAIL;
   else if ((flags & OP_SYNC))   return done?_INSTALL_DONE:_INSTALL_FAIL;
   else if ((flags & OP_REMOVE)) return done?_REMOVE_DONE:_REMOVE_FAIL;
   return _UNKNOWN_OPERATION;
}

/* commit handle to repository */
static void commithandle(_USR_DATA *data, pndman_package_handle *handle)
{
   assert(data && handle);
   if (handle->error) {
      _printf(_D"\4%s \1failed with: \1%s", handle->name, handle->error);
      return;
   }
   if (pndman_package_handle_commit(handle, data->rlist) != RETURN_OK)
      _printf(opstrfromflags(0,data->flags), handle->name);
   else
      _printf(opstrfromflags(1,data->flags), handle->name);
}

/* remove directory */
static int _rmdir_(const char *dir)
{
   char tmp[PATH_MAX];
#ifndef _WIN32
   DIR *dp;
   struct dirent *ep;
#else
   WIN32_FIND_DATA dp;
   HANDLE hFind = NULL;
#endif

   memset(tmp, 0, PATH_MAX);
#ifndef _WIN32
   if (!(dp = opendir(dir)))
      return 1;
   while ((ep = readdir(dp))) {
      if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")) continue;
      if (ep->d_type == DT_DIR) {
         strncpy(tmp, dir, PATH_MAX-1);
         strncat(tmp, "/", PATH_MAX-1);
         strncat(tmp, ep->d_name, PATH_MAX-1);
         _rmdir_(tmp);
      }
      strncpy(tmp, dir, PATH_MAX-1);
      strncat(tmp, "/", PATH_MAX-1);
      strncat(tmp, ep->d_name, PATH_MAX-1);
      unlink(tmp);
   }
   closedir(dp);
#else
   strncpy(tmp, dir,  PATH_MAX-1);
   strncat(tmp, "/*", PATH_MAX-1);

   if ((hFind = FindFirstFile(tmp, &dp)) == INVALID_HANDLE_VALUE)
      return 1;

   do {
      if (!strcmp(dp.cFileName, ".") || !strcmp(dp.cFileName, "..")) continue;
      if (dp.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
         strncpy(tmp, dir, PATH_MAX-1);
         strncat(tmp, "/", PATH_MAX-1);
         strncat(tmp, dp.cFileName, PATH_MAX-1);
         _rmdir_(tmp);
      }
      strncpy(tmp, dir, PATH_MAX-1);
      strncat(tmp, "/", PATH_MAX-1);
      strncat(tmp, dp.cFileName, PATH_MAX-1);
      unlink(tmp);
   } while (FindNextFile(hFind, &dp));
   FindClose(hFind);
#endif
   return rmdir(dir);
}

/* remove PND's appdata */
static void removeappdata(pndman_package *pnd, _USR_DATA *data)
{
   pndman_application *a;
   pndman_device *d, *dev;
   char path[PATH_MAX];
   int ret;
   assert(pnd && data);

   /* user don't want this */
   if (!(data->flags & A_NOSAVE)) return;

   /* find device the PND is on */
   for (dev = NULL, d = data->dlist; d; d = d->next)
      if(!strcmp(d->mount, pnd->mount)) {
         dev = d;
         break;
      }

   /* remove appdata */
   for (a = pnd->app; a; a = a->next) {
      if (!a->appdata) continue;
      memset(path, 0, PATH_MAX);
      strncpy(path, dev->mount, PATH_MAX-1);
      strncat(path, "/pandora/appdata/", PATH_MAX-1);
      strncat(path, a->appdata, PATH_MAX-1);
      if ((ret = _rmdir_(path)) == 0) _printf(_REMOVED_APPDATA, path);
      else if (ret == -1)             _printf(_APPDATA_FAIL, path);
   }
}

/* package callback for commiting */
static void packagecallback(pndman_curl_code code,
      pndman_package_handle *handle)
{
   _USR_DATA *data = (_USR_DATA*)handle->user_data;

   if (code == PNDMAN_CURL_PROGRESS) {
      if (handle->progress.done == 2) return; /* dirty hack */
      data->tdl += handle->progress.download;
      // data->ttdl += handle->progress.total_to_download;
      handle->progress.done = 2; /* we know this is already processed */
   }

   if (code != PNDMAN_CURL_FAIL &&
       code != PNDMAN_CURL_DONE) return;

   ERASE(); commithandle(data, handle);
   if (code == PNDMAN_CURL_FAIL) {
      data->ttdl -= handle->pnd->size;
   }

   /* we can free this already */
   pndman_package_handle_free(handle);
}

/* perform target's action */
static int targetperform(_USR_DATA *data)
{
   _USR_TARGET *t;
   unsigned int c = 0, count = 0;
   int ret;
   assert(data);

   data->ttdl = 0; data->tdl = 0;
   for (t = data->tlist; t; t = t->next) ++c;
   pndman_package_handle handle[c]; t = data->tlist; char start[c];
   for (c = 0; t; t = t->next) {
      pndman_package_handle_init(t->pnd->id?t->pnd->id:"notitle", &handle[c]);
      handle[c].pnd        = t->pnd;
      handle[c].callback   = packagecallback;
      handle[c].user_data  = data;
      handle[c].device     = ((data->flags & OP_UPGRADE) || (data->flags & A_UPGRADE))?data->dlist:data->root;
      handle[c].flags      = handleflagsfromflags(t->pnd, data->flags);
      start[c]             = 0;
      if (c<_QUEUE) {
         if (pndman_package_handle_perform(&handle[c]) != RETURN_OK)
            handle[c].flags = 0;
         start[c] = 1;
      }
      data->ttdl += t->pnd->size;
      ++c;
   }

   if (!(data->flags & GB_NOBAR)) showcursor(0);
   while ((ret = pndman_curl_process(0, 1000)) > 0 || count > 0) {
      /* check active transmissions */
      for (c = 0, count = 0, t = data->tlist; t; t = t->next) {
         if (!handle[c].flags) { ++c; continue; }              /* failed perform */
         if (handle[c].progress.done == 2) handle[c].progress.done = 0; /* clear our hack */
         if (!handle[c].progress.done && start[c]) ++count;    /* active transmissions */
         if (handle[c].progress.done && start[c]) data->tdl += handle[c].pnd->size;
         ++c;
      }

      /* show progressbar */
      if (!(data->flags & GB_NOBAR))
         progressbar(data->tdl<data->ttdl?data->tdl:data->ttdl, data->ttdl);
      data->tdl = 0;

      /* check if we should start new handle */
      for (c = 0, t = data->tlist; count < _QUEUE && t; t = t->next) {
         if (!handle[c].flags) { ++c; continue; }  /* failed perform */
         if (!start[c]) { /* ready to start new download */
            if (pndman_package_handle_perform(&handle[c]) != RETURN_OK)
               handle[c].flags = 0;
            start[c] = 1;
            ++count;
         }
         ++c;
      }
   }
   ERASE();
   if (!(data->flags & GB_NOBAR)) showcursor(1);
   if (ret == -1) _printf(_INTERNAL_CURL_FAIL);

   /* free handles */
   t = data->tlist;
   for (c = 0; t; t = t->next) {
      /* this is for non download operations */
      if ((data->flags & OP_REMOVE)) {
         pndman_package_crawl_single_package(1, handle[c].pnd);
         removeappdata(handle[c].pnd, data);
         commithandle(data, &handle[c]);
      }
      pndman_package_handle_free(&handle[c++]);
   }

   return ret!=-1?RETURN_OK:RETURN_FAIL;
}

/* check that we have remote repository */
static pndman_repository* checkremoterepo(const char *str, _USR_DATA *data)
{
   assert(str && data);
   if (data->rlist->next) return data->rlist->next;
   else _printf(_REPO_WONT_DO_ANYTHING, str);
   return NULL;
}

/* check tha we have synchorized repository */
static int checksyncedrepo(pndman_repository *r)
{
   assert(r);
   for (; r && !r->timestamp; r = r->next);
   if (!r) _printf(_NO_SYNCED_REPO);
   else return RETURN_TRUE;
   return RETURN_FALSE;
}

/* yaourt operation logic */
static int yaourtprocess(_USR_DATA *data)
{
   pndman_repository *rs;

   /* check that we aren't using local repository */
   if (!(rs = checkremoterepo("sync", data)))
      return RETURN_FAIL;

   /* we need synced repos */
   if (!checksyncedrepo(rs)) return RETURN_FAIL;

   /* set PND's for targets */
   if (!targetpnd(rs, data, 0)) return RETURN_FAIL;

   /* show yaourt dialog and get targets from user */
   if (!yaourtdialog(data)) return RETURN_FAIL;

   /* operation dialog */
   if (!pre_op_dialog(data)) return RETURN_FAIL;

   /* perform action here */
   return targetperform(data);
}

/* sync operation logic */
static int syncprocess(_USR_DATA *data)
{
   pndman_repository *r, *rs;
   size_t longest_title;

   /* check that we aren't using local repository */
   if (!(rs = checkremoterepo("sync", data)))
      return RETURN_FAIL;

   /* repository syncing is always the first operation */
   if (data->flags & A_REFRESH) {
      if (syncrepos(rs, data) != RETURN_OK)
         return RETURN_FAIL;
      pndman_repository_check_updates(data->rlist);
      listupdate(data);
   }

   /* everything else needs synced repositories */
   if (!checksyncedrepo(rs)) return RETURN_FAIL;

   /* search */
   if (isquery(data->flags))  {
      longest_title = getlongtarget(data);
      for (r = rs; r && r->url; r = r->next)
         searchpnd(r, data, longest_title);
      return RETURN_OK;
   }

   /* upgrade without no targets given, add all targets that needs update */
   if ((data->flags & A_UPGRADE) && !data->tlist)
      targetup(rs, data);

   /* set PND's for targets here */
   if (!targetpnd(rs, data, (data->flags & A_UPGRADE))) return RETURN_FAIL;

   /* operation dialog */
   if (!pre_op_dialog(data)) return RETURN_FAIL;

   /* Actual sync here */
   return targetperform(data);
}

/* upgrade operation logic */
static int upgradeprocess(_USR_DATA *data)
{
   pndman_repository *rs;

   /* check that we aren't using local repository */
   if (!(rs = checkremoterepo("upgrade", data)))
      return RETURN_FAIL;

   /* we need synced repo as well */
   if (!checksyncedrepo(rs)) return RETURN_FAIL;

   /* handle targets */
   if (!data->tlist) targetup(rs, data);
   if (!targetpnd(rs, data, 1))  return RETURN_FAIL;

   /* operation dialog */
   if (!pre_op_dialog(data)) return RETURN_FAIL;

   /* do oepration */
   return targetperform(data);
}

/* remove operation logic */
static int removeprocess(_USR_DATA *data)
{
   /* search local repo from the targets */
   if (!targetpnd(data->rlist, data, 0)) return RETURN_FAIL;

   /* operation dialog */
   if (!pre_op_dialog(data)) return RETURN_FAIL;

   return targetperform(data);
}

/* query operation logic */
static int queryprocess(_USR_DATA *data)
{
   size_t longest_title;

   if (data->flags & A_QUERY_REPO)
      return searchrepo(data->rlist, data);

   longest_title = getlongtarget(data);
   return searchpnd(data->rlist, data, longest_title);
}

/* version compare, ret 0 on same, 1 on different */
static int vercmp(pndman_version *l, pndman_version *r)
{
   return !(!strcmp(l->major,   r->major)   &&
            !strcmp(l->minor,   r->minor)   &&
            !strcmp(l->release, r->release) &&
            !strcmp(l->build,   r->build)   &&
            l->type == r->type);
}

/* handle corrupt package */
static void handlecorrupt(pndman_package *p, pndman_package *rp,
      pndman_repository *r, int certain, _USR_DATA *data)
{
   (void)r;
   _USR_TARGET *t;

   /* this is here so we won't output warning
    * for same pnd again, (+ user action) */
   if (!(t = addtarget(p->id, &data->tlist)))
      return;

   t->pnd = (data->flags & OP_SYNC)?rp:p;
   _printf(certain==1?_PND_IS_CORRUPT:
           certain==0?_PND_MAY_CORRUPT:_PND_DIF_CORRUPT, p->id);
}

/* crawl operation logic */
static int crawlprocess(_USR_DATA *data)
{
   pndman_device *d;
   pndman_package *p, *pp, *pm;
   pndman_repository *r, *rs, *rm;
   int f = 0, ret = 0, state = 0;

   /* set operation flag if user wants to
    * handle corrupt packages somehow. */
   if (data->flags & A_INTEGRITY) {
      /* remove any target medias */
      data->flags = data->flags & ~A_MENU;
      data->flags = data->flags & ~A_DESKTOP;
      data->flags = data->flags & ~A_APPS;
      freetarget_all(data->tlist); data->tlist = NULL;
      if (data->flags & A_INST_CRPT)    data->flags |= OP_SYNC;
      else if (data->flags & A_RM_CRPT) data->flags |= OP_REMOVE;
   }

   /* crawl and count on success */
   for (d = data->dlist; d; d = d->next)
      if ((ret = pndman_package_crawl(0, d, data->rlist)) != RETURN_FAIL) f += ret;

   /* check integrity */
   if ((data->flags & A_INTEGRITY)) {
      /* check that we aren't using local repository */
      if ((rs = checkremoterepo("crawl", data))) {
         for (p = data->rlist->pnd; p; p = p->next) {
            if (!p->md5 || (data->flags & GB_FORCE))
               pndman_package_fill_md5(p);
            for (r = rs; r; r = r->next)
               for (pp = r->pnd; pp; pp = pp->next) {
                  if (strcmp(p->id, pp->id)) continue;
                  if (!strcmp(p->repository, r->name)    &&
                      !vercmp(&p->version, &pp->version) &&
                      p->modified_time == pp->modified_time &&
                      strcmp(p->md5, pp->md5)) {
                     /* defienatly corrupt package,
                      * unless same md5 sum is found from
                      * some other repo */
                     if (!pm  && !rm) {
                        pm = pp; rm = r; state = 1;
                     }
                  } else if (!vercmp(&p->version, &pp->version) &&
                              p->modified_time == pp->modified_time &&
                              strcmp(p->md5, pp->md5)) {
                     /* maybe corrupt package,
                      * unless same md5 sum is found from
                      * some other repo */
                     if (!pm && !rm) {
                        pm = pp; rm = r; state = 0;
                     }
                  } else if (!strcmp(p->md5, pp->md5)) {
                     /* we found remote pnd with same
                      * md5, this propably is not corrupt. */
                     if (state == -1 && pm &&
                        pm->modified_time < pp->modified_time) {
                        pm = NULL; rm = NULL;
                     }
                     state = 3;
                  } else {
                     /* remote pnd is propably older,
                      * but mark anyways and ignore unless,
                      * match is found */
                     if (!pm && !rm) {
                        pm = pp; rm = r;
                     }
                  }
               }
            if (pm && rm && state != -1)
               handlecorrupt(p, pm, rm, state, data);
            pm = NULL; rm = NULL; state = -1;
         }
      }
   }

   /* check updates and list them */
   pndman_repository_check_updates(data->rlist);
   listupdate(data);
   _printf(_PNDS_CRAWLED, f);

   /* do whatever user wants */
   if (data->tlist && (data->flags & A_INTEGRITY) &&
      ((data->flags & A_INST_CRPT) || (data->flags & A_RM_CRPT))) {
      NEWLINE();
      if (!pre_op_dialog(data)) return RETURN_FAIL;
      return targetperform(data);
   }
   return RETURN_OK;
}

/* clean operation logic */
static int cleanprocess(_USR_DATA *data)
{
   pndman_device *d;
   char tmp[PATH_MAX];
   FILE *f;

   memset(tmp, 0, PATH_MAX);
   for (d = data->dlist; d; d = d->next)
      if (d->appdata) {
         strncpy(tmp, d->appdata, PATH_MAX-1);
         _printf(_CLEANING_DB, tmp);
         strncat(tmp, "/local.db", PATH_MAX-1);
         if ((f = fopen(tmp,"r"))) {
            fclose(f);
            unlink(tmp);
         }
         strncpy(tmp, d->appdata, PATH_MAX-1);
         strncat(tmp, "/repo.db", PATH_MAX-1);
         if ((f = fopen(tmp,"r"))) {
            fclose(f);
            unlink(tmp);
         }
         _printf(_DEVICE_CLEANED, d->mount);
      }
   return RETURN_OK;
}

#ifndef _WIN32
/* check if fd is ready */
static int fdcheck(unsigned int fd)
{
   fd_set fdset;
   struct timeval tm;
   FD_ZERO(&fdset);
   FD_SET(fd, &fdset);
   tm.tv_sec   = 3;
   tm.tv_usec  = 0;
   return select(fd+1, &fdset, NULL, NULL, &tm)==1?1:0;
}

/* strip newline from stdin data */
static char* stripnl(char *s) { s[strlen(s)-1] = 0; return s; }
#endif

/* set repo credentials from CLI arguments or stdin */
static int setrepocredentials(_USR_DATA *data)
{
   _USR_TARGET *t;
   pndman_repository *r;
   unsigned int count = 0;
   char s[LINE_MAX], *repository = NULL, *username, *key;

   /* arguments given */
   if (data->tlist)
      for (t = data->tlist; t; t = t->next) ++count;
#ifndef _WIN32 /* POSIX only */
   else { /* read stdin */
      memset(s, 0, LINE_MAX);
      while (fdcheck(fileno(stdin)) && fgets(s, LINE_MAX-1, stdin))
      { addtarget(stripnl(s), &data->tlist); ++count; }
      if (count == 1) { _printf(_D"\1%s", s); goto fail; }
   }
#endif /* POSIX */

   if (count < 2) goto usage;
   username    = (t = data->tlist)->id;
   key         = (t = t->next)->id;
   if (count > 2) repository  = (t = t->next)->id;

   /* check remote repositories */
   if (!data->rlist->next)
      goto no_repos;

   /* show repository selection */
   if (count < 3) {
      r = repositorydialog(data);
   } else {
      /* check if repository is valid */
      for (r = data->rlist->next; r; r = r->next)
         if (!strcmp(repository, r->url)      ||
             !strcmp(repository, r->api.root) ||
             (r->name && !strcmp(repository, r->name)))
            break;

      /* try strstr instead */
      if (!r) {
         for (r = data->rlist->next; r; r = r->next)
            if (r->name && strstr(r->name, repository)) break;
      }
   }

   /* nothing found, fail */
   if (!r) goto repo_fail;

   /* set credentials */
   _printf(_REPO_API_KEY_ADDED);
   pndman_repository_set_credentials(r, username, key, 1 /* store */);
   repoinfo(r, data);
   return RETURN_OK;

no_repos:
   _printf(_NO_REMOTE_REPOS);
   goto fail;
repo_fail:
   _printf(_COULD_NOT_FIND_REPO, repository);
   goto fail;
usage:
   _printf(_REPO_API_KEY_ADD_USAGE);
fail:
   return RETURN_FAIL;
}

/* type ids for genericcb */
#define _ID_COMMENT  "comment"
#define _ID_CMNT_RM  "commentrm"
#define _ID_RATE_GET "rateget"
#define _ID_RATE_PUT "rateput"

/* generic callback */
static void repoapigenericcb(pndman_curl_code code, const char *info, void *data)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      _printf(_D"\1%s", info);
      return;
   }

   if      (!strcmp((char*)data, _ID_COMMENT))   _printf(_D"\2Comment sent!");
   else if (!strcmp((char*)data, _ID_CMNT_RM))   _printf(_D"\2Comment removed!");
}

/* rating callback */
static void repoapiratecb(pndman_curl_code code, pndman_api_rate_packet *packet)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      _printf(_D"\1%s", packet->error);
      return;
   }

   if (!strcmp((char*)packet->user_data, _ID_RATE_PUT)) {
      _printf(_D"\2Rating sent!");
      _printf(_D"\2New rating for \4%s\2 is \3%d", packet->pnd->id, packet->total_rating);
   } else {
      _printf("%d", packet->rating);
   }
}

/* comment package */
static int repoapiratecomment(_USR_DATA *data, const char *comment, unsigned int rate)
{
   _USR_TARGET *t;
   unsigned int count;

   if (comment && !strlen(comment)) {
      _printf(_REPO_API_NO_COMMENT);
      return RETURN_FAIL;
   }

   if (!(data->flags & A_RATE_GET)) {
      if (_QUIET < 2) {
         for (t = data->tlist, count = 0; t; t = t->next); ++count;
         _printf(_TARGET_LINE"\7", count);
         for (t = data->tlist; t; t = t->next) _printf("\4%s\5%s\7", t->pnd->id, t->next?", ":"");
         NEWLINE();

         if ((data->flags & A_COMMENT) && strlen(comment) > 300)
            _printf(_COMMENT_LENGTH_WARN);
      }
      if (data->flags & A_COMMENT) {
         NEWLINE();
         _printf("\1\"\5%s\1\"", comment);
         if (!yesno(data, _SEND_COMMENT)) return RETURN_FAIL;
      } else {
         NEWLINE();
         _printf("\3Rating: \5%d", rate);
         if (!yesno(data, _SEND_RATING))  return RETURN_FAIL;
      }
   }
   for (t = data->tlist; t; t = t->next) {
      if (data->flags & A_COMMENT)
         pndman_api_comment_pnd(_ID_COMMENT, t->pnd, comment, repoapigenericcb);
      else if (data->flags & A_RATE)
         pndman_api_rate_pnd(_ID_RATE_PUT, t->pnd, rate, repoapiratecb);
      else pndman_api_get_own_rate_pnd(_ID_RATE_GET, t->pnd, repoapiratecb);
   }
   while (pndman_curl_process(0, 1000) > 0);
   return RETURN_OK;
}

/* comment removal callback */
static void repoapicommentrmcb(pndman_curl_code code, pndman_api_comment_packet *p)
{
   _comment_rm_struct *pp = (_comment_rm_struct*)p->user_data;

    if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      _printf(_D"\1%s", p->error);
      return;
   }

   if (pp->needle && strstr(p->comment, pp->needle)) {
      _printf(_FOUND_COMMENT_MATCH, p->pnd->id);
      NEWLINE();
      _printf("\1\"\5%s\1\"", p->comment);
      if (yesno(pp->data, _DELETE_COMMENT)) {
         pndman_api_comment_pnd_delete(_ID_CMNT_RM, p->pnd, p->date, repoapigenericcb);
         pp->needle = NULL;
      }
   }
}

/* remove comment from package */
static int repoapicommentrm(_USR_DATA *data, const char *needle)
{
   _USR_TARGET *t;
   _comment_rm_struct pp;

   /* check needle */
   if (!needle)
      return RETURN_FAIL;

   pp.data   = data;
   pp.needle = needle;
   for (t = data->tlist; t; t = t->next) {
      pndman_api_comment_pnd_pull(&pp, t->pnd, repoapicommentrmcb);
      while (pndman_curl_process(0, 1000) > 0);
   }
   return RETURN_OK;
}

/* comment callback */
static void repoapicommentcb(pndman_curl_code code, pndman_api_comment_packet *p)
{
   _comment_data *d;
   _comment_pull_struct *s = (_comment_pull_struct*)p->user_data;

   if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      _printf(_D"\1%s", p->error);
      return;
   }

   if (!p->pnd)
      return;

   if (!s->comment) d = s->comment = malloc(sizeof(_comment_data));
   else {
      for (d = s->comment; d && d->next; d = d->next);
      d = d->next = malloc(sizeof(_comment_data));
   }

   if (!d) return;
   memset(d, 0, sizeof(_comment_data));
   d->username = strdup(p->username);
   d->comment  = strdup(p->comment);
}

/* process and free comment pull struct */
static void processcommentpull(_comment_pull_struct *s)
{
   _comment_data *d, *dn;
   size_t longest_user = 0, len;

   if (!s->comment) {
      _printf(_REPO_API_NO_COMMENTS);
      return;
   }

   /* get longest user */
   for (d = s->comment; d; d = d->next)
      if ((len = strlen(d->username)) > longest_user)
          longest_user = len;

   /* print */
   char *user = s->pnd->repositoryptr->api.username;
   for (d = s->comment; d; d = d->next) {
      _printf(_D"%s%s\5\7",(user&&!strcmp(user, d->username))?"\4":"\1", d->username);
      for (len = longest_user-strlen(d->username);
           len; --len)
         printf(" ");
      _printf(": %s", d->comment);
   }

   /* free */
   for (d = s->comment; d; d = dn) {
      free(d->username); free(d->comment);
      dn = d->next; free(d);
   }
}

/* get comments for package */
static int repoapicommentpull(_USR_DATA *data)
{
   _USR_TARGET *t;
   _comment_pull_struct s;
   for (t = data->tlist; t; t = t->next) {
      if (t->prev) NEWLINE();
      _printf(_COMMENTS_FOR_PACKAGE, t->pnd->id);
      memset(&s, 0, sizeof(_comment_pull_struct)); s.pnd = t->pnd;
      pndman_api_comment_pnd_pull(&s, t->pnd, repoapicommentcb);
      while (pndman_curl_process(0, 1000) > 0);
      processcommentpull(&s);
   }
   return RETURN_OK;
}

/* history callback */
static void repoapihistorycb(pndman_curl_code code, pndman_api_history_packet *p)
{
   if (code == PNDMAN_CURL_PROGRESS)
      return;

   if (code == PNDMAN_CURL_FAIL) {
      _printf(_D"\1%s", p->error);
      return;
   }

   if (!p->id) {
      _printf(_REPO_API_NO_HISTORY);
      return;
   }

   char *nao;
   /* pndman_repository *r = (pndman_repository*)user_data; */
   nao = gettime(p->download_date);
   _printf("\2%s \5(\4%s.%s.%s.%s\5)\n"
           "\1 └─\5%s", p->id,
         p->version->major, p->version->minor,
         p->version->release, p->version->build,
         nao?nao:"herp derp");
   if (nao) free(nao);
}

/* get download history */
static int repoapidlhistory(_USR_DATA *data)
{
   pndman_repository *r;
   for (r = data->rlist; r; r = r->next)
      pndman_api_download_history(r, r, repoapihistorycb);
   while (pndman_curl_process(0, 1000) > 0);
   return RETURN_OK;
}

/* repository api operation logic */
static int repoapiprocess(_USR_DATA *data)
{
   _USR_TARGET *t;
   pndman_repository *rs;
   char comment[LINE_MAX];
   int rate = 0;

   /* repo api handler register */
   memset(comment, 0, LINE_MAX);

   /* if we do action, do some sanity checks */
   if ((data->flags & A_RATE)          ||
       (data->flags & A_COMMENT)       ||
       (data->flags & A_RATE_GET)      ||
       (data->flags & A_DL_HISTORY)) {
      /* check that we aren't using local repository */
      if (!(rs = checkremoterepo("repository api", data)))
         return RETURN_FAIL;

      /* we need synced repo as well */
      if (!checksyncedrepo(rs)) return RETURN_FAIL;

      /* store comment */
      if ((data->flags & A_COMMENT) ||
          (data->flags & A_RATE)    ||
          (data->flags & A_RATE_GET)) {

         if (((data->flags & A_COMMENT)      &&
             !(data->flags & A_RATE))        ||
             ((data->flags & A_RATE)         &&
             !(data->flags & A_COMMENT))) {
            for (t = data->tlist; t && t->next; t = t->next);
            if (t && t->prev) {
               if (data->flags & A_COMMENT)  strncpy(comment, t->id, LINE_MAX-1);
               if (data->flags & A_RATE)     rate = strtol(t->id, (char**) NULL, 10);
               data->tlist = freetarget(t);
            }
            if ((data->flags & A_RATE) && (!data->tlist || rate < 1 || rate > 5)) goto no_rating;
         }

         /* handle targets */
         if (!data->tlist) goto no_targets;
         if (!targetpnd(rs, data, 0))
            return RETURN_FAIL;
      }
   }

   /* comment on package or pull comments */
   if (data->flags & A_COMMENT &&
       data->flags & A_RATE) { /* -cp */
      return repoapicommentpull(data);
   } else if (data->flags & A_COMMENT &&
              data->flags & A_DL_HISTORY) { /* -cd */
      return repoapicommentrm(data, comment);
   } else if (data->flags & A_COMMENT) {
      return repoapiratecomment(data, comment, 0);
   } else if (data->flags & A_RATE) {
      return repoapiratecomment(data, NULL, rate);
   } else if (data->flags & A_RATE_GET) {
      return repoapiratecomment(data, NULL, 0);
   } else if (data->flags & A_DL_HISTORY)
      return repoapidlhistory(data);

   /* set repo credentials, if called without operation arguments */
   return setrepocredentials(data);

no_rating:
   _printf(_REPO_API_FORGOT_RATING);
   goto fail;
no_targets:
   _printf(_REPO_API_NO_PACKAGES);
fail:
   return RETURN_FAIL;
}

/* version operation logic */
static int version(_USR_DATA *data)
{
   (void)data;
printf(
" ____                 _                       \n"
"|  _ \\ __ _ _ __   __| | ___  _ __ __ _      \n"
"| |_) / _` | '_ \\ / _` |/ _ \\| '__/ _` |    \n"
"|  __/ (_| | | | | (_| | (_) | | | (_| |      \n"
"|_|   \\__,_|_| |_|\\__,_|\\___/|_|  \\__,_|    "
);
puts("");
printf(
" ____      _     _      _   _          \n"
"|  _ \\ ___| |__ (_)_ __| |_| |__      \n"
"| |_) / _ \\ '_ \\| | '__| __| '_ \\   \n"
"|  _ <  __/ |_) | | |  | |_| | | |     \n"
"|_| \\_\\___|_.__/|_|_|   \\__|_| |_|    "
);
puts("\n");

   _printf("\4libpndman && milkyhelper\n");
   _printf("\3https://github.com/Cloudef/libpndman");
   _printf("\5~ %s", pndman_git_head());
   _printf("\5~ %s\n", pndman_git_commit());
   _printf("\5~ \1Cloudef");
   _printf("\4<o/ \2DISCO!");
   return RETURN_OK;
}

/* help operation logic */
static int help(_USR_DATA *data)
{
   unsigned int i;
   printf("usage: %s [-%s] ", data->bin, _G_ARG);
   for (i = 0; i != strlen(_OP_ARG); ++i) {
      printf("[-%c%s] ", _OP_ARG[i],
            _OP_ARG[i]=='S'?_S_ARG:
            _OP_ARG[i]=='R'?_R_ARG:
            _OP_ARG[i]=='Q'?_Q_ARG:
            _OP_ARG[i]=='P'?_P_ARG:
            _OP_ARG[i]=='A'?_A_ARG:"");
   }
   NEWLINE();

   data->flags &= ~OP_HELP;
   if (!hasop(data->flags)) {
      NEWLINE();
      _printf("\2~ Global arguments:");
      _printf("\5  -v : Verbose mode, combine to increase verbose level.");
      _printf("\5  -q : Quietness, combine to make application more silent.");
      _printf("\5  -t : Plain text mode, do not use colors.");
      _printf("\5  -r : Root device/directory.\n%s\n%s",
              "       All operations are going to be done under this device/directory.",
              "       You can leave argument empty, if you want to choose device from a list");

      NEWLINE();
      _printf("\2~ Operation arguments:");
      _printf("\5  -S : Sync/Search/Install operation.");
      _printf("\5  -U : Upgrade operation.");
      _printf("\5  -R : Removal operation.");
      _printf("\5  -Q : Local query operation.");
      _printf("\5  -A : Repository API access.");
      _printf("\5  -P : Crawls PND's from media.");
      _printf("\5  -C : Cleans all database files.");
      _printf("\5  -E : Edit configuration with $EDITOR.");
      _printf("\5  -V : Show version information.");
      _printf("\5  -h : Show help. Combine with operation to get more help.");

      NEWLINE();
      _printf("\2~ Other arguments:");
      _printf("\5  --root      : Alias for -r option.");
      _printf("\5  --help      : Alias for -h option.");
      _printf("\5  --edit      : Alias for -E option.");
      _printf("\5  --version   : Alias for -V option.");
      _printf("\5  --noconfirm : Don't prompt questions.");
      _printf("\5  --nomerge   : Do full repository synchorization.");
      _printf("\5  --needed    : Don't reinstall up-to-date PND's.");
      _printf("\5  --nobar     : Don't show progress bar.");
      _printf("\5  --all       : Target everything.");
   } else if ((data->flags & OP_SYNC)) {
      NEWLINE();
      _printf("\2~ Sync arguments:");
      _printf("\5  -f : Force install even if MD5 checking fails.");
      _printf("\5  -s : Search remote repositories, by matching id/title/description.");
      _printf("\5  -c : Search remote repositories, by matching category.");
      _printf("\5  -i : Show full information of matching PND.");
      _printf("\5  -l : List all remote PND's.");
      _printf("\5  -y : Synchorize remote repository with local database.");
      _printf("\5  -u : Upgrade PND's / Filter by upgrade status.");
      _printf("\5  -m : Use /menu as install target.");
      _printf("\5  -d : Use /desktop as install target.");
      _printf("\5  -a : Use /apps as install target.");
      _printf("\5  -b : Make backup of original file.");
   } else if ((data->flags & OP_REMOVE)) {
      NEWLINE();
      _printf("\2~ Removal arguments:");
      _printf("\5  -n : Remove also appdata.");
   } else if ((data->flags & OP_QUERY)) {
      NEWLINE();
      _printf("\2~ Query arguments:");
      _printf("\5  -s : Search local database, by matching id/title/description.");
      _printf("\5  -c : Search local database, by matching category.");
      _printf("\5  -i : Show full information of matching PND.");
      _printf("\5  -l : List contents of PND's.");
      _printf("\5  -u : Filter query with upgrade status.");
      _printf("\5  -d : List repositories.");
   } else if ((data->flags & OP_UPGRADE)) {
      NEWLINE();
      _printf("\2~ Upgrade operation:");
      _printf("\5  This operation is just a alias for -Su.");
   } else if ((data->flags & OP_CRAWL)) {
      NEWLINE();
      _printf("\2~ Crawl arguments:");
      _printf("\5  -c : Run integrity check.");
      _printf("\5  -f : Regenerate MD5 sums for every crawled PND. (combine with -c)");
      _printf("\5  -s : Reinstall corrupt packages.                (combine with -c)");
      _printf("\5  -d : Remove corrupt packages.                   (combine with -c)");
   } else if ((data->flags & OP_REPO_API)) {
      _printf("\2~ Repository API arguments:");
      _printf("\5  If you specify -A only, you can add your credentials to libpndman.");
      _printf("\5  Package specific functions are executed on each repository that has the package.");
      _printf("\5  -A : <username> <api key> [repo name/url] syntax to add credentials.");
      _printf("\5  -c : Send new comment to packages.  <packages> <comment>");
      _printf("\5  -cp: Get comments for packages.     <packages>");
      _printf("\5  -cd: Delete comment from packages.  <packages> <needle>");
      _printf("\5  -p : Rate package.                         (1-5 rating)");
      _printf("\5  -g : Get own rating for package.");
      _printf("\5  -d : Get download history.");
   } else {
      _printf("\1This operation has no arguments.");
   }

   return RETURN_OK;
}

static int edit(_USR_DATA *data)
{
   (void)data;
   char path[PATH_MAX];
   memset(path, 0, sizeof(path));
   if (getconfigpath(path) != RETURN_OK) return RETURN_FAIL;
   char *editor = getenv("EDITOR");
   if (!editor) editor = "vim";
   execlp(editor, editor, path, NULL);
   return RETURN_OK;
}

/*  _____ _        _    ____ ____
 * |  ___| |      / \  / ___/ ___|
 * | |_  | |     / _ \| |  _\___ \
 * |  _| | |___ / ___ \ |_| |___) |
 * |_|   |_____/_/   \_\____|____/
 */

/* sanity check the user data before proceeding to logic flow */
static int sanitycheck(_USR_DATA *data)
{
   /* expections to sanity checks */
   if ((data->flags & OP_VERSION) || (data->flags & OP_HELP))
      return RETURN_OK;

   /* no root, ask for it */
   if (!data->no_action && !data->root)
      if (rootdialog(data) != RETURN_OK) {
         NEWLINE(); _printf(_BAD_DEVICE_SELECTED);
         return RETURN_FAIL;
      }

   /* save root */
   if (saveroot(data) != RETURN_OK)
      _printf(_ROOT_FAIL);

   /* check target list, NOTE: OP_CLEAN && OP_QUERY && OP_REPO_API can perform without targets */
   if (!data->tlist && needtarget(data->flags)) {
      if (!data->no_action) _printf(_NO_X_SPECIFIED, "targets");
      else                  _printf(data->no_action);
      return RETURN_FAIL;
   } else if (!hasop(data->flags)) data->flags |= OP_YAOURT | OP_SYNC;

   /* check flags */
   if (!data->flags || !hasop(data->flags)) {
      if (!data->no_action) _printf(_NO_X_SPECIFIED, "operation");
      else                  _printf(data->no_action);
      return RETURN_FAIL;
   }

   return RETURN_OK;
}

/* process all flags that were parsed from program arguments */
static int processflags(_USR_DATA *data)
{
   _USR_TARGET       *t;
   pndman_device     *d;
   pndman_repository *r;
   pndman_package    *p;
   int ret = RETURN_FAIL;
   assert(data);

   /* set pndman's verbose level */
   pndman_set_verbose(_VERBOSE);

   /* sanity check */
   if (sanitycheck(data) != RETURN_OK)
      return RETURN_FAIL;

   if (_VERBOSE >= 1) {
      NEWLINE();
      if ((data->flags & OP_YAOURT))    _printf("\3::YAOURT");
      if ((data->flags & OP_SYNC))      _printf("\3::SYNC");
      if ((data->flags & OP_UPGRADE))   _printf("\3::UPGRADE");
      if ((data->flags & OP_REMOVE))    _printf("\3::REMOVE");
      if ((data->flags & OP_QUERY))     _printf("\3::QUERY");
      if ((data->flags & OP_REPO_API))  _printf("\3::REPO_API");
      if ((data->flags & OP_CRAWL))     _printf("\3::CRAWL");
      if ((data->flags & OP_CLEAN))     _printf("\3::CLEAN");
      if ((data->flags & OP_EDIT))      _printf("\3::EDIT");
      if ((data->flags & OP_VERSION))   _printf("\3::VERSION");
      if ((data->flags & OP_HELP))      _printf("\3::HELP");
      if ((data->flags & GB_FORCE))     _printf("\4;;FORCE");
      if ((data->flags & GB_NOCONFIRM)) _printf("\4;;NOCONFIRM");
      if ((data->flags & GB_NOMERGE))   _printf("\4;;NOMERGE");
      if ((data->flags & GB_NEEDED))    _printf("\4;;NEEDED");
      if ((data->flags & GB_NOBAR))     _printf("\4;;NOBAR");
      if ((data->flags & A_SEARCH))     _printf("\2->SEARCH");
      if ((data->flags & A_CATEGORY))   _printf("\2->CATEGORY");
      if ((data->flags & A_INFO))       _printf("\2->INFO");
      if ((data->flags & A_LIST))       _printf("\2->LIST");
      if ((data->flags & A_REFRESH))    _printf("\2->REFRESH");
      if ((data->flags & A_UPGRADE))    _printf("\2->UPGRADE");
      if ((data->flags & A_NOSAVE))     _printf("\2->NOSAVE");
      if ((data->flags & A_BACKUP))     _printf("\2->BACKUP");
      if ((data->flags & A_ALL))        _printf("\2->ALL");
      if ((data->flags & A_INTEGRITY))  _printf("\2->INTEGRITY");
      if ((data->flags & A_INST_CRPT))  _printf("\2->INST_CRPT");
      if ((data->flags & A_RM_CRPT))    _printf("\2->RM_CRPT");
      if ((data->flags & A_QUERY_REPO)) _printf("\2->QUERY_REPO");
      if ((data->flags & A_COMMENT))    _printf("\2->COMMENT");
      if ((data->flags & A_RATE))       _printf("\2->RATE");
      if ((data->flags & A_RATE_GET))   _printf("\2->RATE_GET");
      if ((data->flags & A_DL_HISTORY)) _printf("\2->HISTORY");
      if ((data->flags & A_MENU))       _printf("\1[MENU]");
      if ((data->flags & A_DESKTOP))    _printf("\1[DESKTOP]");
      if ((data->flags & A_APPS))       _printf("\1[APPS]");
      NEWLINE();

      _printf("\1LANG: %s", data->syslc);
      if (data->root)  _printf("\3Root: %s", data->root->mount);
      if (data->rlist) _printf("\n\1Repositories:");
      for (r = data->rlist; r; r = r->next) puts(r->name ? r->name : r->url);
      if (data->tlist) _printf("\n\4Targets:");
      for (t = data->tlist; t; t = t->next) puts(t->id);
      NEWLINE();
   }

   /* read repository information from each device */
   for (d = data->dlist; d; d = d->next)
      for (r = data->rlist; r; r = r->next) pndman_device_read_repository(r, d);
   pndman_repository_check_local(data->rlist);     /* check for removed/bad pnds */
   pndman_repository_check_updates(data->rlist);   /* check for updates */

   /* targetting everything */
   if ((data->flags & A_ALL) && data->rlist->next) {
      for (r = data->rlist->next; r; r = r->next)
         for (p = r->pnd; p; p = p->next)
            addtarget(p->id, &data->tlist);

      if (!data->tlist) _printf(_NO_X_SPECIFIED, "targets");
   }

   /* logic */
   if ((data->flags & OP_VERSION))        ret = version(data);
   else if ((data->flags & OP_HELP))      ret = help(data);
   else if ((data->flags & OP_EDIT))      ret = edit(data);
   else if ((data->flags & OP_YAOURT))    ret = yaourtprocess(data);
   else if ((data->flags & OP_SYNC))      ret = syncprocess(data);
   else if ((data->flags & OP_UPGRADE))   ret = upgradeprocess(data);
   else if ((data->flags & OP_REMOVE))    ret = removeprocess(data);
   else if ((data->flags & OP_QUERY))     ret = queryprocess(data);
   else if ((data->flags & OP_REPO_API))  ret = repoapiprocess(data);
   else if ((data->flags & OP_CRAWL))     ret = crawlprocess(data);
   else if ((data->flags & OP_CLEAN))     ret = cleanprocess(data);

   /* commit everything to disk */
   if (!(data->flags & OP_CLEAN) && data->dlist) {
      if (pndman_repository_commit_all(data->rlist, data->root) != RETURN_OK)
         _printf(_COMMIT_FAIL, data->root->mount);
   }

   return ret;
}

/*  ____ ___ ____ ___ _   _ _____
 * / ___|_ _/ ___|_ _| \ | |_   _|
 * \___ \| | |  _ | ||  \| | | |
 *  ___) | | |_| || || |\  | | |
 * |____/___\____|___|_| \_| |_|
 */

/* we cannot free resources here cause the important data is,
 * not on global level, however we can remove lock file here in future :)
 * (lockfile should be also part of libpndman!?) */
static void sigint(int sig)
{
   _printf("\1Caught %s, exiting...",
         sig == SIGINT  ? "SIGINT"  :
         sig == SIGTERM ? "SIGTERM" : "SIGSEGV");
#ifdef _WIN32
   _fcloseall();
#endif
   exit(sig);
}

/*  __  __    _    ___ _   _
 * |  \/  |  / \  |_ _| \ | |
 * | |\/| | / _ \  | ||  \| |
 * | |  | |/ ___ \ | || |\  |
 * |_|  |_/_/   \_\___|_| \_|
 */

/* try to guess what this does */
int main(int argc, char **argv)
{
   int ret;
   _USR_DATA data;
   char path[PATH_MAX];

   /* init path */
   memset(path, 0, PATH_MAX);

   /* roll cursor back */
   memset(_ERASE, ' ', sizeof(_ERASE)-1);
   _ERASE[sizeof(_ERASE)-2] = '\r';
   _ERASE[sizeof(_ERASE)-1] = 0;

   /* setup signals */
   (void)signal(SIGINT,  sigint);
   (void)signal(SIGTERM, sigint);
   (void)signal(SIGSEGV, sigint);

   /* by default, we fail :) */
   ret = EXIT_FAILURE;

   /* init userdata */
   init_usrdata(&data);
   data.bin = argv[0];

   /* detect devices */
   data.dlist = pndman_device_detect(NULL);

   /* get local repository */
   if (!(data.rlist = pndman_repository_init()))
      _printf(_FAILED_TO_REPOS);

   if (data.rlist) {
      /* read configuration */
      if ((ret = getconfigpath(path)) == RETURN_OK) ret = readconfig(path, &data);
      if (ret != RETURN_OK) _printf(_CONFIG_READ_FAIL, path);

      /* read root */
      data.root = getroot(&data);

      /* parse arguments */
      parseargs(argc, argv, &data);
   }

   /* OP_HELP && OP_VERSION are special */
   if (data.flags & OP_HELP || data.flags & OP_VERSION) {
      ret = processflags(&data) == RETURN_OK ? EXIT_SUCCESS : EXIT_FAILURE;
   } else {
      /* we have a failure now */
      if (!data.dlist) _printf(_FAILED_TO_DEVICES);

      /* do logic, if everything ok! */
      if (data.dlist && data.rlist) {
         ret = processflags(&data) == RETURN_OK ? EXIT_SUCCESS : EXIT_FAILURE;
      }
   }

   /* free repositories && devices */
   pndman_repository_free_all(data.rlist);
   pndman_device_free_all(data.dlist);

   /* free targets && ignores */
   freetarget_all(data.tlist);
   freeignore_all(data.ilist);
   return ret;
}

/* vim: set ts=8 sw=3 tw=0 :*/
