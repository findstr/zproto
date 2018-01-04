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
cszload(const char *file)
{
	printf("helloworld:%s\n", file);
	return NULL;
}

struct zproto * EXPORT
cszparse(const char *content)
{
	struct zproto_parser parser;
	zproto_parse(&parser, content);
	return parser.z;
}

void EXPORT
cszfree(struct zproto *z)
{
	zproto_free(z);
	return ;
}

struct zproto_struct * EXPORT
cszquery(struct zproto *z, const char *name)
{
	return zproto_query(z, name);
}

int EXPORT
csztag(struct zproto_struct *st)
{
	return zproto_tag(st);
}

struct zproto_struct * EXPORT
cszquerytag(struct zproto *z, int tag)
{
	return zproto_querytag(z, tag);
}

int EXPORT
cszencode(struct zproto_struct *st, uint8_t *data, int len, zproto_cb_t cb,
		void *obj)
{
	return zproto_encode(st, data, len, cb, obj);
}

int EXPORT
cszdecode(struct zproto_struct *st, uint8_t *data, int len, zproto_cb_t cb,
		void *obj)
{
	return zproto_decode(st, data, len, cb, obj);
}

int EXPORT
cszpack(const uint8_t *src, int srcsz, uint8_t *dst, int dstsz)
{
	return zproto_pack(src, srcsz, dst, dstsz);
}

int EXPORT
cszunpack(const uint8_t *src, int srcsz, uint8_t *dst, int dstsz)
{
	return zproto_unpack(src, srcsz, dst, dstsz);
}


