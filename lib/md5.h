#ifndef PNDMAN_MD5_H
#define PNDMAN_MD5_H

#ifdef __cplusplus
extern "C" {
#endif

/* \brief get md5 from char buffer */
char* _pndman_md5_buf(char *buffer, size_t size);

/* \brief get md5 of file, remember to free the result */
char* _pndman_md5(char *file);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_MD5_H */

/* vim: set ts=8 sw=3 tw=0 :*/
