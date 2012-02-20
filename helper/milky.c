#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "pndman.h"

#ifdef WIN32
   #include <windows.h>
#endif

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
#ifdef __linux__
   printf("\33[31m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_INTENSITY);
#endif
}

static void _G(void)
{
#ifdef __linux__
   printf("\33[32m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN
   |FOREGROUND_INTENSITY);
#endif
}

static void _Y(void)
{
#ifdef __linux__
   printf("\33[33m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN
   |FOREGROUND_RED|FOREGROUND_INTENSITY);
#endif
}

static void _B(void)
{
#ifdef __linux__
   printf("\33[34m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_BLUE
   |FOREGROUND_GREEN|FOREGROUND_INTENSITY);
#endif
}

static void _W(void)
{
#ifdef __linux__
   printf("\33[37m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_GREEN|FOREGROUND_BLUE);
#endif
}

static void _N(void)
{
#ifdef __linux__
   printf("\33[0m");
#else
   HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   SetConsoleTextAttribute(hStdout, FOREGROUND_RED
   |FOREGROUND_GREEN|FOREGROUND_BLUE);
#endif
}

/* ---------------- */

/*
    _    ____   ____ _   _ __  __ _____ _   _ _____ ____
   / \  |  _ \ / ___| | | |  \/  | ____| \ | |_   _/ ___|
  / _ \ | |_) | |  _| | | | |\/| |  _| |  \| | | | \___ \
 / ___ \|  _ <| |_| | |_| | |  | | |___| |\  | | |  ___) |
/_/   \_\_| \_\\____|\___/|_|  |_|_____|_| \_| |_| |____/

*/

static const char* _G_ARG  = "rvf";          /* global arguments */
static const char* _OP_ARG = "SURQCVh";      /* operations */
static const char* _S_ARG  = "scilyupmda";   /* sync operation arguments */
static const char* _R_ARG  = "n";            /* remove operation arguments */
static const char* _Q_ARG  = "scilu";        /* query operation arguments */

typedef enum _HELPER_FLAGS
{
   GB_ROOT     = 0x000001, GB_VERBOSE   = 0x000002,
   GB_FORCE    = 0x000004, OP_SYNC      = 0x000008,
   OP_UPGRADE  = 0x000010, OP_REMOVE    = 0x000020,
   OP_QUERY    = 0x000040, OP_CLEAN     = 0x000040,
   OP_VERSION  = 0x000100, OP_HELP      = 0x000080,
   A_SEARCH    = 0x000400, A_CATEGORY   = 0x000800,
   A_INFO      = 0x001000, A_LIST       = 0x002000,
   A_REFRESH   = 0x004000, A_UPGRADE    = 0x008000,
   A_CRAWL     = 0x010000, A_MENU       = 0x020000,
   A_DESKTOP   = 0x040000, A_APPS       = 0x080000,
   A_NOSAVE    = 0x100000,
} _HELPER_FLAGS;

static int hasop(unsigned int flags)
{
   return ((flags & OP_SYNC)   || (flags & OP_UPGRADE) ||
           (flags & OP_REMOVE) || (flags & OP_QUERY)   ||
           (flags & OP_CLEAN)  || (flags & OP_VERSION) ||
           (flags & OP_HELP));
}

typedef _HELPER_FLAGS (*_set_func)(char);
static _HELPER_FLAGS getglob(char c)
{
   if (c == 'r')        return GB_ROOT;
   else if (c == 'v')   return GB_VERBOSE;
   else if (c == 'f')   return GB_FORCE;
   return 0;
}

static _HELPER_FLAGS getop(char c)
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

static _HELPER_FLAGS getsync(char c)
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

static _HELPER_FLAGS getremove(char c)
{
   if (c == 'n') return A_NOSAVE;
   return 0;
}

static _HELPER_FLAGS getquery(char c)
{
   if (c == 's')        return A_SEARCH;
   else if (c == 'c')   return A_CATEGORY;
   else if (c == 'i')   return A_INFO;
   else if (c == 'l')   return A_LIST;
   else if (c == 'u')   return A_UPGRADE;
   return 0;
}

static unsigned int parse(_set_func func, const char *ref, char *arg)
{
   int x, y;
   unsigned int flags = 0;
   for (y = 0; y != strlen(ref); ++y)
      for (x = 0; x != strlen(arg); ++x)
         if (arg[x] == ref[y]) {
            flags |= func(ref[y]);
            if (func == getop) return flags; /* only one OP */
         }
   return flags;
}

static unsigned int parsearg(char *arg, unsigned int flags)
{
   /* not a argument */
   if (arg[0] != '-') return 0;
   flags |= parse(getglob, _G_ARG, arg);
   if (!hasop(flags))       flags |= parse(getop, _OP_ARG, arg);
   if ((flags & OP_SYNC))   flags |= parse(getsync, _S_ARG, arg);
   if ((flags & OP_REMOVE)) flags |= parse(getremove, _R_ARG, arg);
   if ((flags & OP_QUERY))  flags |= parse(getquery, _Q_ARG, arg);
   return flags;
}

static unsigned int parseargs(int argc, char **argv)
{
   int i;
   unsigned int flags = 0;
   for (i = 0; i != argc; ++i) flags |= parsearg(argv[i], flags);
   return flags;
}

/* ---------------- */

int main(int argc, char **argv)
{
   unsigned int flags = 0;

   /* init */
   if (pndman_init() != RETURN_OK) {
      _R(); puts("pndman_init failed.."); _N();
      return EXIT_FAILURE;
   }

   /* parse arguments */
   flags = parseargs(argc, argv);
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
      if ((flags & GB_ROOT))     puts(";;ROOT");
      if ((flags & GB_VERBOSE))  puts(";;VERBOSE");
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

   /* quit */
   if (pndman_quit() != RETURN_OK) {
      _R(); puts("pndman_quit failed.."); _N();
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=3 tw=0 :*/
