#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_set>
#include "test_zproto.hpp"

int main()
{
        std::string dat;
        test_zproto::packet pk;
        test_zproto::packet pk2;
        test_zproto::seralizer *S = new test_zproto::seralizer;
        pk.phone.home = 0x3389;
        pk.phone.work = 0x4498;
        pk.address = "ShangHai";
        pk.luck.push_back(3);
        pk.luck.push_back(7);
        pk.luck.push_back(5);
        int sz = S->encode(pk, dat);
        printf("encode size:%d\n", sz);
        for (int i = 0; i < sz; i++)
                printf("%x ", (uint8_t)dat[i]);
        printf("\n");

        sz = S->decode(pk2, dat);
        printf("decode size:%d\n", sz);
        printf("packet::phone::home:0x%x\n", pk2.phone.home);
        printf("packet::phone::work:0x%x\n", pk2.phone.work);
        printf("packet::address:%s\n", pk2.address.c_str());
        printf("packet::luck size:%lu\n", pk2.luck.size());
        for (size_t i = 0; i < pk2.luck.size(); i++)
                printf("%d ", pk2.luck[i]);
        printf("\r\n");
        delete S;
        return 0;
}

