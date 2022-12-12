/*
 *
 * Copyright(c) 2017-2020 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 */
#include <stdio.h>
#include <stdlib.h>

#define sys_err(format, arg...) printf("Error: "format, ##arg );
#define sys_info(format,arg...)	printf(format,##arg)

extern void osl_rsa_test(void);
extern void hmac_sha256_sw_test(int argc, char* argv[]);
extern int aes_test_entry(int argc, char* argv[]);
extern int sha_test_entry(int argc, char *argv[]);
extern int test_af_alg_entry(void);
extern int aw_rand_bytes(unsigned char* buf,unsigned int num);
extern int hmac_sha1_entry(int argc, char *argv[]);
extern int hmac_sha256_entry(int argc, char *argv[]);
extern int aw_cipher_sha_entry(int argc, char* argv[]);
extern int aw_cipher_entry(int argc, char* argv[]);
extern int rsa_entry();


static unsigned char g_rand_buf[256];
int main(void)
{
	int i = 0;
	int outlen;
	sys_info("===========af alg begin===========\n");
	test_af_alg_entry();
	sys_info("===========af alg end===========\n");
	sys_info("===========sw openssl hmac begin===========\n");
	hmac_sha256_sw_test(1,0);
	sys_info("===========sw openssl hmac end===========\n");
	sys_info("=========== openssl_ce sha test begin ===========\n");
	sha_test_entry(0,NULL);
	sys_info("=========== openssl_ce sha test end ===========\n");
	sys_info("=========== openssl_ce aes test begin ===========\n");
	aes_test_entry(0,NULL);
	sys_info("=========== openssl_ce  aes test end ===========\n");
	hmac_sha1_entry(0,NULL);
	hmac_sha256_entry(0,NULL);
	outlen = aw_rand_bytes(g_rand_buf,20);
	printf("\n===========aw_rand_bytes:===========\n");
	for(i = 0; i < outlen; i++)
	printf("0x%2x ",g_rand_buf[i]);
	printf("\n");

	aw_cipher_sha_entry(0, NULL);
	rsa_entry();
	aw_cipher_entry(0, NULL);
	return 0;
}
