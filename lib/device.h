#ifndef PNDMAN_DEVICE_H
#define PNDMAN_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pndman_device
{
   char mount[PATH_MAX];
   char device[PATH_MAX];
   size_t size, free, available;

   char appdata[PATH_MAX];
   struct pndman_device *next, *prev;
} pndman_device;


pndman_device* _pndman_device_first(pndman_device *device);
pndman_device* _pndman_device_last(pndman_device *device);

char* _pndman_device_get_appdata(pndman_device *device);
void _pndman_device_get_appdata_no_create(char *appdata, pndman_device *device);

#ifdef __cplusplus
}
#endif

#endif /* PNDMAN_DEVICE_H */

/* vim: set ts=8 sw=3 tw=0 :*/
