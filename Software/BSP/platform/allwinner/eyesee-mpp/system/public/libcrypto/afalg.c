/*
*Test AF_ALG plugin to makesure af_alg ,algif_hash ,ce_sha is ok
*/
#include <sys/socket.h>
#include <linux/if_alg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef AF_ALG
#define AF_ALG 38
#endif
static unsigned char g_abc_sha1[] =
{
	"\xa9\x99\x3e\x36\x47\x06\x81\x6a"
	"\xba\x3e\x25\x71\x78\x50\xc2\x6c"
	"\x9c\xd0\xd8\x9d"
};
int test_af_alg_entry(void)
{
	int opfd = 0;
	int tfmfd = 0;
	struct sockaddr_alg sa = {
	.salg_family = AF_ALG,
	.salg_type = "hash",
	.salg_name = "sha1"
	};
	char buf[20] = {0};
	int i,ret;

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if(tfmfd < 0)
	{
		printf("tfmfd is %d kernl must open NET && support AF_ALG\n",tfmfd);
		return -1;
	}
	bind(tfmfd, (struct sockaddr *)&sa, sizeof(sa));
	opfd = accept(tfmfd, NULL, 0);
	if(opfd < 0)
	{
		printf("opfd is %d must insert alg_if_hash.ko\n",opfd);
		return -1;
	}

	write(opfd, "abc", 3);
	read(opfd, buf, 20);

	for (i = 0; i < 20; i++) {
	printf("%02x", (unsigned char)buf[i]);
	}
	printf("\n");
	if(memcmp(buf, g_abc_sha1, 20) == 0)
	{
		printf("%s af_alg_hash is ok\n",__FUNCTION__);
		ret = 0;
	}
	else
	{
		ret = -1;
		printf("%s af_alg_hash failed\n",__FUNCTION__);
	}
	close(opfd);
	close(tfmfd);
	return ret;
}
