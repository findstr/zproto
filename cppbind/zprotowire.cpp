#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "zproto.hpp"
#include "zprotowire.h"

namespace zprotobuf {

#define BUFFSZ  (64)

static int
encode_cb(struct zproto_args *arg)
{
        wire *w = (wire *)arg->ud;
        return w->_encode_proxy(arg);
}

static int
decode_cb(struct zproto_args *arg)
{
        wire *w = (wire *)arg->ud;
        return w->_decode_proxy(arg);
}

int 
wire::_encode(uint8_t *buff, int buffsz, struct zproto_struct *st) const
{
        return zproto_encode(st, buff, buffsz, encode_cb, (void *)this);
}

int
wire::_decode(uint8_t *buff, int buffsz, struct zproto_struct *st)
{
        return zproto_decode(st, buff, buffsz, decode_cb, (void *)this);
}

int
wire::_encode_proxy(struct zproto_args *args) const
{
        return _encode_field(args);
}

int
wire::_decode_proxy(struct zproto_args *args)
{
        return _decode_field(args);
}

//syntax tree

wiretree::wiretree(const char *proto)
        :protodef(proto)
{
        int err;
        z = zproto_create();
        err = zproto_parse(z, protodef);
        assert(err == 0);
        buff = new uint8_t[BUFFSZ];
        buffsz = BUFFSZ;
}

wiretree::~wiretree()
{
        zproto_free(z);
        delete buff;
}


void
wiretree::expand()
{
        size_t newsz = buffsz * 2;
        uint8_t *newbuff = new uint8_t[newsz];
        memcpy(newbuff, buff, buffsz);
        delete buff;
        buffsz = newsz;
        buff = newbuff;
        return ;
}

struct zproto_struct *
wiretree::query(const char *name)
{
        struct zproto_struct *st;
        intptr_t k = (intptr_t)name;
        st = cache[k];
        if (st)
                return st;
        st = zproto_query(z, name);
        assert(st);
        cache[k] = st;
        return st;
}

int
wiretree::encode(const wire &w, std::string &dat)
{
        int sz;
        struct zproto_struct *st = query(w._name());
        for (;;) {
                sz = w._encode(buff, buffsz, st);
                if (sz == ZPROTO_ERROR)
                        return sz;
                if (sz == ZPROTO_OOM) {
                        expand();
                        continue;
                }
                dat.assign((char *)buff, sz);
                return sz;
        }
        //never come here
        return ZPROTO_ERROR;
}

int
wiretree::decode(wire &w, const std::string &dat)
{
        int sz;
        uint8_t *data = (uint8_t *)dat.data();
        int datasz = dat.size();
        struct zproto_struct *st = query(w._name());
        sz = w._decode(data, datasz, st);
        return sz;
}

}
