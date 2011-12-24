#ifndef PNDMAN_DEVICE_H
#define PNDMAN_DEVICE_H

typedef struct pndman_device
{
   char mount[PATH_MAX];
   char device[PATH_MAX];
   size_t size, free, available;

   struct pndman_device *next, *prev;

   /* internal */
   int exist;
} pndman_device;

#endif /* PNDMAN_DEVICE_H */
