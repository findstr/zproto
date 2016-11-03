#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <limits.h>
#include <string.h>
#include <unordered_set>
#include <sys/stat.h>
#include "zproto.hpp"
#include "header.h"
#include "body.h"

static std::string inline
strip_path(const char *path)
{
	std::string str;
	size_t n = strlen(path);
	char buff[PATH_MAX];
	char *fstart = &buff[PATH_MAX];
	const char *rstart;
	for (rstart= path + n; rstart >= path; rstart--) {
		if (*rstart == '\\' || *rstart == '/') {
			break;
		}
		if (*rstart != '.')
			*(--fstart) = *rstart;
		else
			*(--fstart) = '_';
	}
	return fstart;
}

int main(int argc, char *argv[])
{
	int err;
	FILE *fp;
	char *proto;
	std::string name;
	struct zproto *z;
	struct stat st;
	if (argc < 2) {
		printf("USAGE:%s [*.zproto]\n", argv[0]);
		return 0;
	}

	err = stat(argv[1], &st);
	if (err < 0) {
		perror("file noexist");
		return 0;
	}

	//read files
	proto = new char[st.st_size + 1];
	fp = fopen(argv[1], "rb");
	assert(fp);
	err = fread(proto, st.st_size, 1, fp);
	fclose(fp);
	if (err == 0) {
		delete []proto;
		return 0;
	}
	proto[st.st_size] = 0;

	z = zproto_create();
	err = zproto_parse(z, proto);
	if (err < 0) {
		delete proto;
		zproto_free(z);
		return -1;
	}
	name = strip_path(argv[1]);
	header(name.c_str(), z);
	body(name.c_str(), proto, z);
	delete []proto;
	zproto_free(z);
	return 0;
}

