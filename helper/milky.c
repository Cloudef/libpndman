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

/* use colors? */
char _USE_COLORS  = 1;

/* verbose level? */
char _VERBOSE     = 0;

/* max concurrent downloads? */
char _QUEUE       = 5;

typedef enum _RETURN_STATUS
{
   RETURN_FAIL    = -1,
   RETURN_OK      =  0,
   RETURN_TRUE    =  1,
   RETURN_FALSE   = !RETURN_TRUE
} _RETURN_STATUS;

#define PND_ID 256
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
   unsigned int      flags;
   pndman_device     *dlist;
   pndman_device     *root;
   pndman_repository *rlist;
   _USR_TARGET       *tlist;
   _USR_IGNORE       *ilist;
   char *no_action;
} _USR_DATA;

/* initialize _USR_DATA struct */
static void init_usrdata(_USR_DATA *data)
{
   assert(data);
   data->flags = 0;
   data->dlist = NULL; data->root  = NULL;
   data->rlist = NULL; data->tlist = NULL;
   data->ilist = NULL; data->bin   = "milky";
   data->no_action = NULL;
}

/*  ____ _____ ____  ___ _   _  ____ ____
 * / ___|_   _|  _ \|_ _| \ | |/ ___/ ___|
 * \___ \ | | | |_) || ||  \| | |  _\___ \
 *  ___) || | |  _ < | || |\  | |_| |___) |
 * |____/ |_| |_| \_\___|_| \_|\____|____/
 */

/* all strings expect HELP and VERSION strings are listed here
 * (* strings that are not considered in final product, aren't here either)
 * maybe in future: custom printf function that handles color escapes
 *                  and resets the colors at the end automatically.
 *                  with this we could get rid of all those _R(); _N();
 *                  etc.. color functions. Plus it would allow colors to
 *                  be customized more easily! */
#define _DEVICE_CLEANED          "Cleaned libpndman database from %s!\n"
#define _PNDS_CRAWLED            "%d PND's were succesfully crawled.\n"
#define _REPO_WONT_DO_ANYTHING   "There is only local repository available, %s operations won't do anything.\n"
#define _NO_SYNCED_REPO          "None of repositories is synced. Sync the repositories first.\n"
#define _REPO_SYNCED             "%s, synchorized succesfully.\n"
#define _REPO_SYNCED_NOT         "%s, failed to synchorize.\n"
#define _PND_NOT_FOUND           "Warning: no such package %s\n"
#define _PND_NO_UPDATE           "Warning: %s, has no update.\n"
#define _PND_HAS_UPDATE          "%s has a update.\n"
#define _UPGRADE_DONE            "Upgraded %s\n"
#define _UPGRADE_FAIL            "Failed to upgrade %s\n"
#define _INSTALL_DONE            "Installed %s\n"
#define _INSTALL_FAIL            "Failed to install %s\n"
#define _REMOVE_DONE             "Removed %s\n"
#define _REMOVE_FAIL             "Failed to remove %s\n"
#define _CLEANING_DB             "Cleaning database files: %s\n"
#define _UNKNOWN_OPERATION       "unknown operation: %s"
#define _NO_X_SPECIFIED          "No %s specified.\n"
#define _BAD_DEVICE_SELECTED     "Bad device selected, exiting..\n"
#define _PND_IGNORED_FORCE       "Package %s is being ignored. Do you want to force?"
#define _PND_REINSTALL           "Package %s is already installed, reinstall?"
#define _NO_ROOT_YET             "Sir! You haven't selected your mount where to store PND's yet.\n"
#define _WARNING_SKIPPING        "Warning: skipping %s\n"
#define _WARNING_UPDATE          "Warning: %s is up-to-date, skipping.\n"
#define _NOTHING_TO_DO           "There is nothing to do"
#define _FAILED_TO_DEVICES       "Failed to detect devices.\n"
#define _FAILED_TO_REPOS         "Failed to initialize local repository..\n"
#define _COMMIT_FAIL             "Warning: failed to commit repositories to %s\n"
#define _ROOT_FAIL               "Warning: failed to store your current root device.\n"
#define _ROOT_SET_FAIL           "Failed to set root."
#define _ROOT_SET                "Root was set."
#define _MKCONFIG                "Creating default configuration file in %s\n"
#define _CONFIG_READ_FAIL        "Warning: failed to read configuration from %s\n"
#define _YAOURT_DIALOG           "Enter number of packages to be installed (ex: 1 2 3)"
#define _TARGET_MEDIA            "Target media "
#define _TARGET_PATH             "Target path  "
#define _DOWNLOAD_SIZE           "Download size"
#define _REMOVE_SIZE             "Removal size "
#define _CONTINUE_INSTALL        "Continue install?"
#define _CONTINUE_UPGRADE        "Continue upgrade?"
#define _CONTINUE_UNINSTALL      "Continue uninstall?"
#define _TARGET_LINE             "Targets"
#define _UNIT_KIB                "KiB"
#define _UNIT_MIB                "MiB"
#define _UNIT_BYTE               "B"
#define _INPUT_LINE              "> "
#define _YESNO                   "[Y/n]"
#define _NOYES                   "[y/N]"

/* other customizable */
#define NEWLINE()                puts("");

/*   ____ ___  _     ___  ____  ____
 *  / ___/ _ \| |   / _ \|  _ \/ ___|
 * | |  | | | | |  | | | | |_) \___ \
 * | |__| |_| | |__| |_| |  _ < ___) |
 *  \____\___/|_____\___/|_| \_\____/
 */

/* red */
static void _R(void)
{
   if (!_USE_COLORS) return;
#ifndef __WIN32__
   printf("\33[31m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_INTENSITY);
#endif
}

/* green */
static void _G(void)
{
   if (!_USE_COLORS) return;
#ifndef __WIN32__
   printf("\33[32m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN
   |FOREGROUND_INTENSITY);
#endif
}

/* yellow */
static void _Y(void)
{
   if (!_USE_COLORS) return;
#ifndef __WIN32__
   printf("\33[33m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN
   |FOREGROUND_RED|FOREGROUND_INTENSITY);
#endif
}

/* blue */
static void _B(void)
{
   if (!_USE_COLORS) return;
#ifndef __WIN32__
   printf("\33[34m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_BLUE
   |FOREGROUND_GREEN|FOREGROUND_INTENSITY);
#endif
}

/* white */
static void _W(void)
{
   if (!_USE_COLORS) return;
#ifndef __WIN32__
   printf("\33[37m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_GREEN|FOREGROUND_BLUE);
#endif
}

/* normal */
static void _N(void)
{
   if (!_USE_COLORS) return;
#ifndef __WIN32__
   printf("\33[0m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_GREEN|FOREGROUND_BLUE);
#endif
   fflush(stdout); /* make sure _N(); is flushed, prevent
                    * bad colors in terminal if crash occurs */
}

/*  _   _ _____ ___ _
 * | | | |_   _|_ _| |
 * | | | | | |  | || |
 * | |_| | | |  | || |___
 *  \___/  |_| |___|_____|
 */

/* trim string */
static size_t strtrim(char *str)
{
   size_t len;
   char *end, *pch = str;

   /* check if we need trimming */
   if (!str || *str == '\0') return 0;

   /* trim from left */
   while (isspace((unsigned char)*pch)) ++pch;
   if (pch != str)
      if ((len = strlen(pch))) memmove(str, pch, len+1);
      else *str = '\0';

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

/* \brief strncmp strings in uppercase, NOTE: returns 0 on found else 1 (so you don't mess up with strcmp) */
static int strnupcmp(const char *hay, const char *needle, size_t len)
{
   size_t i;
   for (i = 0; i != len; ++i)
      if (hay[i] != needle[i]) return RETURN_TRUE;
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

/* strip leading slash from path */
static void stripslash(char *path)
{
   if (path[strlen(path)-1] == '/' || path[strlen(path)-1] == '\\')
      path[strlen(path)-1] = '\0';
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

/* mkdir if it doesn't exist */
static int mkdirexist(const char *path)
{
   if (access(path, F_OK) != 0) {
      if (errno == EACCES) return RETURN_FAIL;
#ifdef __WIN32__
      if (mkdir(path) == -1) return RETURN_FAIL;
#else  /* _WIN32_ */
      if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
         return RETURN_FAIL;
#endif /* POSIX */
   }
   return RETURN_OK;
}

/* get $XDG_CONFIG_HOME/$CFG_DIR path, (create if doesn't exist) */
static int getcfgpath(char *path)
{
/* just dump to cwd on porndora and win32 (on pandora stuff appears in appdata) */
#if !defined(PANDORA) && !defined(__WIN32__)
   const char* xdg_home;
   struct passwd *pw;
   if (!(xdg_home = getenv("XDG_CONFIG_HOME"))) {
      if (!(xdg_home = getenv("HOME")))
         if ((pw = getpwuid(getuid())))
            xdg_home = pw->pw_dir;
         else return RETURN_FAIL;
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
#endif /* !PANDORA && !__WIN32__ */
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
   return data->root = setroot(dev, data->dlist);
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

   if (!strlen(id)) return NULL;
   if (*list) {
      for (t = *list; t; t = t->next) if (!strcmp(t->id, id) ||
                                          !strcmp(t->pnd?t->pnd->id:t->id, id)) return NULL;
      for (t = *list; t && t->next; t = t->next);
      t->next = new = malloc(sizeof(_USR_TARGET));
   } else new = malloc(sizeof(_USR_TARGET));
   if (!new) return NULL;

   if (*list) new->prev = t;
   else     { new->prev = NULL; *list = new; }

   memset(new->id, 0, PND_ID);
   strncpy(new->id, id, PND_ID-1);
   new->pnd  = NULL;
   new->next = NULL;
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

   if (!strlen(id)) return NULL;
   if (*list) {
      for (t = *list; t; t = t->next) if (!strcmp(t->id, id)) return NULL;
      for (t = *list; t && t->next; t = t->next);
      t->next = new = malloc(sizeof(_USR_IGNORE));
   } else new = malloc(sizeof(_USR_IGNORE));
   if (!new) return NULL;

   if (*list) new->prev = t;
   else     { new->prev = NULL; *list = new; }

   memset(new->id, 0, PND_ID);
   strncpy(new->id, id, PND_ID-1);
   new->next = NULL;
   return new;
}

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
   _USR_TARGET *t;
   _USR_IGNORE *i;

   for (i = data->ilist; i; i = i->next)
      for (t = data->tlist; t; t = t->next)
         if (!strcmp(i->id, t->id) ||
             !strcmp(i->id, t->pnd?t->pnd->id:t->id)) return RETURN_TRUE;
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
static const char* _G_ARG  = "vftr";         /* global arguments */
static const char* _OP_ARG = "hSURQPCV";     /* operations */
static const char* _S_ARG  = "scilyumdab";   /* sync operation arguments */
static const char* _R_ARG  = "n";            /* remove operation arguments */
static const char* _Q_ARG  = "sciu";         /* query operation arguments */

/* milkyhelper's operation flags */
typedef enum _HELPER_FLAGS
{
   GB_FORCE    = 0x0000001, OP_YAOURT    = 0x0000002,
   OP_SYNC     = 0x0000004, OP_UPGRADE   = 0x0000008,
   OP_REMOVE   = 0x0000010, OP_QUERY     = 0x0000020,
   OP_CLEAN    = 0x0000040, OP_VERSION   = 0x0000080,
   OP_HELP     = 0x0000100, OP_CRAWL     = 0x0000200,
   A_SEARCH    = 0x0000400, A_CATEGORY   = 0x0000800,
   A_INFO      = 0x0001000, A_LIST       = 0x0002000,
   A_REFRESH   = 0x0004000, A_UPGRADE    = 0x0008000,
   A_MENU      = 0x0010000, A_DESKTOP    = 0x0020000,
   A_APPS      = 0x0040000, A_NOSAVE     = 0x0080000,
   GB_NOMERGE  = 0x0100000, GB_NOCONFIRM = 0x0200000,
   GB_NEEDED   = 0x0400000, GB_NOBAR     = 0x0800000,
   A_BACKUP    = 0x1000000,
} _HELPER_FLAGS;
typedef _HELPER_FLAGS (*_PARSE_FUNC)(char, char*, int*, _USR_DATA*); /* function prototype for parsing flags */

/* do we have operation? */
static int hasop(unsigned int flags)
{
   return ((flags & OP_YAOURT) || (flags & OP_SYNC)    ||
           (flags & OP_REMOVE) || (flags & OP_UPGRADE) ||
           (flags & OP_CRAWL)  || (flags & OP_CLEAN)   ||
           (flags & OP_QUERY)  || (flags & OP_VERSION));
}

/* are we querying information? */
static int isquery(unsigned int flags)
{
   return ((flags & OP_QUERY)   || (flags & A_SEARCH) ||
           (flags & A_CATEGORY) || (flags & A_LIST)   ||
           (flags & A_INFO));
}

/* do we need targets for our operation? */
static int needtarget(unsigned int flags)
{
   return  (!(flags & OP_CLEAN)  && !isquery(flags)       &&
            !(flags & A_REFRESH) && !(flags & OP_UPGRADE) &&
            !(flags & A_UPGRADE) && !(flags & OP_CRAWL));
}

/* set destination */
static unsigned int setdest(_HELPER_FLAGS dest, _USR_DATA *data)
{
   data->flags  = data->flags & ~A_MENU;
   data->flags  = data->flags & ~A_DESKTOP;
   data->flags  = data->flags & ~A_APPS;
   return dest;
}

/* parse global flags */
static _HELPER_FLAGS getglob(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 'v')        ++_VERBOSE;
   else if (c == 'f')   return GB_FORCE;
   else if (c == 't')   _USE_COLORS = 0;
   else if (c == 'r')   _setroot(NULL, data);
   return 0;
}

/* parse operation flags */
static _HELPER_FLAGS getop(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 'S')        return OP_SYNC;
   else if (c == 'U')   return OP_UPGRADE;
   else if (c == 'R')   return OP_REMOVE;
   else if (c == 'Q')   return OP_QUERY;
   else if (c == 'P')   return OP_CRAWL;
   else if (c == 'C')   return OP_CLEAN;
   else if (c == 'V')   return OP_VERSION;
   else if (c == 'h')   return OP_HELP;
   return 0;
}

/* parse sync flags */
static _HELPER_FLAGS getsync(char c, char *arg, int *skipn, _USR_DATA *data)
{
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
   if (c == 'n') return A_NOSAVE;
   return 0;
}

/* parse query flags */
static _HELPER_FLAGS getquery(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 's')        return A_SEARCH;
   else if (c == 'c')   return A_CATEGORY;
   else if (c == 'i')   return A_INFO;
   else if (c == 'u')   return A_UPGRADE;
   return 0;
}

/* passes the information for correct flag parser function, and does extra handling if needed */
static void parse(_PARSE_FUNC func, const char *ref, char *arg, char *narg, int *skipn, _USR_DATA *data)
{
   int x, y;
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
   if (!strcmp(arg, "-r"))             _PASSARG(_setroot);     /* argument with argument */
   if (!strcmp(arg, "--root"))         _PASSARG(_setroot);     /* argument with argument */
   if (!strcmp(arg, "--help"))         _PASSFLG(OP_HELP);      /* another way of calling help */
   if (!strcmp(arg, "--version"))      _PASSFLG(OP_VERSION);   /* another way of calling version */
   if (!strcmp(arg, "--noconfirm"))    _PASSFLG(GB_NOCONFIRM); /* noconfirm option */
   if (!strcmp(arg, "--nomerge"))      _PASSFLG(GB_NOMERGE);   /* nomerge option */
   if (!strcmp(arg, "--needed"))       _PASSFLG(GB_NEEDED);    /* needed option */
   if (!strcmp(arg, "--nobar"))        _PASSFLG(GB_NOBAR);     /* nobar option */
   if (!strcmp(arg, "--nosave"))       _PASSFLG(A_NOSAVE);     /* nosave option */
   if (!strcmp(arg, "--backup"))       _PASSFLG(A_BACKUP);     /* backup option */
   if (arg[0] != '-')                  _PASSTHS(_addtarget);   /* not argument */
   parse(getglob, _G_ARG, arg, narg, skipn, data);
   if (!hasop(data->flags))         parse(getop, _OP_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_SYNC))     parse(getsync, _S_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_REMOVE))   parse(getremove, _R_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_QUERY))    parse(getquery, _Q_ARG, arg, narg, skipn, data);
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
   assert(data);
   if (!argc) return RETURN_FAIL;
   if (!pndman_device_add(argv[0], data->dlist))
      return RETURN_FAIL;
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

/* configuration wrapper for ignoring package */
static int _addignore(char **argv, int argc, _USR_DATA *data)
{
   int i;
   assert(data);
   for (i = 0; i != argc; ++i) addignore(argv[i], &data->ilist);
   return RETURN_OK;
}

/* configuration wrapper for setting nomerge option */
static int _nomerge(char **argv, int argc, _USR_DATA *data)
{
   assert(data);
   data->flags |= GB_NOMERGE;
   return RETURN_OK;
}

/* configuration wrapper for setting queue limit */
static int _queue(char **argv, int argc, _USR_DATA *data)
{
   assert(data);
   if (!argc) return RETURN_FAIL;
   if ((_QUEUE = strtol(argv[0], (char **) NULL, 10)) <= 0)
      _QUEUE = 5;
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
   { "nomerge",               _nomerge },
   { "queue",                 _queue },
   { "<this ends the list>",  NULL },
};

/* read arguments from key and pass to the function */
static int readconfig_arg(const char *key, _CONFIG_FUNC func, _USR_DATA *data, char *line)
{
   int i, p, argc, in_quote;
   char *argv[_CONFIG_MAX_VAR];

   /* reset args */
   for (i = 0; i != _CONFIG_MAX_VAR; ++i) {
      if (!(argv[i] = malloc(_CONFIG_ARG_LEN))) {
         for (; i >= 0; --i) free(argv[i]);
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
   _B(); printf(_MKCONFIG, path); _N();
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
   fputs("\n# Queue limit for concurrent downloads\n", f);
   fputs("# queue 5\n", f);
   fputs("\n# milkshake's repo is enabled by default\n", f);
   fputs("repository \"http://repo.openpandora.org/includes/get_data.php\"\n",f );
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
      if (!strcmp(pnd->id, p->id)) return RETURN_TRUE;
   return RETURN_FALSE;
}

/* get pnd title */
static const char* pndtitle(pndman_package *pnd, const char *lang)
{
   pndman_translated *t;
   for (t = pnd->title; t; t = t->next)
      if (!strupcmp(t->lang, lang)) return strlen(t->string)?t->string:NULL;
   return NULL;
}

/* get pnd description */
static const char* pnddesc(pndman_package *pnd, const char *lang)
{
   pndman_translated *t;
   for (t = pnd->description; t; t = t->next)
      if (!strupcmp(t->lang, lang)) return strlen(t->string)?t->string:NULL;
   return NULL;
}

/* get human readable modified time */
static char* getmodified(pndman_package *pnd)
{
   time_t nao;
   unsigned int seconds, minutes, hours, days, months, years;
   unsigned int rseconds, rminutes, rhours, rdays, rmonths, ryears;
   char *str;

   if (!(str = malloc(4+7+2+8+2+6+2+7+2+9+2+8+1)))
      return NULL;

   nao = time(0);
   rseconds  = seconds = (unsigned int)(nao - pnd->modified_time);
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

/* pndinfo */
static void pndinfo(pndman_package *pnd, _USR_DATA *data)
{
   pndman_category *c;
   pndman_license  *l;
   char *title, *desc, *info, *time;
   assert(pnd);

   /* get description */
   desc  = strstrip((char*)pnddesc(pnd, "en_US"));

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
      _G(); printf("ID            ");
      _W(); printf(": %s\n", pnd->id);
      _G(); printf("Installed     ");
      _W(); printf(": %s\n", pndinstalled(pnd, data)?"Yes":"No");
      if ((data->flags & OP_QUERY)) {
         _G(); printf("Path          ");
         _W(); printf(": %s\n", pnd->path);
      }
      _G(); printf("Repository    ");
      _W(); printf(": %s\n", strlen(pnd->repository)?pnd->repository:"Foreign");
      _G(); printf("Update        ");
      _W(); printf(": %s\n", pnd->update?"Yes":"No");
      if ((time = getmodified(pnd))) {
         _G(); printf("Modified      ");
         _W(); printf(": %s ago\n", time);
         free(time);
      }
      if (pnd->category) {
         _G(); printf("Categories    ");
         _W(); printf(": ");
         for (c = pnd->category; c; c = c->next)
            printf("%s%s%s ", strlen(c->main)?c->main:"", strlen(c->sub)?" ":"", strlen(c->sub)?c->sub:"");
         puts("");
      }
      if (pnd->license) {
         _G(); printf("Licenses      ");
         _W(); printf(": ");
         for (l = pnd->license; l; l = l->next)
            printf("%s ", strlen(l->name)?l->name:"");
         puts("");
      }
      if ((title = strstrip((char*)pndtitle(pnd, "en_US")))) {
         _G(); printf("Title         ");
         _W(); printf(": %s\n", title?title:pnd->id);
         free(title);
      }
      _G(); printf("Size          ");
      _W(); printf(": %.2f MiB\n", (float)pnd->size/1048576);
      _G(); printf("Version       ");
      _W(); printf(": %s.%s.%s.%s %s\n",
            pnd->version.major,
            pnd->version.minor,
            pnd->version.release,
            pnd->version.build,
            pnd->version.type==PND_VERSION_BETA?"Beta":
            pnd->version.type==PND_VERSION_ALPHA?"Alpha":"Release");
      _G(); printf("Rating        ");
      _W(); printf(": %d\n", pnd->rating);
      _G(); printf("URL           ");
      _W(); printf(": %s\n", pnd->url);
      _G(); printf("MD5           ");
      _W(); printf(": %s\n", pnd->md5);
      _G(); printf("Author        ");
      _W(); printf(": %s\n", pnd->author.name);
      _G(); printf("Author page   ");
      _W(); printf(": %s\n", pnd->author.website);
      _G(); printf("Vendor        ");
      _W(); printf(": %s\n", pnd->vendor);
      _G(); printf("Icon          ");
      _W(); printf(": %s\n", pnd->icon);
      if (desc) {
         NEWLINE();
         _B(); printf("Description   ");
         _W(); printf(":\n%s\n", desc);
      }
      if ((info = strstrip((char*)pnd->info))) {
         NEWLINE();
         _B(); printf("Information   ");
         _W(); printf(":\n%s\n", info);
         free(info);
      }
   } else if ((data->flags & A_SEARCH) || (data->flags & OP_YAOURT)) {
      /* default style */
      _Y(); printf("%s", pnd->category ? strlen(pnd->category->sub) ?
                         pnd->category->sub : pnd->category->main : "nogroup");
      _W(); printf("/");
      _G(); printf("%s", pnd->id);
      _W(); printf(" [");
      _Y(); printf("%s.%s.%s.%s%s",
            pnd->version.major,   pnd->version.minor,
            pnd->version.release, pnd->version.build,
            pnd->version.type==PND_VERSION_BETA?" beta":
            pnd->version.type==PND_VERSION_ALPHA?" alpha":"");
      _W(); printf("] ");
      _W(); printf("["); _Y(); printf("%d", pnd->rating); _W(); printf("] ");
      _B(); printf("%s", pndinstalled(pnd, data)?"[installed] ":"");
      _W(); printf("%s\n", pnd->update?"--UPDATE-- ":"");
      _W(); printf("    %s", desc?desc:"...");
   } else {
      _G(); printf("%s ", pnd->id);
      _Y(); printf("%s.%s.%s.%s%s",
            pnd->version.major,   pnd->version.minor,
            pnd->version.release, pnd->version.build,
            pnd->version.type==PND_VERSION_BETA?" beta":
            pnd->version.type==PND_VERSION_ALPHA?" alpha":"");
   }  _N();

   if (desc)  free(desc);
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
   NEWLINE();
   _Y(); printf(_NO_ROOT_YET); _N();
   _B(); for (d = data->dlist; d; d = d->next) printf("%d. %s\n", ++i, d->mount); _N();
   _Y(); printf(_INPUT_LINE); _N();
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

/* yaourt mode dialog */
static int yaourtdialog(_USR_DATA *data)
{
   char response[1024], num[5];
   unsigned int i, c, match;
   _USR_TARGET *t, *nt, *nlist = NULL;

   if (!data->tlist) return RETURN_FALSE;

   /* output query */
   for (i = 0, t = data->tlist; t; t = t->next) {
      _W(); printf("%d. ", ++i);
      pndinfo(t->pnd, data);
      NEWLINE();
   }

   /* output question */
   _Y(); puts(_YAOURT_DIALOG);
   _Y(); printf(_INPUT_LINE); _N();
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
   char response[32];

   fflush(stdout);
   _B(); vprintf(fmt, args); _W();
   printf(" %s %s", yn?_YESNO:_NOYES, noconfirm?"\n":""); _N();
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
static void progressbar(float downloaded, float total_to_download)
{
   unsigned int dots, i, pwdt;
   float fraction;

   pwdt = 40; /* width */
   if (!total_to_download) {
      _Y(); for (i = 0; i != ((unsigned int)downloaded / 1024) % pwdt; ++i) printf("."); _N();
      printf("\r");
      fflush(stdout);
      return;
   }
   fraction = (float)downloaded / (float)total_to_download;
   dots     = round(fraction * pwdt);

   _Y(); printf("%3.0f%% ", fraction * 100); _W();
   printf("["); _R();
   for (i = 0; i != dots; ++i) printf("=");
   for (     ; i != pwdt; ++i) printf(" ");
   _W(); printf("] "); _B();
   printf("%4.2f MiB / %4.2f MiB\r", downloaded/1048576, total_to_download/1048567); _N();
   fflush(stdout);
}

/* dialog shown before performing action */
static int pre_op_dialog(_USR_DATA *data)
{
   _USR_TARGET *t, *tn;
   unsigned int count;
   double size;
   char *unit, gotdialog = 0;
   assert(data);

   /* ask for ignores */
   for (count = 0, t = data->tlist; t; t = tn) {
      tn = t->next; ++count;
      if (!pndinstalled(t->pnd, data)) {
         if (checkignore(t->pnd->id, data))
            if (!yesno(data, _PND_IGNORED_FORCE, t->pnd->id)) {
               _R(); printf(_WARNING_SKIPPING, t->pnd->id); _N();
               data->tlist = freetarget(t);
               --count; gotdialog = 1;
            }
      } else {
         if ((data->flags & GB_NEEDED)  && !t->pnd->update ||
             !(data->flags & A_UPGRADE) && !(data->flags & OP_UPGRADE) &&
             !(data->flags & OP_REMOVE) && !t->pnd->update &&
             !yesno(data, _PND_REINSTALL, t->pnd->id)) {
            if ((data->flags & GB_NEEDED)) {
               _R(); printf(_WARNING_UPDATE, t->pnd->id); _N();
            }
            data->tlist = freetarget(t);
            --count; gotdialog = 1;
         }
      }
   }

   /* nothing to perform action on */
   if (!count) {
      _Y(); puts(_NOTHING_TO_DO); _N();
      return RETURN_FALSE;
   }

   /* formatting */
   if (gotdialog) NEWLINE();

   /* show target line */
   _G(); printf(_TARGET_LINE); _W(); printf(" (");
   _Y(); printf("%d", count);  _W(); printf(")  : ");
   for (size = 0, t = data->tlist; t; t = t->next) {
      _Y(); printf("%s", t->pnd->id);
      if (t->next) { _W(); printf(", "); }
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
      _Y(); printf(_TARGET_MEDIA);
      _W(); printf(": %s\n", data->root->mount);
      _Y(); printf(_TARGET_PATH);
      _W(); printf(": %s\n", (data->flags & A_APPS)?"/apps":
                             (data->flags & A_DESKTOP)?"/desktop":"/menu");
   }
   NEWLINE();

   /* print action size */
   _B(); printf("%s", (data->flags & OP_REMOVE)?_REMOVE_SIZE:_DOWNLOAD_SIZE);
   _W(); printf(": %.2f %s\n", size, unit); _N();

   NEWLINE();
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

/* synchorize repositories */
static void syncrepos(pndman_repository *rs, _USR_DATA *data)
{
   pndman_repository *r;
   float tdl, ttdl; /* total download, total total to download */
   int ret;
   unsigned int c = 0;

   for (r = rs; r; r = r->next) ++c;
   pndman_sync_handle handle[c]; r = rs;
   for (c = 0; r; r = r->next) pndman_sync_request(&handle[c++],
               (data->flags & GB_NOMERGE)?PNDMAN_SYNC_FULL:0, r);

   tdl = 0; ttdl = 0;
   while ((ret = pndman_sync() > 0)) {
      if (!(data->flags & GB_NOBAR)) progressbar(tdl, ttdl);
      r = rs; tdl = 0; ttdl = 0;
      for (c = 0; r; r = r->next) {
         tdl   += (float)handle[c].progress.download;
         ttdl  += (float)handle[c].progress.total_to_download;
         if (handle[c].progress.done) {
            _G(); printf(_REPO_SYNCED, handle[c].repository->name); _N();
         }
         ++c;
      }
      usleep(1000);
   }

   for (r = rs, c = 0; r; r = r->next) {
      if (!handle[c].progress.done) {
         _R();
         printf(_REPO_SYNCED_NOT, strlen(handle[c].repository->name) ?
                handle[c].repository->name : handle[c].repository->url);
         if (strlen(handle[c].error))
            puts(handle[c].error);
         _N();
      }
      pndman_sync_request_free(&handle[c]);
      ++c;
   }
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
         if (!strupcmp(c->main, t->id))     return RETURN_TRUE;
         else if (!strupcmp(c->sub, t->id)) return RETURN_TRUE;
      return RETURN_FALSE;
   }

   if (strupstr(p->id, t->id))        return RETURN_TRUE;
   for (s = p->title; s; s = s->next)
      if (strupstr(s->string, t->id)) return RETURN_TRUE;
   for (s = p->description; s; s = s->next)
      if (strupstr(s->string, t->id)) return RETURN_TRUE;

   return RETURN_FALSE;
}

/* search repository */
static int searchrepo(pndman_repository *r, _USR_DATA *data)
{
   pndman_package *p;
   _USR_TARGET *t;
   char donl = 0;
   assert(r);

   /* lame search for now */
   if (!data->tlist)
      for (p = r->pnd; p; p = p->next) {
         if ((data->flags & A_UPGRADE) && !p->update)
            continue;
         pndinfo(p, data);
         if (p->next) NEWLINE();
      }
   else
      for (t = data->tlist; t; t = t->next)
         for (p = r->pnd; p; p = p->next) {
            if ((data->flags & A_UPGRADE) && !p->update)
               continue;
            if (matchquery(p, t, data)) {
               if (donl) NEWLINE();
               pndinfo(p, data);
               donl = 1;
            }
         }

   /* formatting */
   if (!(data->flags & A_INFO)) NEWLINE();
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
               if (_VERBOSE >= 1) printf("A: %s %s\n", t->id, p->id);
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

   /* then strstr first */
   for (t = data->tlist; t && t != ts; t = t->next) {
      if (!(data->flags & OP_YAOURT) && t->pnd) continue;
      for (r = rs; r; r = r->next) {
         for (p = r->pnd; p; p = p->next)
            if (strstr(p->id, t->id)) {
               if (_VERBOSE >= 1) printf("A: %s %s\n", t->id, p->id);
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
         _R(); printf(_PND_NOT_FOUND, t->id); _N();
         data->tlist = freetarget(t);
         continue;
      }
      if (up && !t->pnd->update) {
         _R(); printf(_PND_NO_UPDATE, t->id); _N();
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
   assert(data);
   for (p = data->rlist->pnd; p; p = p->next)
      if (p->update) { _B(); printf(_PND_HAS_UPDATE, p->id); _N(); }
}

/* get handle flags from milkyhelper's flags */
static unsigned int handleflagsfromflags(unsigned int flags)
{
   unsigned int hflags = 0;
   if ((flags & OP_SYNC))           hflags |= PNDMAN_HANDLE_INSTALL;
   else if ((flags & OP_REMOVE))    hflags |= PNDMAN_HANDLE_REMOVE;
   if ((flags & GB_FORCE))          hflags |= PNDMAN_HANDLE_FORCE;

   /* don't assing destination on upgrade */
   if (!(flags & OP_UPGRADE) && !(flags & A_UPGRADE)) {
      if ((flags & A_DESKTOP))         hflags |= PNDMAN_HANDLE_INSTALL_DESKTOP;
      else if ((flags & A_APPS))       hflags |= PNDMAN_HANDLE_INSTALL_APPS;
      else                             hflags |= PNDMAN_HANDLE_INSTALL_MENU; /* default to menu */
   }

   /* create backup of old file */
   if (flags & A_BACKUP) hflags |= PNDMAN_HANDLE_BACKUP;
   return hflags;
}

/* get operation string from milkyhelper's flags */
static const char* opstrfromflags(char done, unsigned int flags)
{
   if ((flags & A_UPGRADE) || (flags & OP_UPGRADE))
                                 return done?_UPGRADE_DONE:_UPGRADE_FAIL;
   else if ((flags & OP_SYNC))   return done?_INSTALL_DONE:_INSTALL_FAIL;
   else if ((flags & OP_REMOVE)) return done?_REMOVE_DONE:_REMOVE_FAIL;
   return _UNKNOWN_OPERATION;
}

/* commit handle to repository */
static void commithandle(_USR_DATA *data, pndman_handle *handle)
{
   assert(data && handle);
   if (!handle->progress.done && !(data->flags & OP_REMOVE)) {
      _R(); printf(opstrfromflags(0,data->flags), handle->name);
      if (strlen(handle->error)) puts(handle->error);
      _N();
   } else if (pndman_handle_commit(handle, data->rlist) != RETURN_OK) {
      _R(); printf(opstrfromflags(0,data->flags), handle->name); _N();
   } else {
      _G(); printf(opstrfromflags(1,data->flags), handle->name); _N();
   }
}

/* remove PND's appdata */
static void removeappdata(pndman_package *pnd, _USR_DATA *data)
{
   pndman_application *a;
   pndman_device *d, *dev;
   assert(pnd && data);

   /* user don't want this */
   if (!(data->flags & A_NOSAVE)) return;

   /* find device the PND is on */
   for (dev = NULL, d = data->dlist; d; d = d->next)
      if(!strcmp(d->mount, pnd->mount)) {
         dev = d;
         break;
      }

   /* TODO: remove appdata */
   for (a = pnd->app; a; a = a->next) {
      _W(); printf("%s appdata is: %s/pandora/appdata/%s, however this functionality is not yet added.\n", pnd->id, dev->mount, a->appdata); _N();
   }
}

/* perform target's action */
static int targetperform(_USR_DATA *data)
{
   _USR_TARGET *t;
   float tdl, ttdl; /* total download, total total to download */
   int ret;
   unsigned int c = 0, count = 0;
   assert(data);

   for (t = data->tlist; t; t = t->next) ++c;
   pndman_handle handle[c]; t = data->tlist; char done[c], start[c];
   for (c = 0; t; t = t->next) {
      pndman_handle_init(strlen(t->pnd->id)?t->pnd->id:"notitle", &handle[c]);
      handle[c].pnd     = t->pnd;
      handle[c].device  = ((data->flags & OP_UPGRADE) || (data->flags & A_UPGRADE))?data->dlist:data->root;
      handle[c].flags   = handleflagsfromflags(data->flags);
      done[c]           = 0;
      start[c]          = 0;
      if (c<_QUEUE) {
         if (pndman_handle_perform(&handle[c]) != RETURN_OK)
            handle[c].flags = 0;
         start[c] = 1;
      }
      ttdl += t->pnd->size;
      ++c;
   }

   tdl = 0; /* ttdl = 0; */
   while ((ret = pndman_download()) > 0) {
      if (!(data->flags & GB_NOBAR)) progressbar(tdl, ttdl);
      t = data->tlist; tdl = 0; /* ttdl = 0; */
      for (c = 0; t; t = t->next) {
         if (!handle[c].flags) { ++c; continue; } /* failed perform */
         tdl  += (float)handle[c].progress.download;
         /* ttdl += (float)handle[c].progress.total_to_download; */
         if (handle[c].progress.done && !done[c]) {
            if (!(data->flags & GB_NOBAR)) NEWLINE();
            /* commit to repository */
            commithandle(data, &handle[c]);
            done[c] = 1;
         }
         if (count < _QUEUE && !start[c]) {
            if (pndman_handle_perform(&handle[c]) != RETURN_OK)
               handle[c].flags = 0;
            start[c] = 1;
         }
         ++c;
      }
      count = c;
      usleep(1000);
   }

   /* free handles */
   t = data->tlist;
   for (c = 0; t; t = t->next) {
      /* this is for non download operations */
      if ((data->flags & OP_REMOVE)) {
         pndman_crawl_pnd(1, handle[c].pnd);
         removeappdata(handle[c].pnd, data);
         commithandle(data, &handle[c]);
      }
      pndman_handle_free(&handle[c]);
      ++c;
   }
   return RETURN_OK;
}

/* check that we have remote repository */
static pndman_repository* checkremoterepo(const char *str, _USR_DATA *data)
{
   assert(str && data);
   if (data->rlist->next) return data->rlist->next;
   else { _R(); printf(_REPO_WONT_DO_ANYTHING, str); _N(); }
   return NULL;
}

/* check tha we have synchorized repository */
static int checksyncedrepo(pndman_repository *r)
{
   assert(r);
   for (; r && !r->timestamp; r = r->next);
   if (!r) { _R(); printf(_NO_SYNCED_REPO); _N(); }
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

   /* check that we aren't using local repository */
   if (!(rs = checkremoterepo("sync", data)))
      return RETURN_FAIL;

   /* repository syncing is always the first operation */
   if (data->flags & A_REFRESH) {
      syncrepos(rs, data);
      pndman_check_updates(data->rlist);
      listupdate(data);
   }

   /* search */
   if (isquery(data->flags))  {
      for (r = rs; r && strlen(r->url); r = r->next)
         searchrepo(r, data);
      return RETURN_OK;
   }

   /* everything else needs synced repositories */
   if (!checksyncedrepo(rs)) return RETURN_FAIL;

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
   /* lame query for now */
   return searchrepo(data->rlist, data);
}

/* crawl operation logic */
static int crawlprocess(_USR_DATA *data)
{
   pndman_device *d;
   int f = 0, ret = 0;

   /* crawl and count on success */
   for (d = data->dlist; d; d = d->next)
      if ((ret = pndman_crawl(0, d, data->rlist)) != RETURN_FAIL) f += ret;

   pndman_check_updates(data->rlist);
   listupdate(data);
   _G(); printf(_PNDS_CRAWLED,f); _N();
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
      if (strlen(d->appdata)) {
         strncpy(tmp, d->appdata, PATH_MAX-1);
         _Y(); printf(_CLEANING_DB, tmp); _N();
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
         _G(); printf(_DEVICE_CLEANED, d->mount); _N();
      }
   return RETURN_OK;
}

/* version operation logic */
static int version(_USR_DATA *data)
{
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

   _Y(); printf("libpndman && milkyhelper\n\n");
   _B(); printf("https://github.com/Cloudef/libpndman\n"); _W();
   printf("~ %s\n", pndman_git_head());
   printf("~ %s\n\n", pndman_git_commit());
   _W(); printf("~ "); _R(); printf("Cloudef"); printf("\n");
   _Y(); printf("<o/ "); _G(); printf("DISCO!\n"); _N();
   return RETURN_OK;
}

/* help operation logic */
static int help(_USR_DATA *data)
{
   int i;

   printf("usage: %s [-%s] ", data->bin, _G_ARG);
   for (i = 0; i != strlen(_OP_ARG); ++i) {
      printf("[-%c%s] ", _OP_ARG[i],
            _OP_ARG[i]=='S'?_S_ARG:
            _OP_ARG[i]=='R'?_R_ARG:
            _OP_ARG[i]=='Q'?_Q_ARG:"");
   }
   NEWLINE();

   data->flags &= ~OP_HELP;
   if (!hasop(data->flags)) {
      NEWLINE();
      _G(); printf("~ Global arguments:\n");
      _W(); printf("  -v : Verbose mode, combine to increase verbose level.\n");
            printf("  -f : Force install even if MD5 checking fails.\n");
            printf("  -t : Plain text mode, do not use colors.\n");
            printf("  -r : Root device/directory.\n%s\n%s\n%s\n",
                   "       All operations are going to be done under this device/directory.",
                   "       You can leave argument empty, if you want to choose device from a list.",
                   "       NOTE: Directory must be a absolute path!");

      NEWLINE();
      _G(); printf("~ Operation arguments:\n");
      _W(); printf("  -S : Sync/Search/Install operation.\n");
      _W(); printf("  -U : Upgrade operation.\n");
      _W(); printf("  -R : Removal operation.\n");
      _W(); printf("  -Q : Local query operation.\n");
      _W(); printf("  -P : Crawls PND's from media.\n");
      _W(); printf("  -C : Cleans all database files.\n");
      _W(); printf("  -V : Show version information.\n");
      _W(); printf("  -h : Show help. Combine with operation to get more help.\n");

      NEWLINE();
      _G(); printf("~ Other arguments:\n");
      _W(); printf("  --root      : Alias for -r option.\n");
      _W(); printf("  --help      : Alias for -h option.\n");
      _W(); printf("  --version   : Alias for -V option.\n");
      _W(); printf("  --noconfirm : Don't prompt questions.\n");
      _W(); printf("  --nomerge   : Do full repository synchorization.\n");
      _W(); printf("  --needed    : Don't reinstall up-to-date PND's.\n");
      _W(); printf("  --nobar     : Don't show progress bar.\n");
   } else if ((data->flags & OP_SYNC)) {
      NEWLINE();
      _G(); printf("~ Sync arguments:\n");
      _W(); printf("  -s : Search remote repositories, by matching id/title/description.\n");
      _W(); printf("  -c : Search remote repositories, by matching category.\n");
      _W(); printf("  -i : Show full information of matching PND.\n");
      _W(); printf("  -l : List all remote PND's.\n");
      _W(); printf("  -y : Synchorize remote repository with local database.\n");
      _W(); printf("  -u : Upgrade PND's / Filter by upgrade status.\n");
      _W(); printf("  -m : Use /menu as install target.\n");
      _W(); printf("  -d : Use /desktop as install target.\n");
      _W(); printf("  -a : Use /apps as install target.\n");
      _W(); printf("  -b : Make backup of original file.\n");
   } else if ((data->flags & OP_REMOVE)) {
      NEWLINE();
      _G(); printf("~ Removal arguments:\n");
      _W(); printf("  -n : Remove also appdata.\n");
   } else if ((data->flags & OP_QUERY)) {
      NEWLINE();
      _G(); printf("~ Query arguments:\n");
      _W(); printf("  -s : Search local database, by matching id/title/description.\n");
      _W(); printf("  -c : Search local database, by matching category.\n");
      _W(); printf("  -i : Show full information of matching PND.\n");
      _W(); printf("  -u : Filter query with upgrade status.\n");
   } else if ((data->flags & OP_UPGRADE)) {
      _G(); printf("~ Upgrade operation:\n");
      _W(); printf("  This operation is just a alias for -Su.\n");
   } else {
      _R(); printf("This operation has no arguments.\n");
   }

   _N();
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
         _R(); NEWLINE(); printf(_BAD_DEVICE_SELECTED); _N();
         return RETURN_FAIL;
      }

   /* save root */
   if (saveroot(data) != RETURN_OK)
   { _R(); printf(_ROOT_FAIL); _N(); }

   /* check target list, NOTE: OP_CLEAN && OP_QUERY can perform without targets */
   if (!data->tlist && needtarget(data->flags)) {
      if (!data->no_action) {
         _R(); printf(_NO_X_SPECIFIED, "targets"); _N();
      } else {
         _Y(); puts(data->no_action); _N();
      }
      return RETURN_FAIL;
   } else if (!hasop(data->flags)) data->flags |= OP_YAOURT | OP_SYNC;

   /* check flags */
   if (!data->flags || !hasop(data->flags)) {
      if (!data->no_action) {
         _R(); printf(_NO_X_SPECIFIED, "operation"); _N();
      } else {
         _Y(); puts(data->no_action); _N();
      }
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
   int ret = RETURN_FAIL;
   assert(data);

   /* set pndman's verbose level */
   pndman_set_verbose(_VERBOSE);

   /* sanity check */
   if (sanitycheck(data) != RETURN_OK)
      return RETURN_FAIL;

   if (_VERBOSE >= 1) {
      NEWLINE();
      _B();
      if ((data->flags & OP_YAOURT))   puts("::YAOURT");
      if ((data->flags & OP_SYNC))     puts("::SYNC");
      if ((data->flags & OP_UPGRADE))  puts("::UPGRADE");
      if ((data->flags & OP_REMOVE))   puts("::REMOVE");
      if ((data->flags & OP_QUERY))    puts("::QUERY");
      if ((data->flags & OP_CRAWL))    puts("::CRAWL");
      if ((data->flags & OP_CLEAN))    puts("::CLEAN");
      if ((data->flags & OP_VERSION))  puts("::VERSION");
      if ((data->flags & OP_HELP))     puts("::HELP");
      _Y();
      if ((data->flags & GB_FORCE))     puts(";;FORCE");
      if ((data->flags & GB_NOCONFIRM)) puts(";;NOCONFIRM");
      if ((data->flags & GB_NOMERGE))   puts(";;NOMERGE");
      if ((data->flags & GB_NEEDED))    puts(";;NEEDED");
      if ((data->flags & GB_NOBAR))     puts(";;NOBAR");
      _G();
      if ((data->flags & A_SEARCH))    puts("->SEARCH");
      if ((data->flags & A_CATEGORY))  puts("->CATEGORY");
      if ((data->flags & A_INFO))      puts("->INFO");
      if ((data->flags & A_LIST))      puts("->LIST");
      if ((data->flags & A_REFRESH))   puts("->REFRESH");
      if ((data->flags & A_UPGRADE))   puts("->UPGRADE");
      if ((data->flags & A_NOSAVE))    puts("->NOSAVE");
      if ((data->flags & A_BACKUP))    puts("->BACKUP");
      _R();
      if ((data->flags & A_MENU))      puts("[MENU]");
      if ((data->flags & A_DESKTOP))   puts("[DESKTOP]");
      if ((data->flags & A_APPS))      puts("[APPS]");
      _N();
      NEWLINE();

      if (data->root) {
         _B(); printf("Root: %s\n", data->root->mount); _N();
      }

      _R();
      if (data->rlist) puts("\nRepositories:");
      for (r = data->rlist; r; r = r->next) puts(strlen(r->name) ? r->name : r->url);
      _N();

      _W();
      if (data->tlist) puts("\nTargets:");
      for (t = data->tlist; t; t = t->next) puts(t->id);
      _N();
      NEWLINE();
   }

   /* read repository information from each device */
   for (d = data->dlist; d; d = d->next)
      for (r = data->rlist; r; r = r->next) pndman_read_from_device(r, d);
   pndman_repository_check_local(data->rlist);  /* check for removed/bad pnds */
   pndman_check_updates(data->rlist);           /* check for updates */

   /* logic */
   if ((data->flags & OP_VERSION))        ret = version(data);
   else if ((data->flags & OP_HELP))      ret = help(data);
   else if ((data->flags & OP_YAOURT))    ret = yaourtprocess(data);
   else if ((data->flags & OP_SYNC))      ret = syncprocess(data);
   else if ((data->flags & OP_UPGRADE))   ret = upgradeprocess(data);
   else if ((data->flags & OP_REMOVE))    ret = removeprocess(data);
   else if ((data->flags & OP_QUERY))     ret = queryprocess(data);
   else if ((data->flags & OP_CRAWL))     ret = crawlprocess(data);
   else if ((data->flags & OP_CLEAN))     ret = cleanprocess(data);

   /* commit everything to disk */
   if (!(data->flags & OP_CLEAN)) {
      if (pndman_commit_all(data->rlist, data->root) != RETURN_OK)
      { _R(); printf(_COMMIT_FAIL, data->root->mount); _N(); }
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
   _R(); printf("Caught %s, exiting...\n",
         sig == SIGINT  ? "SIGINT"  :
         sig == SIGTERM ? "SIGTERM" : "SIGSEGV"); _N();
#ifdef __WIN32__
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
   memset(path, 0, PATH_MAX);

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
   if (!(data.dlist = pndman_device_detect(NULL))) {
      _R(); printf(_FAILED_TO_DEVICES); _N();
   }

   /* get local repository */
   if (!(data.rlist = pndman_repository_init())) {
      _R(); printf(_FAILED_TO_REPOS); _N();
   }

   /* do logic, if everything ok! */
   if (data.dlist && data.rlist) {
      /* read configuration */
      if ((ret = getconfigpath(path)) == RETURN_OK) ret = readconfig(path, &data);
      if (ret != RETURN_OK) { _R(); printf(_CONFIG_READ_FAIL, path); _N(); }

      /* read root */
      data.root = getroot(&data);

      /* parse arguments */
      parseargs(argc, argv, &data);
      ret = processflags(&data) == RETURN_OK ? EXIT_SUCCESS : EXIT_FAILURE;
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
