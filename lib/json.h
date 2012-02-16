#ifndef PNDMAN_JSON_H
#define PNDMAN_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

int _pndman_json_commit(pndman_repository *r, FILE *f);
int _pndman_json_process(pndman_repository *repo, FILE *data);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_JSON_H */

/* vim: set ts=8 sw=3 tw=0 :*/
