#ifndef PNDMAN_REPOSITORY_H
#define PNDMAN_REPOSITORY_H

#define REPO_URL  LINE_MAX
#define REPO_NAME 24

/* \brief pndman_repository struct */
typedef struct pndman_repository
{
   char url[REPO_URL];
   char name[REPO_NAME];
   char updates[REPO_URL];
   float version;

   pndman_package *pnd;
   struct pndman_repository *next, *prev;

   /* internal */
   int exist;
} pndman_repository;

pndman_repository* _pndman_repository_first(pndman_repository *repo);
pndman_repository* _pndman_repository_last(pndman_repository *repo);

#endif /* PNDMAN_REPOSITORY_H */

/* vim: set ts=8 sw=3 tw=0 :*/
