#include "internal.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <openssl/md5.h>

static int _pndman_md5file(FILE *file, unsigned char *digest)
{
   unsigned char buf[512];
   MD5_CTX c;
   size_t len;

   MD5_Init(&c);
   while(len = fread(buf, sizeof(buf[0]), sizeof(buf), file))
      MD5_Update(&c, buf, len);
   MD5_Final(digest, &c);
   return ferror(file);
}

static char _pndman_nibble2hex(unsigned char b)
{
    return (b > 9 ? b - 10 + 'a' : b + '0');
}

static size_t _pndman_bytes2hex(const unsigned char *bytes, size_t numBytes,
                 char *hex, size_t hexSize)
{
   const unsigned char * bytesStart = bytes;
   const unsigned char * bytesEnd = bytes + numBytes;
   char * hexEnd = hex + hexSize;

   while((hex + 2) < hexEnd && bytes < bytesEnd)
   {
      unsigned char b = *bytes++;
      *hex++ = _pndman_nibble2hex((b >> 4) & 0x0F);
      *hex++ = _pndman_nibble2hex(b & 0x0F);
   }
   if (hex < hexEnd)
      *hex++ = '\0';
   return bytes - bytesStart;
}

/* \brief get md5 from char buffer */
char* _pndman_md5_buf(char *buffer, size_t size)
{
   MD5_CTX c;
   char *md5;
   unsigned char digest[MD5_DIGEST_LENGTH];

   MD5_Init(&c);
   MD5_Update(&c, buffer, size);
   MD5_Final(digest, &c);

   md5 = malloc(MD5_DIGEST_LENGTH * 2 + 1);
   if (!md5) return NULL;

   memset(md5, 0, MD5_DIGEST_LENGTH * 2 + 1);
   _pndman_bytes2hex(digest, MD5_DIGEST_LENGTH, md5, MD5_DIGEST_LENGTH * 2 + 1);
   return md5;
}

/* \brief get md5 of file, remember to free the result */
char* _pndman_md5(const char *file)
{
   FILE *f;
   char *md5;
   unsigned char digest[MD5_DIGEST_LENGTH];

   if (!(f = fopen(file, "rb")))
      goto fail;

   if (_pndman_md5file(f, digest) != 0)
      goto fail;
   fclose(f);

   if (!(md5 = malloc(MD5_DIGEST_LENGTH * 2 + 1)))
      goto fail;

   memset(md5, 0, MD5_DIGEST_LENGTH * 2 + 1);
   _pndman_bytes2hex(digest, MD5_DIGEST_LENGTH, md5, MD5_DIGEST_LENGTH * 2 + 1);
   return md5;

fail:
   IFDO(fclose, f);
   return NULL;
}

/* vim: set ts=8 sw=3 tw=0 :*/
