#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "pndman.h"

#ifdef WIN32
#  include <windows.h>
#elif __linux__
#  include <limits.h>
#endif

/*   ____ _____ _   _ _____ ____      _    _
 *  / ___| ____| \ | | ____|  _ \    / \  | |
 * | |  _|  _| |  \| |  _| | |_) |  / _ \ | |
 * | |_| | |___| |\  | |___|  _ <  / ___ \| |___
 *  \____|_____|_| \_|_____|_| \_\/_/   \_\_____|
*/

/* use colors? */
char _USE_COLORS  = 1;

/* verbose level */
char _VERBOSE     = 0;

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

typedef struct _USR_DATA
{
   unsigned int      flags;
   pndman_device     *dlist;
   pndman_device     *root;
   pndman_repository *rlist;
   _USR_TARGET       *tlist;
} _USR_DATA;

/*   ____ ___  _     ___  ____  ____
 *  / ___/ _ \| |   / _ \|  _ \/ ___|
 * | |  | | | | |  | | | | |_) \___ \
 * | |__| |_| | |__| |_| |  _ < ___) |
 *  \____\___/|_____\___/|_| \_\____/
 */

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
}

/*  _   _ _____ ___ _
 * | | | |_   _|_ _| |
 * | | | | | |  | || |
 * | |_| | | |  | || |___
 *  \___/  |_| |___|_____|
 */

static char* strtrim(char *str)
{
   char *pch = str;

   /* check if we need trimming */
   if (!str || *str == '\0') return str;

   /* trim from left */
   while (isspace((unsigned char)*pch)) ++pch;
   if (pch != str)   memmove(str, pch, (strlen(pch) + 1));

   /* this string is empty, return */
   if (*str == '\0') return str;

   /* trim from right */
   pch = (str + (strlen(str) - 1));
   while (isspace((unsigned char)*pch)) --pch;

   *++pch = '\0';
   return str;
}

/*  ____  _______     _____ ____ _____ ____
 * |  _ \| ____\ \   / /_ _/ ___| ____/ ___|
 * | | | |  _|  \ \ / / | | |   |  _| \___ \
 * | |_| | |___  \ V /  | | |___| |___ ___) |
 * |____/|_____|  \_/  |___\____|_____|____/
 */

static pndman_device* setroot(char *root, pndman_device *list)
{
   pndman_device *d;
   assert(root);
   for (d = list; d; d = d->next)
      if (!strcmp(root, d->mount) || !strcmp(root, d->device)) return d;
   if ((d = pndman_device_add(root, list))) return d;
   _R(); printf("Failed to root: %s\n", root); _N();
   return NULL;
}

/*  _____  _    ____   ____ _____ _____ ____
 * |_   _|/ \  |  _ \ / ___| ____|_   _/ ___|
 *   | | / _ \ | |_) | |  _|  _|   | | \___ \
 *   | |/ ___ \|  _ <| |_| | |___  | |  ___) |
 *   |_/_/   \_\_| \_\\____|_____| |_| |____/
 */

static _USR_TARGET* addtarget(char *id, _USR_TARGET **list)
{
   _USR_TARGET *new, *t;
   assert(id);

   if (*list) {
      for (t = *list; t; t = t->next) if (!strcmp(t->id, id)) return NULL;
      for (t = *list; t && t->next; t = t->next);
      t->next = new = malloc(sizeof(_USR_TARGET));
   } else *list = new = malloc(sizeof(_USR_TARGET));
   if (!new) return NULL;

   if (*list) new->prev = t;
   else       new->prev = NULL;

   memset(new->id, 0, PND_ID);
   strncpy(new->id, id, PND_ID-1);
   new->pnd  = NULL;
   new->next = NULL;
   return new;
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

static void init_usrdata(_USR_DATA *data)
{
   data->flags = 0;
   data->dlist = NULL; data->root  = NULL;
   data->rlist = NULL; data->tlist = NULL;
}

static void _addtarget(char *id, _USR_DATA *data)
{
   addtarget(id, &data->tlist);
}

static void _setroot(char *root, _USR_DATA *data)
{
   data->root = setroot(root, data->dlist);
}

/*     _    ____   ____ _   _ __  __ _____ _   _ _____ ____
 *    / \  |  _ \ / ___| | | |  \/  | ____| \ | |_   _/ ___|
 *   / _ \ | |_) | |  _| | | | |\/| |  _| |  \| | | | \___ \
 *  / ___ \|  _ <| |_| | |_| | |  | | |___| |\  | | |  ___) |
 * /_/   \_\_| \_\\____|\___/|_|  |_|_____|_| \_| |_| |____/
 */

#define _PASSARG(x) { x(narg, data); *skipn = 1; return; }
#define _PASSTHS(x) { x(arg, data); return; }
#define _PASSFLG(x) { data->flags |= x; return; }
static const char* _G_ARG  = "vft";          /* global arguments */
static const char* _OP_ARG = "SURQCVh";      /* operations */
static const char* _S_ARG  = "scilyupmda";   /* sync operation arguments */
static const char* _R_ARG  = "n";            /* remove operation arguments */
static const char* _Q_ARG  = "scilu";        /* query operation arguments */

typedef enum _HELPER_FLAGS
{
   GB_FORCE    = 0x000001, OP_YAOURT    = 0x000002,
   OP_SYNC     = 0x000004, OP_UPGRADE   = 0x000008,
   OP_REMOVE   = 0x000010, OP_QUERY     = 0x000020,
   OP_CLEAN    = 0x000040, OP_VERSION   = 0x000080,
   OP_HELP     = 0x000100, A_SEARCH     = 0x000200,
   A_CATEGORY  = 0x000400, A_INFO       = 0x000800,
   A_LIST      = 0x001000, A_REFRESH    = 0x002000,
   A_UPGRADE   = 0x004000, A_CRAWL      = 0x008000,
   A_MENU      = 0x010000, A_DESKTOP    = 0x020000,
   A_APPS      = 0x040000, A_NOSAVE     = 0x080000,
   S_NOMERGE   = 0x100000,
} _HELPER_FLAGS;

static int hasop(unsigned int flags)
{
   return ((flags & OP_YAOURT)  || (flags & OP_SYNC)    ||
           (flags & OP_REMOVE)  || (flags & OP_UPGRADE) ||
           (flags & OP_CLEAN)   || (flags & OP_QUERY)   ||
           (flags & OP_VERSION) || (flags & OP_HELP));
}

typedef _HELPER_FLAGS (*_PARSE_FUNC)(char, char*, int*, _USR_DATA*);
static _HELPER_FLAGS getglob(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 'v')        ++_VERBOSE;
   else if (c == 'f')   return GB_FORCE;
   else if (c == 't')   _USE_COLORS = 0;
   return 0;
}

static _HELPER_FLAGS getop(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 'S')        return OP_SYNC;
   else if (c == 'U')   return OP_UPGRADE;
   else if (c == 'R')   return OP_REMOVE;
   else if (c == 'Q')   return OP_QUERY;
   else if (c == 'C')   return OP_CLEAN;
   else if (c == 'V')   return OP_VERSION;
   else if (c == 'h')   return OP_HELP;
   return 0;
}

static _HELPER_FLAGS getsync(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 's')        return A_SEARCH;
   else if (c == 'c')   return A_CATEGORY;
   else if (c == 'i')   return A_INFO;
   else if (c == 'l')   return A_LIST;
   else if (c == 'y')   return A_REFRESH;
   else if (c == 'u')   return A_UPGRADE;
   else if (c == 'p')   return A_CRAWL;
   else if (c == 'm')   return A_MENU;
   else if (c == 'd')   return A_DESKTOP;
   else if (c == 'a')   return A_APPS;
   return 0;
}

static _HELPER_FLAGS getremove(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 'n') return A_NOSAVE;
   return 0;
}

static _HELPER_FLAGS getquery(char c, char *arg, int *skipn, _USR_DATA *data)
{
   if (c == 's')        return A_SEARCH;
   else if (c == 'c')   return A_CATEGORY;
   else if (c == 'i')   return A_INFO;
   else if (c == 'l')   return A_LIST;
   else if (c == 'u')   return A_UPGRADE;
   return 0;
}

static void parse(_PARSE_FUNC func, const char *ref, char *arg, char *narg, int *skipn, _USR_DATA *data)
{
   int x, y;
   for (y = 0; y != strlen(ref); ++y)
      for (x = 0; x != strlen(arg); ++x)
         if (arg[x] == ref[y]) {
            data->flags |= func(ref[y], narg, skipn, data);
            if (func == getop) return; /* only one OP */
         }
}

static void parsearg(char *arg, char *narg, int *skipn, _USR_DATA *data)
{
   if (!strncmp(arg, "-r", 2))      _PASSARG(_setroot);     /* argument with argument */
   if (!strncmp(arg, "--help", 6))  _PASSFLG(OP_HELP);      /* another way of calling help */
   if (arg[0] != '-')               _PASSTHS(_addtarget);   /* not argument */
   parse(getglob, _G_ARG, arg, narg, skipn, data);
   if (!hasop(data->flags))         parse(getop, _OP_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_SYNC))     parse(getsync, _S_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_REMOVE))   parse(getremove, _R_ARG, arg, narg, skipn, data);
   if ((data->flags & OP_QUERY))    parse(getquery, _Q_ARG, arg, narg, skipn, data);
}

static void parseargs(int argc, char **argv, _USR_DATA *data)
{
   int i, skipn = 0;
   for (i = 1; i != argc; ++i)
      if (!skipn) parsearg(argv[i], argc==i+1 ? NULL:argv[i+1], &skipn, data);
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

static int _addrepository(char **argv, int argc, _USR_DATA *data)
{
   if (!argc) return RETURN_FAIL;
   if (!(pndman_repository_add(argv[0], data->rlist)))
      return RETURN_FAIL;
   return RETURN_OK;
}

static int _addignore(char **argv, int argc, _USR_DATA *data)
{
   return RETURN_OK;
}

static int _nomerge(char **argv, int argc, _USR_DATA *data)
{
   data->flags |= S_NOMERGE;
   return RETURN_OK;
}

/*   ____ ___  _   _ _____ ___ ____
 *  / ___/ _ \| \ | |  ___|_ _/ ___|
 * | |  | | | |  \| | |_   | | |  _
 * | |__| |_| | |\  |  _|  | | |_| |
 *  \____\___/|_| \_|_|   |___\____|
 */

#define _CONFIG_SEPERATOR  ' '
#define _CONFIG_COMMENT    '#'
#define _CONFIG_QUOTE      '\"'
#define _CONFIG_MAX_VAR    24
#define _CONFIG_KEY_LEN    24
#define _CONFIG_ARG_LEN    256

typedef int (*_CONFIG_FUNC)(char **argv, int argc, _USR_DATA *data);
typedef struct _CONFIG_KEYS
{
   const char     key[_CONFIG_KEY_LEN];
   _CONFIG_FUNC   func;
} _CONFIG_KEYS;

/* configuration options */
static _CONFIG_KEYS _CONFIG_KEY[] = {
   { "repository", _addrepository },
   { "ignore", _addignore },
   { "nomerge", _nomerge },
};

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
   in_quote = 0; p = 0; argc = 0;
   for (; i != strlen(line); ++i) {
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

static int readconfig(const char *path, _USR_DATA *data)
{
   unsigned int i;
   char line[LINE_MAX];
   FILE *f;

   if (!(f = fopen(path, "r")))
         return RETURN_FAIL;

   while (fgets(line, LINE_MAX, f)) {
      strtrim(line); /* trim */
      if (!strlen(line) || line[0] == _CONFIG_COMMENT) continue; /* skip comments */
      for (i = 0; _CONFIG_KEY[i].key; ++i)
         if (!strncmp(_CONFIG_KEY[i].key, line, strlen(_CONFIG_KEY[i].key))) {
            readconfig_arg(_CONFIG_KEY[i].key, _CONFIG_KEY[i].func, data, line);
            break;
         }
   }
   fclose(f);

   return RETURN_OK;
}

/*  ___ _   _ ____  _   _ _____
 * |_ _| \ | |  _ \| | | |_   _|
 *  | ||  \| | |_) | | | | | |
 *  | || |\  |  __/| |_| | | |
 * |___|_| \_|_|    \___/  |_|
 */

static int rootdialog(_USR_DATA *data)
{
   unsigned int i, s;
   char response[32];
   pndman_device *d;

   i = 0;
   fflush(stdout);
   puts("");
   _Y(); puts("Sir! You haven't selected your mount where to store PND's yet."); _N();
   _B(); for (d = data->dlist; d; d = d->next) printf("%d. %s\n", ++i, d->mount); _N();
   _Y(); printf("> "); _N();
   fflush(stdout);

   if (!fgets(response, sizeof(response), stdin))
      return RETURN_FAIL;

   if ((s = strtol(response, (char **) NULL, 10)) <= 0)
      return RETURN_FAIL;

   /* good answer got, set root */
   i = 0;
   for (d = data->dlist; d; d = d->next)
      if (++i==s) {
         data->root = d;
         return RETURN_OK;
      }

   return RETURN_FAIL;
}

/*  ____  ____   ___   ____ _____ ____ ____
 * |  _ \|  _ \ / _ \ / ___| ____/ ___/ ___|
 * | |_) | |_) | | | | |   |  _| \___ \___ \
 * |  __/|  _ <| |_| | |___| |___ ___) |__) |
 * |_|   |_| \_\\___/ \____|_____|____/____/
 */

static int yaourtprocess(_USR_DATA *data)
{
   return RETURN_OK;
}

static int syncprocess(_USR_DATA *data)
{
   return RETURN_OK;
}

static int upgradeprocess(_USR_DATA *data)
{
   return RETURN_OK;
}

static int removeprocess(_USR_DATA *data)
{
   return RETURN_OK;
}

static int queryprocess(_USR_DATA *data)
{
   return RETURN_OK;
}

static int cleanprocess(_USR_DATA *data)
{
   return RETURN_OK;
}

static int version(_USR_DATA *data)
{
   _Y(); printf("libpndman && milkyhelper\n\n");
   _B(); printf("https://github.com/Cloudef/libpndman\n"); _W();
   printf("~ %s\n", pndman_git_head());
   printf("~ %s\n\n", pndman_git_commit());
   _W(); printf("~ "); _R(); printf("Cloudef"); printf("\n");
   _Y(); printf("<o/ "); _G(); printf("DISCO!\n"); _N();
   return RETURN_OK;
}

static int help(_USR_DATA *data)
{
   _R(); printf("Read the source code for help.\n"); _N();
   return RETURN_OK;
}

/*  _____ _        _    ____ ____
 * |  ___| |      / \  / ___/ ___|
 * | |_  | |     / _ \| |  _\___ \
 * |  _| | |___ / ___ \ |_| |___) |
 * |_|   |_____/_/   \_\____|____/
 */

static int sanitycheck(_USR_DATA *data)
{
   /* expections to sanity checks */
   if ((data->flags & OP_VERSION) || (data->flags & OP_HELP))
      return RETURN_OK;

   /* check target list, NOTE: OP_CLEAN && OP_QUERY can perform without targets */
   if (!data->tlist && (!(data->flags & OP_CLEAN) && !(data->flags & OP_QUERY))) {
      _R(); puts("No targets specified."); _N();
      return RETURN_FAIL;
   } else if (!hasop(data->flags)) data->flags |= OP_YAOURT;

   /* check flags */
   if (!data->flags || !hasop(data->flags)) {
      _R(); puts("No operation specified."); _N();
      return RETURN_FAIL;
   }

   /* no root, ask for it */
   if (!data->root)
      if (rootdialog(data) != RETURN_OK) {
         _R(); puts("\nBad device selected, exiting.."); _N();
         return RETURN_FAIL;
      }

   return RETURN_OK;
}

static int processflags(_USR_DATA *data)
{
   _USR_TARGET *t;
   pndman_repository *r;
   int ret = RETURN_FAIL;

   /* sanity check */
   if (sanitycheck(data) != RETURN_OK)
      return RETURN_FAIL;

   puts("");
   _B();
   if ((data->flags & OP_YAOURT))   puts("::YAOURT");
   if ((data->flags & OP_SYNC))     puts("::SYNC");
   if ((data->flags & OP_UPGRADE))  puts("::UPGRADE");
   if ((data->flags & OP_REMOVE))   puts("::REMOVE");
   if ((data->flags & OP_QUERY))    puts("::QUERY");
   if ((data->flags & OP_CLEAN))    puts("::CLEAN");
   if ((data->flags & OP_VERSION))  puts("::VERSION");
   if ((data->flags & OP_HELP))     puts("::HELP");
   _Y();
   if ((data->flags & GB_FORCE))    puts(";;FORCE");
   _G();
   if ((data->flags & A_SEARCH))    puts("->SEARCH");
   if ((data->flags & A_CATEGORY))  puts("->CATEGORY");
   if ((data->flags & A_INFO))      puts("->INFO");
   if ((data->flags & A_LIST))      puts("->LIST");
   if ((data->flags & A_REFRESH))   puts("->REFRESH");
   if ((data->flags & A_UPGRADE))   puts("->UPGRADE");
   if ((data->flags & A_CRAWL))     puts("->CRAWL");
   if ((data->flags & A_NOSAVE))    puts("->NOSAVE");
   _R();
   if ((data->flags & A_MENU))      puts("[MENU]");
   if ((data->flags & A_DESKTOP))   puts("[DESKTOP]");
   if ((data->flags & A_APPS))      puts("[APPS]");
   _N();
   puts("");

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

   /* logic */
   if ((data->flags & OP_YAOURT))         ret = yaourtprocess(data);
   else if ((data->flags & OP_SYNC))      ret = syncprocess(data);
   else if ((data->flags & OP_UPGRADE))   ret = upgradeprocess(data);
   else if ((data->flags & OP_REMOVE))    ret = removeprocess(data);
   else if ((data->flags & OP_QUERY))     ret = queryprocess(data);
   else if ((data->flags & OP_CLEAN))     ret = cleanprocess(data);
   else if ((data->flags & OP_VERSION))   ret = version(data);
   else if ((data->flags & OP_HELP))      ret = help(data);
   return ret;
}

/*  __  __    _    ___ _   _
 * |  \/  |  / \  |_ _| \ | |
 * | |\/| | / _ \  | ||  \| |
 * | |  | |/ ___ \ | || |\  |
 * |_|  |_/_/   \_\___|_| \_|
 */

int main(int argc, char **argv)
{
   int ret;
   _USR_DATA data;
   _USR_TARGET *t, *tn;

   /* by default, we fail :) */
   ret = EXIT_FAILURE;

   /* init */
   if (pndman_init() != RETURN_OK) {
      _R(); puts("pndman_init failed.."); _N();
      return EXIT_FAILURE;
   }

   /* init userdata */
   init_usrdata(&data);

   /* detect devices */
   if (!(data.dlist = pndman_device_detect(NULL))) {
      _R(); puts("Failed to detect devices.."); _N();
   }

   /* get local repository */
   if (!(data.rlist = pndman_repository_init())) {
      _R(); puts("Failed to initialize local repository."); _N();
   }

   /* do logic, if everything ok! */
   if (data.dlist && data.rlist) {
      /* read configuration */
      if (readconfig("milkycfg", &data) == RETURN_OK) {

         /* parse arguments */
         parseargs(argc, argv, &data);
         ret = processflags(&data) == RETURN_OK ? EXIT_SUCCESS : EXIT_FAILURE;
      }
   }

   /* free repositories */
   pndman_repository_free_all(data.rlist);

   /* free devices */
   pndman_device_free_all(data.dlist);

   /* free targets */
   for (t = data.tlist; t; t = tn)
   { tn = t->next; free(t); }

   /* quit */
   if (pndman_quit() != RETURN_OK) {
      _R(); puts("pndman_quit failed.."); _N();
      return EXIT_FAILURE;
   }

   return ret;
}

/* vim: set ts=8 sw=3 tw=0 :*/
