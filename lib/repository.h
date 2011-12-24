#ifndef PNDMAN_REPOSITORY_H
#define PNDMAN_REPOSITORY_H

typedef struct pndman_repository
{
   const char url[LINE_MAX];
   const char name[LINE_MAX];
   const char updates[LINE_MAX];
   float version;
   pndman_package *pnd;

   struct pndman_repository *next, *prev;

   /* internal */
   int exist;
} pndman_repository;

#endif /* PNDMAN_REPOSITORY_H */
