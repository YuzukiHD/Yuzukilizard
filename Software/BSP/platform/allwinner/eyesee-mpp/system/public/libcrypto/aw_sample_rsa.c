#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <assert.h>

#include <crypto/aw_cipher.h>


#define AW_ERR_CIPHER(format, arg...)    \
		printf( "%s,%d: " format , __FUNCTION__, __LINE__, ## arg)
#define AW_INFO_CIPHER(format, arg...)    \
		printf( "%s,%d: " format , __FUNCTION__, __LINE__, ## arg)

static int printBuffer(const char *string, const unsigned char *in,
						unsigned int u32Length)
{
    unsigned int i = 0;

    if ( NULL != string )
    {
        printf("%s\n", string);
    }

    for ( i = 0 ; i < u32Length; i++ )
    {
        if( (i % 16 == 0) && (i != 0)) printf("\n");
        printf("0x%02x ", in[i]);
    }
    printf("\n");

    return 0;
}

static unsigned char test_data[] = \
			{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"};

unsigned char NO_PADDING[] =
{
    0x31, 0x5d, 0xfa, 0x52, 0xa4, 0x93, 0x52, 0xf8,
    0xf5, 0xed, 0x39, 0xf4, 0xf8, 0x23, 0x4b, 0x30,
    0x11, 0xa2, 0x2c, 0x5b, 0xa9, 0x8c, 0xcf, 0xdf,
    0x19, 0x66, 0xf5, 0xf5, 0x1a, 0x6d, 0xf6, 0x25,
    0x89, 0xaf, 0x06, 0x13, 0xdc, 0xa4, 0xd4, 0x0b,
    0x3c, 0x1c, 0x4f, 0xb9, 0xd3, 0xd0, 0x63, 0x29,
    0x2a, 0x5d, 0xfe, 0xb6, 0x99, 0x20, 0x58, 0x36,
    0x2b, 0x1d, 0x57, 0xf4, 0x71, 0x38, 0xa7, 0x8b,
    0xad, 0x8c, 0xef, 0x1f, 0x2f, 0xea, 0x4c, 0x87,
    0x2b, 0xd7, 0xb8, 0xc8, 0xb8, 0x09, 0xcb, 0xb9,
    0x05, 0xab, 0x43, 0x41, 0xd9, 0x75, 0x36, 0x4d,
    0xb6, 0x8a, 0xd3, 0x45, 0x96, 0xfd, 0x9c, 0xe8,
    0x6e, 0xc8, 0x37, 0x5e, 0x4f, 0x63, 0xf4, 0x1c,
    0x18, 0x2c, 0x38, 0x79, 0xe2, 0x5a, 0xe5, 0x1d,
    0x48, 0xf6, 0xb2, 0x79, 0x57, 0x12, 0xab, 0xae,
    0xc1, 0xb1, 0x9d, 0x11, 0x4f, 0xa1, 0x4d, 0x1b,
    0x4c, 0x8c, 0x3a, 0x2d, 0x7b, 0x98, 0xb9, 0x89,
    0x7b, 0x38, 0x84, 0x13, 0x8e, 0x3f, 0x3c, 0xe8,
    0x59, 0x26, 0x90, 0x77, 0xe7, 0xca, 0x52, 0xbf,
    0x3a, 0x5e, 0xe2, 0x58, 0x54, 0xd5, 0x9b, 0x2a,
    0x0d, 0x33, 0x31, 0xf4, 0x4d, 0x68, 0x68, 0xf3,
    0xe9, 0xb2, 0xbe, 0x28, 0xeb, 0xce, 0xdb, 0x36,
    0x1e, 0xae, 0xb7, 0x37, 0xca, 0xaa, 0xf0, 0x9c,
    0x6e, 0x27, 0x93, 0xc9, 0x61, 0x76, 0x99, 0x1a,
    0x0a, 0x99, 0x57, 0xa8, 0xea, 0x71, 0x96, 0x63,
    0xbc, 0x76, 0x11, 0x5c, 0x0c, 0xd4, 0x70, 0x0b,
    0xd8, 0x1c, 0x4e, 0x95, 0x89, 0x5b, 0x09, 0x17,
    0x08, 0x44, 0x70, 0xec, 0x60, 0x7c, 0xc9, 0x8a,
    0xa0, 0xe8, 0x98, 0x64, 0xfa, 0xe7, 0x52, 0x73,
    0xb0, 0x04, 0x9d, 0x78, 0xee, 0x09, 0xa1, 0xb9,
    0x79, 0xd5, 0x52, 0x4f, 0xf2, 0x39, 0x1c, 0xf7,
    0xb9, 0x73, 0xe0, 0x3d, 0x6b, 0x54, 0x64, 0x86
};


#define PUB_KEY_PATH "/mnt/extsd/pubkey.bin"
#define PRI_KEY_PATH "/mnt/extsd/privkey.bin"
#define RSA_MAX_BITS 4096

/***************get pubkey, prikey**************/
static unsigned char g_pkey[1024] ;
static unsigned char g_privkey[1024] ;
static int g_key_valid = 0;

int PKCS_PUB_ENC(AW_CIPHER_RSA_ENC_SCHEME_E enScheme)
{
    int ret = 0;
    unsigned char  u8Enc[RSA_MAX_BITS >> 2];//save enc data
    unsigned char  u8Dec[RSA_MAX_BITS >> 2];//save dec data
    unsigned int u32OutLen,klen;

    ret = AW_CIPHER_RsaGenPubKey(PUB_KEY_PATH, g_pkey, &klen);
    ret += AW_CIPHER_RsaGenPriKey(PRI_KEY_PATH, g_privkey, &klen);

	if ((ret) || (klen > (RSA_MAX_BITS>>2)))
	{
		AW_ERR_CIPHER("Get key pair failed key lenth: %d Bytes\n", klen);
		return -1;
		
	}	

	g_key_valid = 1;	
    /***************pub key enc**************/
    AW_CIPHER_RSA_PUB_ENC_S pstRsaEnc;
    memset(&pstRsaEnc, 0, sizeof(AW_CIPHER_RSA_PUB_ENC_S));
    pstRsaEnc.enScheme = enScheme;
    pstRsaEnc.stPubKey = g_pkey;
    if(enScheme == AW_CIPHER_RSA_ENC_SCHEME_NO_PADDING)
    {
        ret = AW_CIPHER_RsaPublicEnc(&pstRsaEnc,NO_PADDING, klen,
        u8Enc, &u32OutLen);
    }
    else
    {
        ret = AW_CIPHER_RsaPublicEnc(&pstRsaEnc, test_data,
        sizeof(test_data) - 1, u8Enc, &u32OutLen);
    }

    if ( 0 != ret )
    {
        AW_ERR_CIPHER("AW_CIPHER_RsaPublicEnc failed\n\n");
        ret =  -1;
        goto OUT;

    }

    printBuffer("After enc:", u8Enc, u32OutLen);

    /***************pri key dec**************/
    AW_CIPHER_RSA_PRI_ENC_S pstRsaDec;
    memset(&pstRsaDec, 0, sizeof(AW_CIPHER_RSA_PRI_ENC_S));
    pstRsaDec.enScheme = enScheme;
    pstRsaDec.stPriKey = g_privkey;

    ret = AW_CIPHER_RsaPrivateDec(&pstRsaDec, u8Enc, u32OutLen,
    	u8Dec, &u32OutLen);

    printBuffer("After dec", u8Dec, u32OutLen);

    if ( 0 != ret )
    {
        AW_ERR_CIPHER("AW_CIPHER_RsaPrivateDec failed\n\n");
        ret =  -1;
        goto OUT;
    }

    if(enScheme == AW_CIPHER_RSA_ENC_SCHEME_NO_PADDING)
    {
        if(memcmp(u8Dec, NO_PADDING, klen) != 0)
        {
            AW_ERR_CIPHER("AW_CIPHER_RsaPrivateDec failed\n");
            printBuffer("expect", NO_PADDING, klen);
            printBuffer("real", u8Dec, klen);
            ret =  -1;
        }
    }
    else
    {
        if((sizeof(test_data) - 1) != u32OutLen)
        {
            AW_ERR_CIPHER("aw_CIPHER_RsaPrivateDec len error\n\n");
        }
        if(memcmp(u8Dec, test_data, sizeof(test_data) - 1) != 0)
        {
            AW_ERR_CIPHER("AW_CIPHER_RsaPrivateDec failed\n\n");
            ret = -1;
        }
        printBuffer("expect", test_data, sizeof(test_data) - 1);
        printBuffer("real", u8Dec, u32OutLen);
    }
OUT:
    return 0;
}
static const unsigned char to_sign[] = "Hello, World!!";
static unsigned char g_sig[1024] ;
static unsigned int sig_len = 0;
int rsa_entry()
{
	int ret = -1;
	AW_CIPHER_RSA_SIGN_S sign;
	AW_CIPHER_RSA_VERIFY_S verify;
    AW_INFO_CIPHER("RSA test begin.........................\n");
    PKCS_PUB_ENC(AW_CIPHER_RSA_ENC_SCHEME_NO_PADDING);
    PKCS_PUB_ENC(AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_2);
    AW_INFO_CIPHER("RSA test end.........................\n");

	memset(&sign, 0, sizeof(sign));
	sign.enScheme = AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA256;
	sign.stPriKey = g_privkey;

	if(g_key_valid == 0)
	{
	 	AW_ERR_CIPHER("RSA key invalid ,return...\n");	
		return 0;
	}
	AW_INFO_CIPHER("Sign data begin.........................\n");
	AW_CIPHER_RsaSign(&sign , (unsigned char*)to_sign,
	(unsigned int )strlen((const char*)to_sign),g_sig, &sig_len);

	if (sig_len <= 1024)
	printBuffer("signature :", g_sig, sig_len);
	
	AW_INFO_CIPHER("Verity signature begin .........................\n");
	memset(&verify, 0, sizeof(verify));
	verify.enScheme = AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA256;
	verify.stPubKey = g_pkey;
	ret = AW_CIPHER_RsaVerify(&verify, (unsigned char*)to_sign,
			(unsigned int )strlen((const char*)to_sign),g_sig, sig_len);
	if (ret != 0)
		AW_ERR_CIPHER("fail :verify signature failed.\n");
	AW_ERR_CIPHER("verify signature success.\n");	
    return 0;
}
