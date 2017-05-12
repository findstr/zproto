#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zproto.h"

#ifdef	WIN32
#define	EXPORT __declspec(dllexport)
#else
#define	EXPORT
#endif

struct zproto * EXPORT
csload(const char *file)
{
	printf("helloworld:%s\n", file);
	return NULL;
}

struct zproto * EXPORT
csparse(const char *content)
{
	int err = 0;
	char *buff;
	struct zproto *z;
	size_t len = strlen(content);

	z = zproto_create();
	buff = malloc(len + 1);
	memcpy(buff, content, len);
	buff[len] = 0;
	err = zproto_parse(z, buff);
	free(buff);
	if (err < 0) {
		zproto_free(z);
		z = NULL;
	}
	return z;
}

void EXPORT
csfree(struct zproto *z)
{
	zproto_free(z);
	return ;
}

struct zproto_struct * EXPORT
csquery(struct zproto *z, const char *name)
{
	return zproto_query(z, name);
}

int EXPORT
cstag(struct zproto_struct *st)
{
	return zproto_tag(st);
}

struct zproto_struct * EXPORT
csquerytag(struct zproto *z, int tag)
{
	return zproto_querytag(z, tag);
}

int EXPORT
csencode(struct zproto_struct *st, uint8_t *data, int len, zproto_cb_t cb,
		void *obj)
{
	return zproto_encode(st, data, len, cb, obj);
}

int EXPORT
csdecode(struct zproto_struct *st, uint8_t *data, int len, zproto_cb_t cb,
		void *obj)
{
	return zproto_decode(st, data, len, cb, obj);
}

