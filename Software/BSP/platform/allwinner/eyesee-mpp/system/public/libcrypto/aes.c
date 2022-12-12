/*
 * Test AES-ECB/CBC.
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
#include <openssl/aes.h>

#include "ss_test.h"
#define AES_KEY_SIZE_128        16
#define AES_KEY_SIZE_192        24
#define AES_KEY_SIZE_256        32

#define SS_CTR_MODE_ENABLE
#define SS_CTS_MODE_ENABLE
/*#define SS_OFB_MODE_ENABLE*/
/*#define SS_CFB_MODE_ENABLE*/
#ifndef SN_aes_128_cts
/*sync from 1.0.2g, support aes-cts only for aw engines*/
#define SN_aes_128_cts		"AES-128-CTS"
#define LN_aes_128_cts		"aes-128-cts"
#define NID_aes_128_cts		960

#define SN_aes_192_cts		"AES-192-CTS"
#define LN_aes_192_cts		"aes-192-cts"
#define NID_aes_192_cts		961

#define SN_aes_256_cts		"AES-256-CTS"
#define LN_aes_256_cts		"aes-256-cts"
#define NID_aes_256_cts		962
#endif
/*support aes-cts only for aw engines end */

/* The identification string to indicate the key source. */
#define CE_KS_INPUT			"default"
#define CE_KS_SSK			"KEY_SEL_SSK"
#define CE_KS_HUK			"KEY_SEL_HUK"
#define CE_KS_RSSK			"KEY_SEL_RSSK"
#define CE_KS_INTERNAL_0	"KEY_SEL_INTRA_0"
#define CE_KS_INTERNAL_1	"KEY_SEL_INTRA_1"
#define CE_KS_INTERNAL_2	"KEY_SEL_INTRA_2"
#define CE_KS_INTERNAL_3	"KEY_SEL_INTRA_3"
#define CE_KS_INTERNAL_4	"KEY_SEL_INTRA_4"
#define CE_KS_INTERNAL_5	"KEY_SEL_INTRA_5"
#define CE_KS_INTERNAL_6	"KEY_SEL_INTRA_6"
#define CE_KS_INTERNAL_7	"KEY_SEL_INTRA_7"
#define CE_KS_INTERNAL_8	"KEY_SEL_INTRA_8"
#define CE_KS_INTERNAL_9	"KEY_SEL_INTRA_9"
#define CE_KS_INTERNAL_a	"KEY_SEL_INTRA_a"
#define CE_KS_INTERNAL_b	"KEY_SEL_INTRA_b"
#define CE_KS_INTERNAL_c	"KEY_SEL_INTRA_c"
#define CE_KS_INTERNAL_d	"KEY_SEL_INTRA_d"
#define CE_KS_INTERNAL_e	"KEY_SEL_INTRA_e"
#define CE_KS_INTERNAL_f	"KEY_SEL_INTRA_f"

unsigned char g_inbuf[SS_TEST_BUF_SIZE] = {0x30,0x31,0x32};
unsigned char g_inbuf_dst[SS_TEST_BUF_SIZE] = {0};
unsigned char g_outbuf[SS_TEST_BUF_SIZE] = {0};
unsigned char g_key[AES_KEY_SIZE_256] = {
				0xFE, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
				0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
				0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
				0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
unsigned char g_iv[AES_BLOCK_SIZE] = {
				0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
				0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

static ENGINE *openssl_engine_init(void)
{
	ENGINE *e = NULL;
	const char *name = "af_alg";
	char aes_type[] = {"aes-128-ecb aes-192-ecb aes-256-ecb "
					   "aes-128-cbc aes-192-cbc aes-256-cbc "
					   "des-ecb des-cbc des-ede3 des-ede3-cbc "
#ifdef SS_CTR_MODE_ENABLE
					   "aes-128-ctr aes-192-ctr aes-256-ctr "
#endif
#ifdef SS_CTS_MODE_ENABLE
					   "aes-128-cts aes-192-cts aes-256-cts "
#endif
#ifdef SS_CFB_MODE_ENABLE
					   "aes-128-cfb1 aes-192-cfb1 aes-256-cfb1 "
					   "aes-128-cfb8 aes-192-cfb8 aes-256-cfb8 "
					   "aes-128-cfb  aes-192-cfb  aes-256-cfb "
#endif
#ifdef SS_OFB_MODE_ENABLE
					   "aes-128-ofb aes-192-ofb aes-256-ofb"
#endif
					   };
	OpenSSL_add_all_algorithms();
	ENGINE_load_builtin_engines();

	e = ENGINE_by_id(name);
	if (!e) {
		DBG("find engine %s error\n", name);
		return NULL;
	}
	printf("aes_type: %s\n",aes_type);
	if (ENGINE_ctrl_cmd_string(e, "CIPHERS", aes_type, 0) == 0)
		DBG("Failed to register method.\n");
	return e;
}

static void openssl_engine_free(ENGINE *e)
{
	if (e != NULL)
		ENGINE_free(e);

	ENGINE_cleanup();
	EVP_cleanup();
}
#define AES_METHOD_ITEM 64
#define AES_METHOD_ITEM_NAME 32
static unsigned char g_aes_methed[AES_METHOD_ITEM][AES_METHOD_ITEM_NAME] =
{
	"aes-128-ecb",
	"aes-192-ecb",
	"aes-256-ecb",
	"aes-128-cbc",
	"aes-192-cbc",
	"aes-256-cbc",
	"des-ecb",
	"des-cbc",
	"des-ede3",
	"des-ede3-cbc",
#ifdef SS_CTR_MODE_ENABLE
	"aes-128-ctr",
	"aes-192-ctr",
	"aes-256-ctr",
#endif
#ifdef SS_CTS_MODE_ENABLE
	"aes-128-cts",
	"aes-192-cts",
	"aes-256-cts",
#endif
	""
};

int str2nid(char *method)
{
	int i;
	struct {
		int nid;
		char name[AES_METHOD_ITEM_NAME];
	} nids[] = {
			{NID_aes_128_ecb, "aes-128-ecb"},
			{NID_aes_192_ecb, "aes-192-ecb"},
			{NID_aes_256_ecb, "aes-256-ecb"},
			{NID_aes_128_cbc, "aes-128-cbc"},
			{NID_aes_192_cbc, "aes-192-cbc"},
			{NID_aes_256_cbc, "aes-256-cbc"},
			{NID_des_ecb, "des-ecb"},
			{NID_des_cbc, "des-cbc"},
			{NID_des_ede3_ecb, "des-ede3"},
			{NID_des_ede3_cbc, "des-ede3-cbc"},
#ifdef SS_CTR_MODE_ENABLE
			{NID_aes_128_ctr, "aes-128-ctr"},
			{NID_aes_192_ctr, "aes-192-ctr"},
			{NID_aes_256_ctr, "aes-256-ctr"},
#endif
#ifdef SS_CTS_MODE_ENABLE
			{NID_aes_128_cts, "aes-128-cts"},
			{NID_aes_192_cts, "aes-192-cts"},
			{NID_aes_256_cts, "aes-256-cts"},
#endif
#ifdef SS_CFB_MODE_ENABLE
			{NID_aes_128_cfb1, "aes-128-cfb1"},
			{NID_aes_192_cfb1, "aes-192-cfb1"},
			{NID_aes_256_cfb1, "aes-256-cfb1"},
			{NID_aes_128_cfb8, "aes-128-cfb8"},
			{NID_aes_192_cfb8, "aes-192-cfb8"},
			{NID_aes_256_cfb8, "aes-256-cfb8"},
			{NID_aes_128_cfb128, "aes-128-cfb"},
			{NID_aes_192_cfb128, "aes-192-cfb"},
			{NID_aes_256_cfb128, "aes-256-cfb"},
#endif
#ifdef SS_OFB_MODE_ENABLE
			{NID_aes_128_ofb128, "aes-128-ofb"},
			{NID_aes_192_ofb128, "aes-192-ofb"},
			{NID_aes_256_ofb128, "aes-256-ofb"},
#endif
			{0, ""}};

	for (i=0; ;i++) {
		if (nids[i].nid == 0)
			break;

		if (strcmp(method, nids[i].name) == 0)
			return nids[i].nid;
	}

	DBG("Unsupported method: %s\n", method);
	return -1;
}

static int aes_test_item(ENGINE *e, char* aes_name, unsigned char* key,
unsigned char*  iv, int enc, unsigned char *inbuf, unsigned char *outbuf, int len)
{
	int inl = 0;
	int outl = 0;
	EVP_CIPHER_CTX ctx = {0};
	const EVP_CIPHER *e_cipher = NULL;

	e_cipher = ENGINE_get_cipher(e, str2nid(aes_name));
	if (e_cipher == NULL) {
		DBG("Failed to get cipher.\n");
		return -1;
	}

	EVP_CipherInit(&ctx, e_cipher, key, iv, enc);
	EVP_CipherUpdate(&ctx, outbuf, &outl, inbuf, len);
	DBG("outl is %d\n",outl);
	EVP_CipherFinal(&ctx, outbuf + outl, &outl);
	EVP_CIPHER_CTX_cleanup(&ctx);

	return 0;
}

#ifdef __cplusplus
extern "C"{
#endif
int aes_test_entry(int argc, char* argv[]);
#ifdef __cplusplus
}
#endif
#define CE_TEST_PREFIX "CE_AES:"
#define TEST_LEN 64
int aes_test_entry(int argc, char *argv[])
{
    int ret = 0, i = 0;
	ENGINE *e = NULL;
	char* pt[AES_METHOD_ITEM];
	printf("\n%s test begin:\n",CE_TEST_PREFIX);
	e = openssl_engine_init();
	if(!e)
	{
		printf("\n%s test end no engine:\n",CE_TEST_PREFIX);
		return -1;
	}
	for(i = 0; i < AES_METHOD_ITEM; i++)
	{
		pt[i] = (char*)g_aes_methed[i];
		if(strlen(pt[i]) == 0)
		{
			printf("\n%s test end: ret is %d\n",CE_TEST_PREFIX, ret);
			openssl_engine_free(e);
			return 0;
		}
		printf("\n%s%s test begin:\n",CE_TEST_PREFIX,pt[i]);
		aes_test_item(e, pt[i], g_key, g_iv, 1, g_inbuf, g_outbuf,TEST_LEN);
		aes_test_item(e, pt[i], g_key, g_iv, 0, g_outbuf, g_inbuf_dst,TEST_LEN);
		if (memcmp(g_inbuf_dst, g_inbuf, TEST_LEN) == 0)
		{
			printf("\n%s%s test success:\n",CE_TEST_PREFIX,pt[i]);
		}
		else
		{
			ret = -1;
			printf("\n%s%s test failed:\n",CE_TEST_PREFIX,pt[i]);
		}
		memset(g_inbuf_dst, 0, sizeof(g_inbuf_dst));
	}
	openssl_engine_free(e);
	return ret;
}
