/*
 * Test SHA.
 *
 * Copyright (C) 2013 Allwinner.
 *
 * Mintow <duanmintao@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * 2013-09-08, mintow, create.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/engine.h>
#include <openssl/sha.h>

#include "ss_test.h"
#define CE_TEST_PREFIX "CE_SHA:"
char g_buf[SS_TEST_BUF_SIZE] = {0};
#define SHA_METHOD_ITEM 8
#define SHA_METHOD_ITEM_NAME 10

unsigned char g_sha_methed[SHA_METHOD_ITEM][SHA_METHOD_ITEM_NAME] =
{
	"md5",
	"sha1",
	"sha224",
	"sha256",
	"sha384",
	"sha512",
	""
};
void print_md(unsigned char *md, int len)
{
	int i;
	for (i=0; i<len; i++)
		printf("%02x", md[i]);
	printf("\n");
}

ENGINE *openssl_engine_init(char *type)
{
	ENGINE *e = NULL;
	const char *name = "af_alg";
	OpenSSL_add_all_algorithms();
	ENGINE_load_builtin_engines();

	e = ENGINE_by_id(name);
	if (!e) {
		DBG("find engine %s error\n", name);
		return NULL;
	}

	ENGINE_ctrl_cmd_string(e, "DIGESTS", type, 0);
	return e;
}

static void openssl_engine_free(ENGINE *e)
{
	if (e != NULL)
		ENGINE_free(e);

	ENGINE_cleanup();
	EVP_cleanup();
}

static const char g_test_string[17] = "0123456789abcdef";
int sha_test_entry(int argc, char *argv[])
{
    int ret = 0;
	int nid = NID_sha256;
	ENGINE *e = NULL;
	int hash_len = 32;
	EVP_MD_CTX ctx = {0};
	const EVP_MD *e_md = NULL;
	unsigned int md_size = 0;
	unsigned char md[128] = {0};
	char* pt[SHA_METHOD_ITEM];
	unsigned int i = 0;
	size_t wt_len;
	for(i = 0; i < sizeof(g_sha_methed)/sizeof(g_sha_methed[0]); i++)
	{
		pt[i] = (char*)g_sha_methed[i];
		printf("\n%s%s test begin:\n",CE_TEST_PREFIX,pt[i]);
		if(strlen(pt[i]) == 0)
		{
			ret = 0;
			break;
		}
		e = openssl_engine_init(pt[i]);
		if (e == NULL) {
			ret = -1;
			break;
		}
		if (strcmp(pt[i], "sha1") == 0)
		{
			nid = NID_sha1;
			hash_len = 20;
		}
		else if (strcmp(pt[i], "md5") == 0)
		{
			nid = NID_md5;
			hash_len = 16;
		}
		else if (strcmp(pt[i], "sha224") == 0)
		{
			nid = NID_sha224;
			hash_len = 28;
		}
		else if (strcmp(pt[i], "sha384") == 0)
		{
			nid = NID_sha384;
			hash_len = 48;
		}
		else if (strcmp(pt[i], "sha512") == 0)
		{
			nid = NID_sha512;
			hash_len = 64;
		}
		e_md = ENGINE_get_digest(e, nid);
		if (e_md == NULL) {
			DBG("ENGINE_get_digest() failed! e_af_alg not support \n");
			ret = 0;
			break;
		}

		EVP_DigestInit(&ctx, e_md);
		EVP_DigestUpdate(&ctx, g_test_string, sizeof(g_test_string));
		EVP_DigestFinal(&ctx, md, &md_size);

		print_md(md, hash_len);
		EVP_MD_CTX_cleanup(&ctx);
		openssl_engine_free(e);
		printf("\n%s%s test end:\n",CE_TEST_PREFIX,pt[i]);
	}
	printf("\n%s test end result:%d\n",CE_TEST_PREFIX,ret);
    return ret;
}

#define SS_RAND_BYTES_MAX 32
int aw_rand_bytes(unsigned char* buf,unsigned int num)
{
	ENGINE *e = NULL;
	const RAND_METHOD* rnd_md =NULL;
	const char *name = "af_alg";
	EVP_MD_CTX ctx = {0};
	OpenSSL_add_all_algorithms();
	ENGINE_load_builtin_engines();
	if(num > SS_RAND_BYTES_MAX)
	{
		return 0;
	}
	e = ENGINE_by_id(name);
	if (!(e && buf)) {
		DBG("find engine %s error\n", name);
		return -1;
	}
	ENGINE_ctrl_cmd_string(e, "RAND", 0, 0);
	rnd_md = ENGINE_get_RAND(e);
	if (rnd_md && rnd_md->bytes)
	if(1 == rnd_md->bytes(buf, num))
	{
		return num;
	}
	return(-1);
}
