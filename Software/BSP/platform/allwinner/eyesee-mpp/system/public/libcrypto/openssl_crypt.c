/*
 * Copyright(c) 2000-2999 Allwinnertech Co., Ltd.
 *
 *		http://www.allwinnertech.com
 *
 *
 *
 *
 */
#include <stdio.h>
#include <string.h>

#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/hmac.h>

#define sys_info(format,arg...)	printf(format,##arg)
#define sys_err(format,arg...)	printf(format,##arg)

extern void dump_buffer(void *buf, int len);

#define AES_PER_BLK (1024)
#define AES128_KEY_LENGTH (16)

#define u32 unsigned int
#define u16 unsigned short
#define u8	unsigned char
#define s32 int
#define s16 short
#define s8  char

/*
 * Use default openssl engine
 */
int openssl_encrypt(u8 *in, u8 *out, u8 *key, u32 key_len, u32 data_len)
{

	EVP_CIPHER_CTX ctx;
	int outl,outltmp;
	int ret ;
	u8 tmp[32];

	EVP_CIPHER_CTX_init(&ctx);
	if( key_len == AES128_KEY_LENGTH ){
		sys_info("aes 128\n");
		ret = EVP_EncryptInit_ex(&ctx,EVP_aes_128_ecb(),NULL,key,NULL);
		if( ret != 1 ){
			sys_err(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}else{
		ret = EVP_EncryptInit_ex(&ctx,EVP_aes_256_ecb(),NULL,key,NULL);
		if( ret != 1 ){
			sys_err(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}

	EVP_CIPHER_CTX_set_padding(&ctx, 0);

	int rest = data_len, per_len ;
	unsigned char *inp = in, *outp = out;
	for ( ;  rest> 0; inp += outl, outp+=outl, rest -= outl){
		per_len = (rest> AES_PER_BLK)? AES_PER_BLK : rest ;
		ret=EVP_EncryptUpdate(&ctx,outp,&outl,(const unsigned char *)inp,per_len);
		if( ret != 1 ) {
			sys_err(" Encryption update fail\n");
			return -1 ;
		}
	}

	ret=EVP_EncryptFinal_ex(&ctx,tmp,&outltmp);


	sys_info("ciphertext update length: %d, outl %d\n", data_len,outltmp);

	sys_info("key buffer\n");
	dump_buffer(key,key_len);
	sys_info("key string %s\n",key);

	sys_info("in buffer\n");
	dump_buffer(in,32);

	sys_info("out buffer\n");
	dump_buffer(out,32);

	sys_info("padding buffer\n");
	dump_buffer(tmp,outltmp);

	EVP_CIPHER_CTX_cleanup(&ctx);
	return 0 ;
}

int openssl_decrypt(u8 *in, u8 *out, u8 *key, u32 key_len, u32 data_len)
{

	EVP_CIPHER_CTX ctx;
	int outl,outltmp;
	int ret ;
	u8 tmp[32];

	EVP_CIPHER_CTX_init(&ctx);
	if( key_len == AES128_KEY_LENGTH ){
		sys_info("aes 128\n");
		ret = EVP_DecryptInit_ex(&ctx,EVP_aes_128_ecb(),NULL,key,NULL);
		if( ret != 1 ){
			sys_err(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}else{
		ret = EVP_DecryptInit_ex(&ctx,EVP_aes_256_ecb(),NULL,key,NULL);
		if( ret != 1 ){
			sys_err(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}

	EVP_CIPHER_CTX_set_padding(&ctx, 0);

	int rest = data_len, per_len ;
	unsigned char *inp = in, *outp = out;
	for ( ;  rest> 0; inp += outl, outp+=outl, rest -= outl){
		per_len = (rest> AES_PER_BLK)? AES_PER_BLK : rest ;
		ret=EVP_DecryptUpdate(&ctx,outp,&outl,(const unsigned char *)inp,per_len);
		if( ret != 1 ) {
			sys_err(" Encryption update fail\n");
			return -1 ;
		}
	}

	EVP_DecryptFinal_ex(&ctx,tmp,&outltmp);

	sys_info("ciphertext update length: %d, outl %d\n", data_len,outltmp);

	sys_info("key buffer\n");
	dump_buffer(key,key_len);
	sys_info("key string %s\n",key);

	sys_info("in buffer\n");
	dump_buffer(in,32);

	sys_info("out buffer\n");
	dump_buffer(out,32);

	sys_info("padding buffer\n");
	dump_buffer(tmp,outltmp);

	EVP_CIPHER_CTX_cleanup(&ctx);
	return 0 ;
}

#define RSA_PK_N_MAX 512
#define RSA_PK_E_MAX 512

/*return value: the outlen of decrypted data*/
int openssl_rsa_pub_dec(u8 *in, u32 inlen,
u8 *out, u8 *pk_n, u32 n_sz,u8 *pk_e, u32 e_sz)
{
	u8 *n = NULL;
	u8 *e = NULL;
	int outlen = 0;
    RSA *key = NULL;
	key = RSA_new();
	n = malloc(RSA_PK_N_MAX + 1);
	e = malloc(RSA_PK_E_MAX);
	int cp_size = n_sz;

	if (!(in&&out&&pk_n))
	{
		printf("%d \n",__LINE__);
		goto errout;
	}

	if (!(key&&n&&e))
	{
		printf("%d \n",__LINE__);
		goto errout;
	}
	memset(n, 0, RSA_PK_N_MAX);
	if(pk_n[0] >= 0x80)
	{
		memcpy(n+1,pk_n,n_sz);
		cp_size++;
	}
	else
	{
		memcpy(n, pk_n, n_sz);
	}

	memset(e, 0, RSA_PK_E_MAX);
	memcpy(e, pk_e, e_sz);
	sys_info("e is:\n");
	dump_buffer(e,e_sz);

	sys_info("cp_size is %d key->n is: \n",cp_size);
	dump_buffer(n,cp_size);

	key->n = BN_bin2bn(n, cp_size, key->n);
	key->e = BN_bin2bn(e, e_sz, key->e);

	/*the below is for private keys,
	only fake value filled :because for pk_enc it is useless
	key->d = BN_bin2bn(n, cp_size, key->d);
	key->p = BN_bin2bn(n, cp_size, key->p);
	key->q = BN_bin2bn(n, cp_size, key->q);
	key->dmp1 = BN_bin2bn(n, cp_size, key->dmp1);
	key->dmq1 = BN_bin2bn(n, cp_size, key->dmq1);
	key->iqmp = BN_bin2bn(n, cp_size, key->iqmp);
	*/
	printf("RSA_public_decrypt begin %d \n",__LINE__);
	printf("inlen: %d \n",inlen);
    outlen = RSA_public_decrypt(inlen, in, out, key,
                                 RSA_PKCS1_PADDING);
	printf("RSA_public_decrypt end %d \n \noutlen is %d \n\n",__LINE__, outlen);
errout:
	if(n)
		free(n);
	if(e)
		free(e);
	if(key)
		RSA_free(key);
	return outlen;
}

# define SetKey \
  key->n = BN_bin2bn(n, sizeof(n)-1, key->n); \
  key->e = BN_bin2bn(e, sizeof(e)-1, key->e); \
  key->d = BN_bin2bn(d, sizeof(d)-1, key->d); \
  key->p = BN_bin2bn(p, sizeof(p)-1, key->p); \
  key->q = BN_bin2bn(q, sizeof(q)-1, key->q); \
  key->dmp1 = BN_bin2bn(dmp1, sizeof(dmp1)-1, key->dmp1); \
  key->dmq1 = BN_bin2bn(dmq1, sizeof(dmq1)-1, key->dmq1); \
  key->iqmp = BN_bin2bn(iqmp, sizeof(iqmp)-1, key->iqmp); \
  memcpy(c, ctext_ex, sizeof(ctext_ex) - 1); \
  return (sizeof(ctext_ex) - 1);

static int key1(RSA *key, unsigned char *c)
{
    static unsigned char n[] =
        "\x00\xAA\x36\xAB\xCE\x88\xAC\xFD\xFF\x55\x52\x3C\x7F\xC4\x52\x3F"
        "\x90\xEF\xA0\x0D\xF3\x77\x4A\x25\x9F\x2E\x62\xB4\xC5\xD9\x9C\xB5"
        "\xAD\xB3\x00\xA0\x28\x5E\x53\x01\x93\x0E\x0C\x70\xFB\x68\x76\x93"
        "\x9C\xE6\x16\xCE\x62\x4A\x11\xE0\x08\x6D\x34\x1E\xBC\xAC\xA0\xA1"
        "\xF5";

    static unsigned char e[] = "\x11";

    static unsigned char d[] =
        "\x0A\x03\x37\x48\x62\x64\x87\x69\x5F\x5F\x30\xBC\x38\xB9\x8B\x44"
        "\xC2\xCD\x2D\xFF\x43\x40\x98\xCD\x20\xD8\xA1\x38\xD0\x90\xBF\x64"
        "\x79\x7C\x3F\xA7\xA2\xCD\xCB\x3C\xD1\xE0\xBD\xBA\x26\x54\xB4\xF9"
        "\xDF\x8E\x8A\xE5\x9D\x73\x3D\x9F\x33\xB3\x01\x62\x4A\xFD\x1D\x51";

    static unsigned char p[] =
        "\x00\xD8\x40\xB4\x16\x66\xB4\x2E\x92\xEA\x0D\xA3\xB4\x32\x04\xB5"
        "\xCF\xCE\x33\x52\x52\x4D\x04\x16\xA5\xA4\x41\xE7\x00\xAF\x46\x12"
        "\x0D";

    static unsigned char q[] =
        "\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
        "\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5A\x0F\x20\x35\x02\x8B\x9D"
        "\x89";

    static unsigned char dmp1[] =
        "\x59\x0B\x95\x72\xA2\xC2\xA9\xC4\x06\x05\x9D\xC2\xAB\x2F\x1D\xAF"
        "\xEB\x7E\x8B\x4F\x10\xA7\x54\x9E\x8E\xED\xF5\xB4\xFC\xE0\x9E\x05";

    static unsigned char dmq1[] =
        "\x00\x8E\x3C\x05\x21\xFE\x15\xE0\xEA\x06\xA3\x6F\xF0\xF1\x0C\x99"
        "\x52\xC3\x5B\x7A\x75\x14\xFD\x32\x38\xB8\x0A\xAD\x52\x98\x62\x8D"
        "\x51";

    static unsigned char iqmp[] =
        "\x36\x3F\xF7\x18\x9D\xA8\xE9\x0B\x1D\x34\x1F\x71\xD0\x9B\x76\xA8"
        "\xA9\x43\xE1\x1D\x10\xB2\x4D\x24\x9F\x2D\xEA\xFE\xF8\x0C\x18\x26";

    static unsigned char ctext_ex[] =
        "\x1b\x8f\x05\xf9\xca\x1a\x79\x52\x6e\x53\xf3\xcc\x51\x4f\xdb\x89"
        "\x2b\xfb\x91\x93\x23\x1e\x78\xb9\x92\xe6\x8d\x50\xa4\x80\xcb\x52"
        "\x33\x89\x5c\x74\x95\x8d\x5d\x02\xab\x8c\x0f\xd0\x40\xeb\x58\x44"
        "\xb0\x05\xc3\x9e\xd8\x27\x4a\x9d\xbf\xa8\x06\x71\x40\x94\x39\xd2";
	SetKey;

}

static const char rnd_seed[] =
    "string to make the random number generator think it has entropy";

void osl_rsa_test(void)
{
    RSA *key;
    unsigned char ptext[256];
    unsigned char ctext[256];
    static unsigned char ptext_ex[] = "\x54\x85\x9b\x34\x2c\x49\xea\x2a";
    unsigned char ctext_ex[256];
	int clen,plen,num;

	CRYPTO_malloc_debug_init();
    CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
	RAND_seed(rnd_seed, sizeof rnd_seed);
	key = RSA_new();
	clen = key1(key, ctext_ex);
	plen = sizeof(ptext_ex) - 1;

	num = RSA_public_encrypt(plen, ptext_ex, ctext, key,
                                 RSA_PKCS1_PADDING);
    if (num != clen) {
    sys_info("PKCS#1 v1.5 encryption failed!\n");
	}

	dump_buffer(ctext,num);
	dump_buffer(ctext_ex,num);

    num = RSA_private_decrypt(num, ctext, ptext, key, RSA_PKCS1_PADDING);
    if (num != plen || memcmp(ptext, ptext_ex, num) != 0) {
        sys_info("PKCS#1 v1.5 decryption failed!\n");
    } else
        sys_info("PKCS #1 v1.5 encryption/decryption ok\n");

}


static struct test_st {
    unsigned char key[64];
    int key_len;
    unsigned char data[64];
    int data_len;
    unsigned char digest[33];
} test[] = {
	{
				.key	=
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					,
				.key_len   = 64,
				.data =
				    "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
					"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
				.data_len	= 64,
				.digest = "\xf1\x27\xf4\x01\xa6\x6d\x9d\x6f"
					  "\xbc\xf0\x7e\x0a\x15\x88\x8a\x91"
					  "\xad\x7a\xe6\xdb\x6b\x36\xb4\x90"
					  "\x64\xe6\x38\xe8\x7b\x3e\x96\x93",
	},
	{
		.key =
        "Jefe",
        .key_len = 4,
        .data = "what do ya want for nothing?",
        .data_len= 28,
        .digest = "\x5b\xdc\xc1\x46\xbf\x60\x75\x4e"
				  "\x6a\x04\x24\x26\x08\x95\x75\xc7"
				  "\x5a\x00\x3f\x08\x9d\x27\x39\x83"
				  "\x9d\xec\x58\xb9\x64\xec\x38\x43",
    }


};

static void xx_dump(char * str,unsigned char* data,int len,int align)
{
	int i = 0;
	printf("\n%s: ",str);
	for(i = 0;i<len;i++)
	{
		if((i%align) == 0)
		{
			printf("\n");
		}
		printf("%x ",*(data++));

	}
	printf("\n");
}

static char *pt(unsigned char *md, unsigned int len)
{
    unsigned int i;
    static char buf[80];

    for (i = 0; i < len; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return (buf);
}

void hmac_sha256_sw_test(int argc, char* argv[])
{
	char * p;
	int i = 1 , j;
	unsigned char md[64];
	unsigned int md_len = 0;

	for (i = 0; i < sizeof(test)/sizeof(test[0]); i++)
	{
	    p = pt(HMAC(EVP_sha256(),
                test[i].key, test[i].key_len,
                test[i].data, test[i].data_len, (unsigned char*)md,
                &md_len), 32);

	    if (memcmp(md, (char *)test[i].digest, 32) != 0) {
			printf("md_len is %d\n",md_len);
			xx_dump("hmac result:",(unsigned char *)test[i].digest,32,16);
			xx_dump("md result:",md, 32, 16);
	    } else
	        printf("hmac test item%d ok\n", i);
	}

}
