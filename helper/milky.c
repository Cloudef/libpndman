#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "pndman.h"

#ifdef WIN32
   #include <windows.h>
#endif

/*
  ____ _____ _   _ _____ ____      _    _
 / ___| ____| \ | | ____|  _ \    / \  | |
| |  _|  _| |  \| |  _| | |_) |  / _ \ | |
| |_| | |___| |\  | |___|  _ <  / ___ \| |___
 \____|_____|_| \_|_____|_| \_\/_/   \_\_____|

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

/*
  ____ ___  _     ___  ____  ____
 / ___/ _ \| |   / _ \|  _ \/ ___|
| |  | | | | |  | | | | |_) \___ \
| |__| |_| | |__| |_| |  _ < ___) |
 \____\___/|_____\___/|_| \_\____/

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

/* ---------------- */

/*
____  _______     _____ ____ _____ ____
|  _ \| ____\ \   / /_ _/ ___| ____/ ___|
| | | |  _|  \ \ / / | | |   |  _| \___ \
| |_| | |___  \ V /  | | |___| |___ ___) |
|____/|_____|  \_/  |___\____|_____|____/

*/

static pndman_device* setroot(char *root, pndman_device *list)
{
   pndman_device *d;
   assert(root);
   for (d = list; d; d = d->next)
      if (!strcmp(root, d->mount) || !strcmp(root, d->device)) return d;
   return NULL;
}

/* ---------------- */

/*
 _____  _    ____   ____ _____ _____ ____
 |_   _|/ \  |  _ \ / ___| ____|_   _/ ___|
   | | / _ \ | |_) | |  _|  _|   | | \___ \
   | |/ ___ \|  _ <| |_| | |___  | |  ___) |
   |_/_/   \_\_| \_\\____|_____| |_| |____/

*/

#define PND_ID 256
typedef struct _USR_TARGET
{
   char                 id[PND_ID];
   pndman_package       *pnd;
   struct _USR_TARGET   *next;
   struct _USR_TARGET   *prev;
} _USR_TARGET;

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

/* ---------------- */

/*
 ____   _    ____  ____  _____
|  _ \ / \  |  _ \/ ___|| ____|
| |_) / _ \ | |_) \___ \|  _|
|  __/ ___ \|  _ < ___) | |___
|_| /_/   \_\_| \_\____/|_____|

__        ______      _    ____  ____  _____ ____
\ \      / /  _ \    / \  |  _ \|  _ \| ____|  _ \
 \ \ /\ / /| |_) |  / _ \ | |_) | |_) |  _| | |_) |
  \ V  V / |  _ <  / ___ \|  __/|  __/| |___|  _ <
   \_/\_/  |_| \_\/_/   \_\_|   |_|   |_____|_| \_\

*/

typedef struct _USRDATA
{
   pndman_device     *dlist;
   pndman_device     *root;
   pndman_repository *rlist;
   _USR_TARGET       *tlist;
} _USRDATA;

static void init_usrdata(_USRDATA *data)
{
   data->dlist = NULL; data->root  = NULL;
   data->rlist = NULL; data->tlist = NULL;
}

static void _addtarget(char *id, void **data)
{
   addtarget(id, &((_USRDATA*)*data)->tlist);
}

static void _setroot(char *root, void **data)
{
   ((_USRDATA*)*data)->root = setroot(root, ((_USRDATA*)*data)->dlist);
}

/*
    _    ____   ____ _   _ __  __ _____ _   _ _____ ____
   / \  |  _ \ / ___| | | |  \/  | ____| \ | |_   _/ ___|
  / _ \ | |_) | |  _| | | | |\/| |  _| |  \| | | | \___ \
 / ___ \|  _ <| |_| | |_| | |  | | |___| |\  | | |  ___) |
/_/   \_\_| \_\\____|\___/|_|  |_|_____|_| \_| |_| |____/

*/

#define _PASSARG(x) { x(narg, data); *skipn = 1; return 0; }
#define _PASSTHS(x) { x(arg, data); return 0; }
static const char* _G_ARG  = "vft";          /* global arguments */
static const char* _OP_ARG = "SURQCVh";      /* operations */
static const char* _S_ARG  = "scilyupmda";   /* sync operation arguments */
static const char* _R_ARG  = "n";            /* remove operation arguments */
static const char* _Q_ARG  = "scilu";        /* query operation arguments */

typedef enum _HELPER_FLAGS
{
   GB_FORCE    = 0x000001, OP_SYNC      = 0x000002,
   OP_UPGRADE  = 0x000004, OP_REMOVE    = 0x000008,
   OP_QUERY    = 0x000010, OP_CLEAN     = 0x000020,
   OP_VERSION  = 0x000040, OP_HELP      = 0x000080,
   A_SEARCH    = 0x000100, A_CATEGORY   = 0x000200,
   A_INFO      = 0x000400, A_LIST       = 0x000800,
   A_REFRESH   = 0x001000, A_UPGRADE    = 0x002000,
   A_CRAWL     = 0x004000, A_MENU       = 0x008000,
   A_DESKTOP   = 0x010000, A_APPS       = 0x020000,
   A_NOSAVE    = 0x040000,
} _HELPER_FLAGS;

static int hasop(unsigned int flags)
{
   return ((flags & OP_SYNC)   || (flags & OP_UPGRADE) ||
           (flags & OP_REMOVE) || (flags & OP_QUERY)   ||
           (flags & OP_CLEAN)  || (flags & OP_VERSION) ||
           (flags & OP_HELP));
}

typedef _HELPER_FLAGS (*_set_func)(char, char*, int*, void**);
static _HELPER_FLAGS getglob(char c, char *arg, int *skipn, void **data)
{
   if (c == 'v')        ++_VERBOSE;
   else if (c == 'f')   return GB_FORCE;
   else if (c == 't')   _USE_COLORS = 0;
   return 0;
}

static _HELPER_FLAGS getop(char c, char *arg, int *skipn, void **data)
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

static _HELPER_FLAGS getsync(char c, char *arg, int *skipn, void **data)
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

static _HELPER_FLAGS getremove(char c, char *arg, int *skipn, void **data)
{
   if (c == 'n') return A_NOSAVE;
   return 0;
}

static _HELPER_FLAGS getquery(char c, char *arg, int *skipn, void **data)
{
   if (c == 's')        return A_SEARCH;
   else if (c == 'c')   return A_CATEGORY;
   else if (c == 'i')   return A_INFO;
   else if (c == 'l')   return A_LIST;
   else if (c == 'u')   return A_UPGRADE;
   return 0;
}

static unsigned int parse(_set_func func, const char *ref, char *arg, char *narg, int *skipn, void **data)
{
   int x, y;
   unsigned int flags = 0;
   for (y = 0; y != strlen(ref); ++y)
      for (x = 0; x != strlen(arg); ++x)
         if (arg[x] == ref[y]) {
            flags |= func(ref[y], narg, skipn, data);
            if (func == getop) return flags; /* only one OP */
         }
   return flags;
}

static unsigned int parsearg(char *arg, char *narg, unsigned int flags, int *skipn, void **data)
{
   if (!strncmp(arg, "-r", 2)) _PASSARG(_setroot);    /* argument with argument */
   if (arg[0] != '-')          _PASSTHS(_addtarget);  /* not argument */
   flags |= parse(getglob, _G_ARG, arg, narg, skipn, data);
   if (!hasop(flags))       flags |= parse(getop, _OP_ARG, arg, narg, skipn, data);
   if ((flags & OP_SYNC))   flags |= parse(getsync, _S_ARG, arg, narg, skipn, data);
   if ((flags & OP_REMOVE)) flags |= parse(getremove, _R_ARG, arg, narg, skipn, data);
   if ((flags & OP_QUERY))  flags |= parse(getquery, _Q_ARG, arg, narg, skipn, data);
   return flags;
}

static unsigned int parseargs(int argc, char **argv, void *data)
{
   int i, skipn = 0;
   unsigned int flags = 0;
   for (i = 1; i != argc; ++i) {
      if (!skipn) flags |= parsearg(argv[i], argc==i+1 ? NULL:argv[i+1], flags, &skipn, &data);
      else skipn = 0;
   }
   return flags;
}

/* ---------------- */

int main(int argc, char **argv)
{
   unsigned int flags = 0;
   _USRDATA data;
   _USR_TARGET *t, *tn;

   /* init */
   if (pndman_init() != RETURN_OK) {
      _R(); puts("pndman_init failed.."); _N();
      return EXIT_FAILURE;
   }

   /* parse arguments */
   init_usrdata(&data);
   flags = parseargs(argc, argv, &data);
   if (!flags) {
      _R(); puts("no flags"); _N();
   } else {
      _B();
      if ((flags & OP_SYNC))     puts("::SYNC");
      if ((flags & OP_UPGRADE))  puts("::UPGRADE");
      if ((flags & OP_REMOVE))   puts("::REMOVE");
      if ((flags & OP_QUERY))    puts("::QUERY");
      if ((flags & OP_CLEAN))    puts("::CLEAN");
      if ((flags & OP_VERSION))  puts("::VERSION");
      if ((flags & OP_HELP))     puts("::HELP");
      _Y();
      if ((flags & GB_FORCE))    puts(";;FORCE");
      _G();
      if ((flags & A_SEARCH))    puts("->SEARCH");
      if ((flags & A_CATEGORY))  puts("->CATEGORY");
      if ((flags & A_INFO))      puts("->INFO");
      if ((flags & A_LIST))      puts("->LIST");
      if ((flags & A_REFRESH))   puts("->REFRESH");
      if ((flags & A_UPGRADE))   puts("->UPGRADE");
      if ((flags & A_CRAWL))     puts("->CRAWL");
      if ((flags & A_NOSAVE))    puts("->NOSAVE");
      _R();
      if ((flags & A_MENU))      puts("[MENU]");
      if ((flags & A_DESKTOP))   puts("[DESKTOP]");
      if ((flags & A_APPS))      puts("[APPS]");
      _N();
   }

   /* list targets */
   _W();
   if (!data.tlist) puts("No targets!");
   else for (t = data.tlist; t; t = t->next) puts(t->id);
   _N();

   /* free targets */
   for (t = data.tlist; t; t = tn)
   { tn = t->next; free(t); }

   /* quit */
   if (pndman_quit() != RETURN_OK) {
      _R(); puts("pndman_quit failed.."); _N();
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
