#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_set>
#include "test_zproto.hpp"

static void
print_hex(const uint8_t *buf, size_t sz)
{
	for (size_t i = 0; i < sz; i++)
		printf("%x ", buf[i]);
	printf("\n");
}

static void
print_struct(const test_zproto::packet &pk)
{
	for (auto &iter:pk.phone) {
		printf("packet::phone[%f]::home:0x%x\n", iter.first, iter.second.home);
		printf("packet::phone[%f]::work:%f\n", iter.first, iter.second.work);
	}
	printf("packet::address:%s\n", pk.address.c_str());
	printf("packet::luck size:%lu\n", pk.luck.size());
	for (size_t i = 0; i < pk.luck.size(); i++)
		printf("%d ", pk.luck[i]);
	printf("\n");
	for (const auto &iter:pk.address1)
		printf("packet::address1 %s\n", iter.c_str());
	printf("\n");
}

int main()
{
	std::string dat;
	const uint8_t *datbuf;
	int datsize;
	test_zproto::packet pk;
	test_zproto::packet pk2;
	test_zproto::packet pk3;
	pk.phone[1].home = 0x3389;
	pk.phone[1].work = 999.98;
	pk.address = "ShangHai";
	pk.luck.push_back(3);
	pk.luck.push_back(7);
	pk.luck.push_back(5);
	pk.address1.push_back("hello");
	pk.address1.push_back("world");
	int sz = pk._serialize(dat);
	printf("encode1 size:%d\n", sz);
	print_hex((uint8_t *)dat.c_str(), dat.size());
	datsize = pk._serialize(&datbuf);
	printf("encode2 size:%d\n", sz);
	print_hex(datbuf, datsize);
	sz = pk2._parse(dat);
	printf("decode1 size:%d\n", sz);
	print_struct(pk2);
	sz = pk3._parse(datbuf, datsize);
	printf("decode2 size:%d\n", sz);
	print_struct(pk3);
	return 0;
}

