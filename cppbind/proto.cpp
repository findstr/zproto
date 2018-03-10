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

static inline void
strip_ext(char *path)
{
	char *p = strstr(path, ".zproto");
	if (p != NULL)
		*p = 0;
	return ;
}

static inline std::string
remove_dot(char *path)
{
	char *p = path;
	for (;;) {
		p = strchr(path, '.');
		if (p == NULL)
			break;
		*p = '_';
		p++;
	}
	return path;
}

static inline void
name_space(char *path, std::vector<const char *> &space)
{
	char *p;
	for (;;) {
		p = strchr(path, '.');
		space.push_back(path);
		if (p != NULL)
			*p = 0;
		else
			break;
		path = p + 1;
	}
	return ;
}

int main(int argc, char *argv[])
{
	int err;
	FILE *fp;
	char *proto;
	char *space_buf;
	std::string name;
	struct stat st;
	struct zproto_parser parser;
	std::vector<const char *>space;
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
	err = zproto_parse(&parser, proto);
	if (err < 0) {
		fprintf(stderr, "%s", parser.error);
		delete proto;
		return -1;
	}
	strip_ext(argv[1]);
	space_buf = strdup(argv[1]);
	name = remove_dot(argv[1]);
	name_space(space_buf, space);
	header(name.c_str(), space, parser.z);
	body(name.c_str(), space, proto, parser.z);
	free(space_buf);
	delete []proto;
	zproto_free(parser.z);
	return 0;
}

