/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    aw_cipher.c
 * @brief
 * @author
 * @version
 * @date
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/engine.h>
#include <openssl/aes.h>
#include <openssl/des.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>

#include <crypto/aw_cipher.h>

#include "list.h"
#include <cutils/atomic-arm.h>
#define LOG_TAG "AW_CIPHER"
#include <cutils/log.h>
typedef unsigned int u32;

int AW_CIPHER_RsaPublicEnc(AW_CIPHER_RSA_PUB_ENC_S *pstRsaEnc,
                      unsigned char *pu8Input, unsigned int u32InLen,
                      unsigned char *pu8Output, unsigned int *pu32OutLen)
{

    int ret = 0;
    unsigned int rsa_len = 0;

    if((pstRsaEnc == NULL)||(pu8Input == NULL)||
    	(pu8Output == NULL)||(pu32OutLen == NULL)||(u32InLen == 0))
    {
        ALOGE("input para is invalid, null or 0.\n");
        return -1;
    }

    rsa_len = RSA_size((RSA *)(pstRsaEnc->stPubKey));
    switch(pstRsaEnc->enScheme)
    {
    case AW_CIPHER_RSA_ENC_SCHEME_NO_PADDING:
        *pu32OutLen = rsa_len;
        if(RSA_public_encrypt(u32InLen, pu8Input, pu8Output,
        	(RSA *)(pstRsaEnc->stPubKey),RSA_NO_PADDING)<0){
            ret = -1;
        }
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_0:
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_1:
        ALOGE("RSA padding mode = 0x%x.\n", pstRsaEnc->enScheme);
        ret = -1;
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_2:
        *pu32OutLen = rsa_len;
        if(RSA_public_encrypt(u32InLen, pu8Input,pu8Output,
        	(RSA *)(pstRsaEnc->stPubKey),RSA_PKCS1_PADDING) < 0){
            ret = -1;
        }
        break;

    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA1:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA256:
        *pu32OutLen = rsa_len;
        if(RSA_public_encrypt(rsa_len, pu8Input, pu8Output,
        	(RSA *)(pstRsaEnc->stPubKey),RSA_PKCS1_OAEP_PADDING)<0){
            ret = -1;
        }
        break;

    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_PKCS1_V1_5:
    	ret = -1;
        ALOGE("RSA padding mode = 0x%x.\n", pstRsaEnc->enScheme);
        break;

    default:
        ALOGE("RSA padding  mode = 0x%x.\n", pstRsaEnc->enScheme);
        ret = -1;
        break;
    }

    if (ret != 0)
    {
        ALOGE("RSA padding error, ret = 0x%x.\n", ret);
        return ret;
    }

    if(*pu32OutLen != rsa_len)
    {
        ALOGE("Error, u32OutLen != rsa_len. pu32OutLen is %d,\
        rsa_len is %ud.\n", *pu32OutLen, rsa_len);
        return ret;
    }

    return ret;
}


int AW_CIPHER_RsaPrivateDec(AW_CIPHER_RSA_PRI_ENC_S *pstRsaDec,
                       unsigned char *pu8Input, unsigned int u32InLen,
                       unsigned char *pu8Output, unsigned int *pu32OutLen)
{

    int ret = 0;

    if((pstRsaDec == NULL)||(pu8Input == NULL)||(pu8Output == NULL)||
    (pu32OutLen == NULL)||(u32InLen == 0))
    {
        ALOGE("input para is invalid, null or 0.\n");
        return -1;
    }

    switch(pstRsaDec->enScheme)
    {
    case AW_CIPHER_RSA_ENC_SCHEME_NO_PADDING:
        *pu32OutLen = u32InLen;
        if(RSA_private_decrypt(u32InLen, pu8Input,pu8Output,\
        (RSA *)(pstRsaDec->stPriKey),RSA_NO_PADDING)<0){
            ret = -1;
        }
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_0:
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_1:
        ALOGE("RSA padding mode err, mode = 0x%x.\n", pstRsaDec->enScheme);
        ret = -1;
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_2:
        (*pu32OutLen) = RSA_private_decrypt(u32InLen, pu8Input,pu8Output,\
        (RSA *)(pstRsaDec->stPriKey), \
        								RSA_PKCS1_PADDING);
        if(*pu32OutLen<0){
            ret = -1;
        }
        break;

    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA1:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA256:
        if(((*pu32OutLen) = RSA_private_decrypt(u32InLen, pu8Input,\
        	pu8Output, (RSA *)(pstRsaDec->stPriKey), \
        								RSA_PKCS1_OAEP_PADDING))<0){
            ret = -1;
        }
        break;

    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_PKCS1_V1_5:
    	ret = -1;
        ALOGE("RSA padding mode err, mode = 0x%x.\n", pstRsaDec->enScheme);
        break;

    default:
        ALOGE("RSA padding mode err, mode = 0x%x.\n", pstRsaDec->enScheme);
        ret = -1;
        break;
    }

    if (ret != 0)
    {
        ALOGE("RSA padding error, ret = 0x%x.\n", ret);
        return ret;
    }

    return ret;
}

int AW_CIPHER_RsaPrivateEnc(AW_CIPHER_RSA_PRI_ENC_S *pstRsaEnc,
                   unsigned char *pu8Input, unsigned int u32InLen,
                   unsigned char *pu8Output, unsigned int *pu32OutLen)
{

    int ret = 0;
    int rsa_len = 0;

    if((pstRsaEnc == NULL)||(pu8Input == NULL)||
    	(pu8Output == NULL)||(pu32OutLen == NULL)||(u32InLen == 0))
    {
        ALOGE("input para is invalid, null or 0.\n");
        return -1;
    }
    rsa_len = RSA_size((RSA *)(pstRsaEnc->stPriKey));;
    switch(pstRsaEnc->enScheme)
    {
    case AW_CIPHER_RSA_ENC_SCHEME_NO_PADDING:
        *pu32OutLen = rsa_len;
        if(RSA_private_encrypt(u32InLen, pu8Input, pu8Output,
        	(RSA *)(pstRsaEnc->stPriKey),RSA_NO_PADDING)<0){
            ret = -1;
        }
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_0:
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_1:
        *pu32OutLen = rsa_len;
        if(RSA_private_encrypt(u32InLen, pu8Input, pu8Output,
        	(RSA *)(pstRsaEnc->stPriKey),RSA_PKCS1_PADDING)<0){
            ret = -1;
        }
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_2:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA1:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA256:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_PKCS1_V1_5:
        ALOGE("RSA padding mode = 0x%x.\n", pstRsaEnc->enScheme);
        ret = -1;
        break;

    default:
        ALOGE("RSA padding mode = 0x%x.\n", pstRsaEnc->enScheme);
        ret = -1;
        break;
    }

    if (ret != 0)
    {
        ALOGE("RSA padding error, ret = 0x%x.\n", ret);
        return ret;
    }

    if(*pu32OutLen != rsa_len)
    {
        ALOGE("pu32OutLen is %d, rsa_len is %d.\n", *pu32OutLen, rsa_len);
        return ret;
    }

    return ret;
}

static inline const EVP_MD * aw_getevp(int nid)
{
	switch (nid)
	{
		case NID_sha1:
			return EVP_sha1();
		case NID_sha256:
			return EVP_sha256();
		case NID_sha512:
			return EVP_sha512();
		default:
		break;
	}
	return NULL;
}

static int aw_cal_md(int nid, unsigned char * in, unsigned int inlen,
unsigned char * md)
{
	EVP_MD_CTX ctx;
	const EVP_MD *evp_md = NULL;
	unsigned int md_len;
	evp_md = aw_getevp(nid);
	if (!evp_md)
		return -1;
	EVP_DigestInit(&ctx, evp_md);
	EVP_DigestUpdate(&ctx, in, inlen);
	EVP_DigestFinal(&ctx, md, &md_len);
	return 0;
}

int AW_CIPHER_RsaSign(AW_CIPHER_RSA_SIGN_S *pstRsaSign,
         unsigned char *pu8InData,  unsigned int u32InDataLen,
         unsigned char *pu8OutSign, unsigned int *pu32OutSignLen)
{
	int md_len = 0, nid, ret;
	unsigned char* md = NULL;
	RSA* rsa = NULL;
    if((pstRsaSign == NULL)|| (pu8OutSign == NULL)
    || (pu32OutSignLen == NULL) ||(u32InDataLen == 0))
    {
        ALOGE("input para is invalid, NULL.\n");
        return AW_ERR_CIPHER_INVALID_ARG;
    }

	if (AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA1 ==
    	pstRsaSign->enScheme){
    	nid = NID_sha1;
    	md_len = 20;

    }
    else if(AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA256 ==
    	pstRsaSign->enScheme){
    	nid = NID_sha256;
    	md_len = 32;
    }
    else if(AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA512 ==
    	pstRsaSign->enScheme){
    	nid = NID_sha512;
    	md_len = 64;
    }
    else
    {
    	ALOGW("Please extend RSA_SIGN_SCHEME!!\n");
    	return AW_ERR_CIPHER_NOT_SUPPORTED; 
    }

    md = malloc(md_len);
    if(!md)
    {
    	ALOGW("AW_ERR_CIPHER_HEAP!!\n");
    	return AW_ERR_CIPHER_HEAP;
    }

    ret = aw_cal_md(nid ,pu8InData, u32InDataLen, md);
    if (ret)
    {
    	ALOGW("calulate message digest failed!!\n");
    	free(md);
    	return AW_ERR_CIPHER_OP_FAILED;
    }

	rsa = (RSA*)(pstRsaSign->stPriKey);
    if ( RSA_sign(nid, md, md_len, pu8OutSign, pu32OutSignLen, rsa) != 1 ){
		ALOGE( "Couldn't sign message digest.\n" );
		free(md);
		return AW_ERR_CIPHER_OP_FAILED ;
	}
	free(md);
    return 0;
}


int AW_CIPHER_RsaVerify(AW_CIPHER_RSA_VERIFY_S *pstRsaVerify,
	               unsigned char *pu8InData, unsigned int u32InDataLen,
	               unsigned char *pu8InSign, unsigned int u32InSignLen)
{
	int md_len = 0, nid;
	unsigned char *md = NULL;
	RSA* rsa = NULL;
	if((pu8InData == NULL)|| (pu8InSign == NULL)
    || (u32InSignLen == 0) ||(u32InDataLen == 0))
    {
        ALOGE("input para is invalid, NULL.\n");
        return AW_ERR_CIPHER_INVALID_ARG;
    }
    if (AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA1 ==
    	pstRsaVerify->enScheme){
    	nid = NID_sha1;
    	md_len = 20;
    }
    else if(AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA256 ==
    	pstRsaVerify->enScheme){
    	nid = NID_sha256;
    	md_len = 32;
    }
	else if(AW_CIPHER_RSA_SIGN_SCHEME_RSASSA_PKCS1_V15_SHA512 ==
		pstRsaVerify->enScheme){
		nid = NID_sha512;
		md_len = 64;
	}
    else
    {
    	ALOGW("Not supported RSA_SIGN_SCHEME ,Please extend!!\n");
    	return AW_ERR_CIPHER_NOT_SUPPORTED; 
    }
    md = malloc(md_len);
    if(!md)
    {
    	ALOGW("AW_ERR_CIPHER_HEAP!!\n");
    	return AW_ERR_CIPHER_HEAP;
    }

    aw_cal_md(nid ,pu8InData, u32InDataLen, md);

	rsa = (RSA*)(pstRsaVerify->stPubKey);
    if ( RSA_verify(nid, md, md_len, pu8InSign, u32InSignLen,rsa) != 1 ) {
		ALOGE("Couldn't verify message digest.\n" );
		free(md);
		return AW_ERR_CIPHER_OP_FAILED ;
	}
	free(md);
    return 0;
}


int AW_CIPHER_RsaPublicDec(AW_CIPHER_RSA_PUB_ENC_S *pstRsaDec,
                   unsigned char *pu8Input, unsigned int u32InLen,
                   unsigned char *pu8Output, unsigned int *pu32OutLen)
{
    int ret = 0;

    if((pstRsaDec == NULL)||(pu8Input == NULL)||
    	(pu8Output == NULL)||(pu32OutLen == NULL)||(u32InLen == 0))
    {
        ALOGE("input para is invalid\n");
        return AW_ERR_CIPHER_INVALID_ARG;
    }

    switch(pstRsaDec->enScheme)
    {
    case AW_CIPHER_RSA_ENC_SCHEME_NO_PADDING:
        *pu32OutLen = u32InLen;
        if(RSA_public_decrypt(u32InLen, pu8Input, pu8Output,
        	(RSA *)(pstRsaDec->stPubKey),RSA_NO_PADDING)<0){
            ret = AW_ERR_CIPHER_OP_FAILED;
        }
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_0:
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_1:
        (*pu32OutLen) = RSA_public_decrypt(u32InLen, pu8Input, pu8Output,
        		(RSA *)(pstRsaDec->stPubKey),RSA_PKCS1_PADDING);
        if((*pu32OutLen) < 0){
            ret = AW_ERR_CIPHER_OP_FAILED;
        }
        break;
    case AW_CIPHER_RSA_ENC_SCHEME_BLOCK_TYPE_2:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA1:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_OAEP_SHA256:
    case AW_CIPHER_RSA_ENC_SCHEME_RSAES_PKCS1_V1_5:
        ALOGE("RSA padding  mode = 0x%x.\n", pstRsaDec->enScheme);
        ret = AW_ERR_CIPHER_NOT_SUPPORTED;
        break;

    default:
        ALOGE("RSA padding mode = 0x%x.\n", pstRsaDec->enScheme);
        ret = AW_ERR_CIPHER_OP_FAILED;
        break;
    }

    return ret;
}


extern RSA *PEM_read_RSA_PUBKEY(FILE *fp, RSA **x,
			void *cb, void *u);

//Produce rsa key pair:  openssl genrsa -out privkey.pem 1024
//openssl rsa -in privkey.pem -pubout -out pubkey.pem
//This API: pubkey.pem---> octec array pubkey (Actually,PubKey is RSA*)
int AW_CIPHER_RsaGenPubKey(const char *path_key,
	unsigned char *PubKey, unsigned int* p_klen)
{
    RSA *p_rsa;
    FILE *file;
    int rsa_len;
    if ((PubKey == NULL) || (!p_klen)){
        ALOGE("error, pubkey addr is NULL....");
        return AW_ERR_CIPHER_INVALID_ARG;
    }

    if((file = fopen(path_key, "r")) == NULL){
        ALOGE("open public key file error!!!!!!!!!!!!!!!!");
        return AW_ERR_CIPHER_OP_FAILED;
    }
    if((p_rsa = PEM_read_RSA_PUBKEY(file,NULL,NULL,NULL)) == NULL){
        ALOGE("read RSA public key error.");
        return AW_ERR_CIPHER_OP_FAILED;
    }

    rsa_len = RSA_size(p_rsa);
    if(rsa_len  <=  0){
        ALOGE("rsa public key length error...\n");
        return AW_ERR_CIPHER_OP_FAILED;
    }
    *p_klen = rsa_len;
    ALOGE("rsa public key rsa_len is %d...\n", rsa_len);
    memcpy(PubKey, p_rsa, rsa_len);
    ALOGE("read RSA public key success......\n");
    return 0;
}

//Get RSA private key(RSA*) from PEM file and filled it to octec array.
extern RSA *PEM_read_RSAPrivateKey(FILE *fp, RSA **x,
		void *cb, void *u);
int AW_CIPHER_RsaGenPriKey(const char *path_key,
				unsigned char *PriKey, unsigned int* p_klen)
{
    RSA *p_rsa;
    FILE *file;
    int rsa_len;
    if ((PriKey == NULL) || (!p_klen)){
    	ALOGE("ARG issue...");
  		return AW_ERR_CIPHER_INVALID_ARG;
    }

    if((file=fopen(path_key,"r"))==NULL){
        ALOGE("open private key file error");
        return AW_ERR_CIPHER_OP_FAILED;
    }
    if((p_rsa=PEM_read_RSAPrivateKey(file,NULL,NULL,NULL))==NULL){
        ALOGE("read RSA private key error.");
        return AW_ERR_CIPHER_OP_FAILED;
    }

    rsa_len = RSA_size(p_rsa);
    if(rsa_len <= 0){
        ALOGE("rsa private key length error......\n");
        return AW_ERR_CIPHER_OP_FAILED;
    }
    ALOGE("rsa private key rsa_len is %d...\n", rsa_len);
    *p_klen = rsa_len;
    memcpy(PriKey, p_rsa, rsa_len);

    ALOGE("read RSA private key success......\n");
    return 0;
}

typedef struct aw_cipher_handle
{
	struct list_head node;
	AW_CIPHER_TYPE_S type;
	struct aw_cipher_handle *phandle;
	pthread_mutex_t   handle_mutex;
	int status;
	AW_CIPHER_SUB1_U aw_cipher_sub1;
}AW_CIPHER_HANDLE_S;

#if 1
static pthread_mutex_t   g_module_mutex;

#define AW_MODULE_MUTEX_CREAT()     \
do{pthread_mutex_init(&g_module_mutex, NULL);} \
while(0);

#define AW_MODULE_MUTEX_DEL()     \
do{ \
  	pthread_mutex_destroy(&g_module_mutex);\
}\
while(0);

#define AW_HANDLE_MUTEX_CREAT(pmtx)     \
do{pthread_mutex_init(pmtx, NULL);} while(0);

#define AW_HANDLE_MUTEX_DEL(pmtx)     \
do{pthread_mutex_destroy(pmtx);} while(0);

#define AW_MODULE_LOCK()      (void)pthread_mutex_lock(&g_module_mutex);
#define AW_MODULE_UNLOCK()    (void)pthread_mutex_unlock(&g_module_mutex);

#define AW_HANDLE_LOCK(mutex)      (void)pthread_mutex_lock(&mutex);
#define AW_HANDLE_UNLOCK(mutex)    (void)pthread_mutex_unlock(&mutex);
#else
//dbg single thread 
#define AW_MODULE_MUTEX_CREAT(...) do{}while(0)
#define AW_MODULE_MUTEX_DEL(...) do{}while(0)
#define AW_HANDLE_MUTEX_CREAT(...) do{}while(0)
#define AW_HANDLE_MUTEX_DEL(...) do{}while(0)
#define AW_HANDLE_LOCK(...) do{}while(0)
#define AW_HANDLE_UNLOCK(...) do{}while(0)
#define AW_MODULE_LOCK(...) do{}while(0)
#define AW_MODULE_UNLOCK(...) do{}while(0)

#endif

static ENGINE *openssl_engine_init()
{
    ENGINE *e = NULL;
    OpenSSL_add_all_algorithms();
    ENGINE_load_builtin_engines();
    e = ENGINE_by_id("af_alg");
    return e;
}

static void openssl_engine_free(ENGINE *e)
{
    if (e != NULL)
        ENGINE_free(e);

    ENGINE_cleanup();
    EVP_cleanup();
}

#define AW_RAND_BYTES_MAX 32
static int aw_hw_rand_bytes(unsigned char* buf,unsigned int num)
{
	ENGINE *e = NULL;
	const RAND_METHOD* rnd_md =NULL;
	const char *name = "af_alg";
	EVP_MD_CTX ctx = {0};
	OpenSSL_add_all_algorithms();
	ENGINE_load_builtin_engines();
	e = ENGINE_by_id(name);
	if (!e) {
		ALOGE("find engine %s error\n", name);
		return AW_ERR_CIPHER_OP_FAILED;
	}
	ENGINE_ctrl_cmd_string(e, "RAND", 0, 0);
	rnd_md = ENGINE_get_RAND(e);
	if ((rnd_md && rnd_md->bytes) && (1 == rnd_md->bytes(buf, num)))
	{
		return num;
	}
	return(AW_ERR_CIPHER_OP_FAILED);
}


static int aw_sw_rand_bytes(unsigned char* buf,unsigned int num)
{
	return RAND_bytes(buf, num);
}

/*Just provide simple api, if the arg: num is too large ,  call multiple times is ok*/
int AW_CIPHER_Rand(unsigned char* buf,unsigned int num)
{
	int ret = 0;
	if (!buf)
	{
		ALOGE("ARG issue...");
		return AW_ERR_CIPHER_INVALID_ARG;
	}
	if (num > AW_RAND_BYTES_MAX)
	{
		ALOGE("AW_ERR_CIPHER_RANDBYTES_TOO_LARGE");
		return AW_ERR_CIPHER_RANDBYTES_TOO_LARGE;
	}
	ret = aw_hw_rand_bytes(buf, num);
	
	if(ret <= 0)
	{
		ret = aw_sw_rand_bytes(buf, num);
	}
	
	return ret;
}

static unsigned int g_aw_cipher_ref = 0;
LIST_HEAD(g_aw_cipher_handle_list);
static int aw_cipher_module_init()
{
	return 0;
}

static int aw_cipher_module_deinit()
{
	return 0;
}

static int aw_cipher_module_get()
{
	g_aw_cipher_ref++;
	return g_aw_cipher_ref;
}

static int aw_cipher_module_put()
{
	if(g_aw_cipher_ref != 0)
		g_aw_cipher_ref--;
	else
		return -1;
	return g_aw_cipher_ref;
}


static int g_init_flg = 1,g_mod_mtx_created = 0;

static int aw_check_init()
{
	if (g_aw_cipher_ref)
		return 0;
	return AW_ERR_CIPHER_NOT_INIT;
}
int AW_CIPHER_Init()
{
    int ret = 0;
	#if 0
	#define PROTECT_MUTEX_CREAT_TIME_US 100
    if (android_atomic_inc(&g_init_flg) == 2)
    {
    	ALOGI("AW_MODULE_MUTEX_CREAT");
    	AW_MODULE_MUTEX_CREAT();
    	g_mod_mtx_created = 1;
    }
    else
    {
    	while(g_mod_mtx_created == 0)
    	{
    		usleep(PROTECT_MUTEX_CREAT_TIME_US);
    	}
    }
    #else
    if (g_mod_mtx_created == 0)
	{
		g_mod_mtx_created = 1;
		INIT_LIST_HEAD(&g_aw_cipher_handle_list);
		AW_MODULE_MUTEX_CREAT();
	}
    #endif
    g_init_flg = 100;
    AW_MODULE_LOCK();
    ret = aw_cipher_module_init();
    aw_cipher_module_get();
    AW_MODULE_UNLOCK();

    return ret;
}

int AW_CIPHER_DeInit()
{
	int ret = 0;
    AW_MODULE_LOCK();
    if(aw_cipher_module_put() == 0)
    {
    	ret = aw_cipher_module_deinit();
    }
    AW_MODULE_UNLOCK();
    return ret;
}

static int type_parsing(AW_CIPHER_TYPE_S type)
{
	unsigned int  hw = type.hw;
	unsigned int  tp_sub1 = type.tp_sub1;
	unsigned int  alg = type.alg;
	unsigned int  mode = type.mode;
	unsigned int  key_len = type.key_len;
	if ((hw >= AW_CIPHER_TYPE_BUTT) ||
	(tp_sub1 >= AW_CIPHER_SUBTYPE1_BUTT)||
	(alg >= AW_CIPHER_ALG_BUTT)||(key_len >= AW_CIPHER_KEY_LEN_BUTT))
		return -1;

	return 0;
}

#define AW_GET_CIPHER_TYPE(alg,mode,keylen,bw) \
(((((u32)(alg)) << ALG_OFFSET) & (ALG_MASK))|\
((((u32)(mode)) << MODE_OFFSET) & (MODE_MASK))|\
((((u32)(keylen)) << KEY_LEN_OFFSET) & (KEY_LEN_MASK))|\
((((u32)(bw)) << BW_OFFSET) & (BW_MASK)))

static int map_netid(AW_CIPHER_CRYPTO_S* pcry,u32 alg,
u32 mode, u32 klen, u32 bw)
{
	unsigned int type = ((alg << ALG_OFFSET) & ALG_MASK)|
	((mode << MODE_OFFSET) & MODE_MASK)|
	((klen << KEY_LEN_OFFSET) & KEY_LEN_MASK)|
	((bw << BW_OFFSET) & BW_MASK);
	switch (type)
	{
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_DES, AW_CIPHER_MODE_ECB,\
		     AW_CIPHER_KEY_DES, AW_CIPHER_BIT_WIDTH_64BIT):
			 pcry->e_cipher = EVP_des_ecb();
			 return NID_des_ecb;

		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_3DES, AW_CIPHER_MODE_ECB,
			 AW_CIPHER_KEY_DES_2KEY, AW_CIPHER_BIT_WIDTH_64BIT) :
			 pcry->e_cipher = EVP_des_ede_ecb();
			 return NID_des_ede_ecb;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_3DES, AW_CIPHER_MODE_ECB,
			 AW_CIPHER_KEY_DES_3KEY, AW_CIPHER_BIT_WIDTH_64BIT) :
			 pcry->e_cipher = EVP_des_ede3_ecb();
			 return NID_des_ede3_ecb;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_ECB,
			 AW_CIPHER_KEY_AES_128BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_128_ecb();
			 return NID_aes_128_ecb;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_ECB,
			 AW_CIPHER_KEY_AES_192BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_192_ecb();
			 return NID_aes_192_ecb;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_ECB,
			 AW_CIPHER_KEY_AES_256BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_256_ecb();
			 return NID_aes_256_ecb;			 
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CBC,
			 AW_CIPHER_KEY_AES_128BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_128_cbc();
			 return NID_aes_128_cbc;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CBC,
			 AW_CIPHER_KEY_AES_192BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_192_cbc();
			 return NID_aes_192_cbc;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CBC,
			 AW_CIPHER_KEY_AES_256BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_256_cbc();
			 return NID_aes_256_cbc;			 
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CBC_CTS,
			 AW_CIPHER_KEY_AES_128BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_128_cts();
			 return NID_aes_128_cts;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CBC_CTS,
			 AW_CIPHER_KEY_AES_192BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_192_cts();
			 return NID_aes_192_cts;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CBC_CTS,
			 AW_CIPHER_KEY_AES_256BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_256_cts();
			 return NID_aes_256_cts;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CTR,
			 AW_CIPHER_KEY_AES_128BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_128_ctr();
			 return NID_aes_128_ctr;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CTR,
			 AW_CIPHER_KEY_AES_192BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_192_ctr();
			 return NID_aes_192_ctr;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CTR,
			 AW_CIPHER_KEY_AES_256BIT, AW_CIPHER_BIT_WIDTH_128BIT) :
			 pcry->e_cipher = EVP_aes_256_ctr();
			 return NID_aes_256_ctr;			 
		default :
			 return -1;
	}
}

static int aw_cipher_creat_hw(AW_CIPHER_CRYPTO_S* pcrypto,
int alg, int mode, int key_len, int bw)
{
	int ret ,nid;
	pcrypto->engine = openssl_engine_init();
	if (pcrypto->engine == NULL) {
		ALOGE("!!No engine !!\n");
		return -1;
	}
	nid = map_netid(pcrypto,(u32)alg, (u32)mode, (u32)key_len, (u32)bw);
	if (nid == -1)
	{
		ALOGE("!!Map id failed, Please extend cipher alg!!\n");
		goto err;
	}

	pcrypto->e_cipher = ENGINE_get_cipher(pcrypto->engine, nid);
	if (pcrypto->e_cipher == NULL)
	{
		ALOGE("!!ENGINE_get_cipher failed!!\n");
		goto err;
	}

	ALOGI("success!!! aw_cipher_creat_hw success...\n");
	return 0;
err:
	ALOGE("failed!!! aw_cipher_creat_hw failed...\n");
	openssl_engine_free(pcrypto->engine);
	return -1 ;
}

static int aw_cipher_destroy_hw(AW_CIPHER_CRYPTO_S* pcrypto,
int alg, int mode, int key_len, int bw)
{
	openssl_engine_free(pcrypto->engine);
	pcrypto->engine = NULL;
	return 0;
}


static int aw_cipher_creat_sw(AW_CIPHER_CRYPTO_S* pcrypto,
int alg, int mode, int key_len, int bw)
{
	int nid;
	nid = map_netid(pcrypto, alg, mode, key_len, bw);
	if (pcrypto->e_cipher == NULL)
	{
		ALOGE("failed!!! aw_cipher_creat_sw failed...nid %d\n",nid);
		return -1;
	}
	return 0;
}

static int aw_cipher_destroy_sw(AW_CIPHER_CRYPTO_S* pcrypto,
int alg, int mode, int key_len, int bw)
{
	ALOGI("trace: aw_cipher_destroy_sw...\n");
	return 0;
}

static int aw_hmac_pack(EVP_MD * evp_md,void * key,
int key_len,const unsigned char * d,size_t n,
unsigned char * md,unsigned int * md_len)
{
	HMAC(evp_md, key, key_len, d, n, md, md_len);
	return 0;
}
static int map_netid_hash(AW_CIPHER_HASH_S* phash,u32 alg,
u32 mode, u32 klen, u32 bw)
{
	unsigned int type = ((alg << ALG_OFFSET) & ALG_MASK)|
	((mode << MODE_OFFSET) & MODE_MASK)|
	((klen << KEY_LEN_OFFSET) & KEY_LEN_MASK)|
	((bw << BW_OFFSET) & BW_MASK);
	switch (type)
	{
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_SHA, AW_HASH_MD5, 0, 0):
			phash->e_md = EVP_md5();
			return NID_md5;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_SHA, AW_HASH_SHA1, 0, 0):
			phash->e_md = EVP_sha1();
			return NID_sha1;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_SHA, AW_HASH_SHA256, 0, 0):
			phash->e_md = EVP_sha256();
			return NID_sha256;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_SHA, AW_HASH_SHA384,0,0):
			phash->e_md = EVP_sha224();
			return NID_sha384;
		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_SHA, AW_HASH_SHA512,0,0):
			phash->e_md = EVP_sha512();
			return NID_sha512;

		case AW_GET_CIPHER_TYPE(AW_CIPHER_ALG_SHA, AW_HASH_HMAC_SHA1,0,0):
			phash->aw_func= aw_hmac_pack;
			phash->hm_para.e_md = (EVP_MD*)EVP_sha1();
			return NID_hmac_sha1;

		default :
			 return -1;
	}
}

static int aw_hash_creat_hw(AW_CIPHER_HASH_S* phash,
int alg, int mode, int key_len, int bw)
{
	int ret ,nid;

	/*Aw hw hmac need special data process, mask it */
	if ((mode >= AW_HMAC_BEGIN) && (mode <= AW_HMAC_END))
	{
		ALOGE("Mask HW hmac!\n");
		return -1;
	}

	phash->engine = openssl_engine_init();
	if (phash->engine == NULL) {
	 	ALOGE("NO engine mode is %d!\n",mode);
		return -1;
	}
	nid = map_netid_hash(phash,(u32)alg, (u32)mode, (u32)key_len, (u32)bw);
	if (nid == -1)
	{
		ALOGE("map_netid_hash err!\n");
		goto err;
	}

	phash->e_md = ENGINE_get_digest(phash->engine, nid);
	if (phash->e_md == NULL)
	{
		ALOGE("can not get hw method!\n");
		goto err;
	}

	ALOGI("success!!! aw_hash_creat_hw success...\n");
	return 0;
err:
	ALOGE("failed!!! aw_hash_creat_hw failed...\n");
	openssl_engine_free(phash->engine);
	return -1 ;
}

static int aw_hash_destroy_hw(AW_CIPHER_HASH_S* phash,
int alg, int mode, int key_len, int bw)
{
	openssl_engine_free(phash->engine);
	phash->engine = NULL;
	return 0;
}

static int aw_hash_creat_sw(AW_CIPHER_HASH_S* phash,
int alg, int mode, int key_len, int bw)
{
	int ret ,nid;
	nid = map_netid_hash(phash, alg, mode, key_len, bw);
	if (nid == -1)
	{
		ALOGE("failed!!! aw_hash_creat_sw failed\n");
		return -1 ;
	}
	return 0;
}

static int aw_hash_destroy_sw(AW_CIPHER_HASH_S* phash,
int alg, int mode, int key_len, int bw)
{
	ALOGI("trace: aw_cipher_destroy_sw...\n");
	return 0;
}

int AW_CIPHER_CreateHandle(void** pp,
AW_CIPHER_TYPE_S type)
{
    int ret = 0;
    AW_CIPHER_HANDLE_S **pphandle = (AW_CIPHER_HANDLE_S **)pp;

	ret = aw_check_init();
    if(0!= ret)
    {
    	ALOGE("Not init!\n");
    	return ret;
    }

    ret = 0;
	if((-1 == type_parsing(type)) || (!pphandle)) {
        ALOGE("Argu issue or Not supported cipher type!\n");
        return AW_ERR_CIPHER_NOT_SUPPORTED;
    }
	AW_CIPHER_HANDLE_S *hdl = malloc(sizeof(AW_CIPHER_HANDLE_S));
	if(!hdl){
        ALOGE("Invalid or not supported cipher type!\n");
        return AW_ERR_CIPHER_HEAP;
    }
    memset(hdl, 0, sizeof(AW_CIPHER_HANDLE_S));
	AW_MODULE_LOCK();
	hdl->phandle = hdl;
	list_add_tail(&hdl->node, &g_aw_cipher_handle_list);
	AW_HANDLE_MUTEX_CREAT(&hdl->handle_mutex);
	AW_HANDLE_LOCK(hdl->handle_mutex);
	AW_MODULE_UNLOCK();
	hdl->type = (AW_CIPHER_TYPE_S)type;
	if(type.hw == AW_CIPHER_HW)
	{
		switch(type.tp_sub1)
		{
			case AW_CIPHER_SUBTYPE1_CRYPTO:
				ret = aw_cipher_creat_hw(&hdl->aw_cipher_sub1.crypto,
			    hdl->type.alg, hdl->type.mode, hdl->type.key_len,
			    hdl->type.bw);
				break;
			case AW_CIPHER_SUBTYPE1_HASH:
				ret = aw_hash_creat_hw(&hdl->aw_cipher_sub1.hash,
				 hdl->type.alg, hdl->type.mode, 0, 0);
				break;
			default:
				break;
		}
	}
	else
	{
		switch(type.tp_sub1)
		{
			case AW_CIPHER_SUBTYPE1_CRYPTO:
				ret = aw_cipher_creat_sw(&hdl->aw_cipher_sub1.crypto,
			hdl->type.alg, hdl->type.mode, hdl->type.key_len,hdl->type.bw);
				break;
			case AW_CIPHER_SUBTYPE1_HASH:
				ret = aw_hash_creat_sw(&hdl->aw_cipher_sub1.hash,
				hdl->type.alg, hdl->type.mode, 0, 0);
				break;
			default:
				break;
		}
	}
	AW_HANDLE_UNLOCK(hdl->handle_mutex);

	if(ret)
	{
		*pphandle = NULL;
		hdl->status = 0;
		AW_CIPHER_DestroyHandle((void**)&hdl);
		ret = AW_ERR_CIPHER_OP_FAILED;
	}
	else
	{
		hdl->status = 1;
		*pphandle = hdl;
	}
    return ret;
}

static int find_node( struct list_head** ppnod ,
struct list_head* list, AW_CIPHER_HANDLE_S* phandle)
{
	struct list_head* pnod = NULL;
	AW_CIPHER_HANDLE_S*  ptr = NULL ;
	list_for_each(pnod, list)
	{
		ptr = container_of(pnod ,AW_CIPHER_HANDLE_S ,node);
		if (ptr->phandle == phandle)
		{
			ALOGI("found handle: %p!\n", phandle);
			*ppnod = pnod;
			return 1;
		}
	}
	return 0;
}

int AW_CIPHER_DestroyHandle(void** pp)
{
    int ret = 0 ,found = 1;
    struct list_head* pnod ;
    AW_CIPHER_HANDLE_S* phandle = NULL;
    AW_CIPHER_TYPE_S type;
    ret = aw_check_init();
    if(0!= ret)
    	return ret;
    ret = 0;
	if (( NULL == pp ) || (NULL == *(AW_CIPHER_HANDLE_S**)pp)){
		ALOGE("pphandle or phandle is NULL!\n");
		return AW_ERR_CIPHER_INVALID_ARG;
	}
	phandle = *(AW_CIPHER_HANDLE_S**)pp;
	type = phandle->type;
	AW_MODULE_LOCK();
	found = find_node(&pnod, &g_aw_cipher_handle_list, phandle);
	if (found == 0)
	{
		AW_MODULE_UNLOCK();
		goto NOTFOUNDHANDLE;
	}
	else
		list_del(pnod);

	if(phandle->status == 0)
	{
		ALOGW("phandle->status is 0!\n");
		AW_MODULE_UNLOCK();
		goto HANDLE_STATUS_FAIL;
	}

	ALOGI("handle resource free!\n");
	AW_HANDLE_LOCK(phandle->handle_mutex);
	AW_MODULE_UNLOCK();

	if (type.hw == AW_CIPHER_HW)
	{
		switch(type.tp_sub1)
		{
			case AW_CIPHER_SUBTYPE1_CRYPTO:
				ret = aw_cipher_destroy_hw(&phandle->aw_cipher_sub1.crypto,
				phandle->type.alg, phandle->type.mode,
				phandle->type.key_len, phandle->type.bw);
				break;
			case AW_CIPHER_SUBTYPE1_HASH:
				ret = aw_hash_destroy_hw(&phandle->aw_cipher_sub1.hash,
				phandle->type.alg, phandle->type.mode,
				phandle->type.key_len, phandle->type.bw);
				break;
			default:
				break;
		}
	}
	else
	{
		switch(type.tp_sub1)
		{
			case AW_CIPHER_SUBTYPE1_CRYPTO:
				ret = aw_cipher_destroy_sw(&phandle->aw_cipher_sub1.crypto,
				phandle->type.alg, phandle->type.mode,
				phandle->type.key_len, phandle->type.bw);
				break;
			case AW_CIPHER_SUBTYPE1_HASH:
				ret = aw_hash_destroy_sw(&phandle->aw_cipher_sub1.hash,
				phandle->type.alg, phandle->type.mode,
				phandle->type.key_len, phandle->type.bw);
				break;
			default:
				break;
		}
	}
	AW_HANDLE_UNLOCK(phandle->handle_mutex);

HANDLE_STATUS_FAIL:
	AW_HANDLE_MUTEX_DEL(&phandle->handle_mutex);
	free(phandle);
NOTFOUNDHANDLE:
	if (ret)
		ret = AW_ERR_CIPHER_OP_FAILED;
    return ret;
}

//config key data,iv data if necessary.
static int aw_config_handle(AW_CIPHER_HANDLE_S* phandle,
AW_CIPHER_CFG_S* cfg)
{
	AW_CIPHER_TYPE_S type = phandle->type;
	int ret = -1;
	int iv_len, klen;
	AW_CIPHER_CRYPTO_S *pcrypto =NULL;
	AW_CIPHER_HASH_S* phash = &phandle->aw_cipher_sub1.hash;
	switch(type.tp_sub1)
	{
		case AW_CIPHER_SUBTYPE1_CRYPTO:
			pcrypto = &(phandle->aw_cipher_sub1.crypto);
			EVP_CIPHER_CTX_init(&pcrypto->ctx);
			memcpy(&pcrypto->cip_cfg, cfg, sizeof(AW_CIPHER_CFG_S));

			if ((AW_CIPHER_KEY_SRC_SECHW == cfg->enKeySrc) &&
				(AW_CIPHER_HW == type.hw))
			{
				ret = EVP_CipherInit(&pcrypto->ctx, pcrypto->e_cipher,
				(unsigned char*)cfg->hw_key, cfg->u8pIv, cfg->enc);
			}
			else
			{
				klen = EVP_CIPHER_key_length(pcrypto->e_cipher);
				iv_len = EVP_CIPHER_iv_length(pcrypto->e_cipher);
				if ((cfg->ivlen < iv_len) || (cfg->klen< klen))
				{
					ALOGE("iv len or key len err.[cfg->ivlen:%d ]\
					[cfg->klen:%d ] [iv_len:%d ] [klen:%d ]!\n",\
					cfg->ivlen, cfg->klen, iv_len, klen);
					return -1;
				}
				ret = EVP_CipherInit(&pcrypto->ctx, pcrypto->e_cipher,
				cfg->u8pKey, cfg->u8pIv, cfg->enc);
			}
			break;
		case AW_CIPHER_SUBTYPE1_HASH:
			if (phash->aw_func && phash->hm_para.e_md)
			{
				if ((cfg->u8pKey) && (cfg->klen <= HMAC_MAX_KLEN))
				{
					memcpy(phash->hm_para.key, cfg->u8pKey, cfg->klen);
					phash->hm_para.key_len = cfg->klen;
					ret = 0;
				}
			}
			else
			{
				EVP_MD_CTX_init(&phash->ctx);
				EVP_DigestInit(&phash->ctx, phash->e_md);
				ret = 0;
			}
			break;
		default:
			break;
	}
	return ret;
}

int AW_CIPHER_ConfigHandle(void* ptr,
AW_CIPHER_CFG_S* cfg)
{
	struct list_head *pnod =NULL ;
	int found;
	int ret = 0;
	AW_CIPHER_HANDLE_S *phandle = (AW_CIPHER_HANDLE_S*)ptr;
	ret = aw_check_init();
    if(0!= ret)
    	return ret;
	if ((!cfg) || (!phandle))
		return AW_ERR_CIPHER_INVALID_ARG;
	AW_MODULE_LOCK();
	found = find_node(&pnod, &g_aw_cipher_handle_list, phandle);
	if (found == 0)
	{
		AW_MODULE_UNLOCK();
		ALOGE("!!!no node found %p!\n",phandle);
		return AW_ERR_CIPHER_INVALID_ARG;
	}
	AW_HANDLE_LOCK(phandle->handle_mutex);
	AW_MODULE_UNLOCK();
	ret = aw_config_handle(phandle, cfg);
	if (!ret)
	{
		phandle->status = AW_CIPHER_HANDLE_STATUS_CFGED;
	}
	AW_HANDLE_UNLOCK(phandle->handle_mutex);

	if (ret)
		ret = AW_ERR_CIPHER_OP_FAILED;
    return ret;
}


static int aw_cmd_hash(AW_CIPHER_HANDLE_S* phandle,
AW_CIPHER_OPS_S* cmd)
{
	AW_CIPHER_HASH_S *phash = &phandle->aw_cipher_sub1.hash;
	AW_HASH_DESC_S* pdesc = &cmd->ops.hash_ops.hash_desc;\
	switch (cmd->ops.crypto_ops.cmd)
	{
		case AW_HASH_CMD_DIGEST:
			if (phash->aw_func)
			{
				phash->aw_func(phash->hm_para.e_md, phash->hm_para.key,
				phash->hm_para.key_len, pdesc->in, pdesc->ilen,
				pdesc->md, pdesc->p_mdlen);
			}
			else
			{
				EVP_DigestUpdate(&phash->ctx, pdesc->in, pdesc->ilen);
				EVP_DigestFinal(&phash->ctx, pdesc->md, pdesc->p_mdlen);
			}
			break;
		default :
			break;
	}
	return 0;
}

static int aw_cmd(AW_CIPHER_HANDLE_S* phandle, AW_CIPHER_OPS_S* cmd)
{
	int ret = -1;
	int tmp_olen = 0;
	AW_CIPHER_TYPE_S type = phandle->type;
	AW_CIPHER_CRYPTO_S *pcrypto = NULL;
	ret = aw_check_init();
    if(0!= ret)
    	return ret;

	switch(type.tp_sub1)
	{
		case AW_CIPHER_SUBTYPE1_CRYPTO:
			switch (cmd->ops.crypto_ops.cmd)
			{
				case AW_CIPHER_CMD_STRM_UPDATE:
					if ((phandle->status == AW_CIPHER_HANDLE_STATUS_CFGED)||
					(phandle->status == AW_CIPHER_HANDLE_STATUS_IOING))
					{
						pcrypto = &phandle->aw_cipher_sub1.crypto;
						if (pcrypto->cip_cfg.enc)
						{
							EVP_EncryptUpdate(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out,
							cmd->ops.crypto_ops.packet_desc.olen,
							cmd->ops.crypto_ops.packet_desc.in,
							cmd->ops.crypto_ops.packet_desc.ilen);
						}
						else
						{
							EVP_DecryptUpdate(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out,
							cmd->ops.crypto_ops.packet_desc.olen,
							cmd->ops.crypto_ops.packet_desc.in,
							cmd->ops.crypto_ops.packet_desc.ilen);
						}
						phandle->status = AW_CIPHER_HANDLE_STATUS_IOING;
					}
					else
					{
						ALOGE("handle status issue %p!\n");
						ret = -1;
					}
					break;
				case AW_CIPHER_CMD_STRM_FINAL:
					if ((phandle->status == AW_CIPHER_HANDLE_STATUS_IOING))
					{
						pcrypto = &phandle->aw_cipher_sub1.crypto;
						if (pcrypto->cip_cfg.enc)
						{
							EVP_EncryptFinal(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out,
							cmd->ops.crypto_ops.packet_desc.olen);
						}
						else
						{
							EVP_DecryptFinal(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out,
							cmd->ops.crypto_ops.packet_desc.olen);
						}
						phandle->status = AW_CIPHER_HANDLE_STATUS_IOED;
					}
					else
					{
						ALOGE("handle status issue %p!\n");
						ret = -1;
					}
					break;
				case AW_CIPHER_CMD_PACKET_SINGLE:
					if (phandle->status == AW_CIPHER_HANDLE_STATUS_CFGED)
					{
						pcrypto = &phandle->aw_cipher_sub1.crypto;
						if (pcrypto->cip_cfg.enc)
						{
							EVP_EncryptUpdate(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out,
							&tmp_olen,
							cmd->ops.crypto_ops.packet_desc.in,
							cmd->ops.crypto_ops.packet_desc.ilen);
							EVP_EncryptFinal(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out+tmp_olen,
							cmd->ops.crypto_ops.packet_desc.olen);
							*cmd->ops.crypto_ops.packet_desc.olen +=
							tmp_olen;
						}
						else
						{
							EVP_DecryptUpdate(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out,
							&tmp_olen,
							cmd->ops.crypto_ops.packet_desc.in,
							cmd->ops.crypto_ops.packet_desc.ilen);
							EVP_DecryptFinal(&pcrypto->ctx,
							cmd->ops.crypto_ops.packet_desc.out+tmp_olen,
							cmd->ops.crypto_ops.packet_desc.olen);
							*cmd->ops.crypto_ops.packet_desc.olen +=
							tmp_olen;
						}
						phandle->status = AW_CIPHER_HANDLE_STATUS_IOED;
					}
					else
					{
						ALOGE("handle status issue %p!\n");
						ret = -1;
					}
					break;
				default:
					break;
			}
			break;
		case AW_CIPHER_SUBTYPE1_HASH:
			ret = aw_cmd_hash(phandle, cmd);
			break;
		default:
			break;
	}
	return ret;
}

int AW_CIPHER_SendCmd(void* ptr, AW_CIPHER_OPS_S* cmd)
{
	struct list_head *pnod ;
	int found;
	int ret = 0;
	AW_CIPHER_HANDLE_S * phdl = (AW_CIPHER_HANDLE_S*)ptr;

	if ((!cmd) || (!phdl))
		return AW_ERR_CIPHER_INVALID_ARG;
	AW_MODULE_LOCK();
	found = find_node(&pnod, &g_aw_cipher_handle_list, phdl);
	if (found == 0)
	{
		AW_MODULE_UNLOCK();
		return AW_ERR_CIPHER_INVALID_ARG;
	}
	AW_HANDLE_LOCK(phdl->handle_mutex);
	AW_MODULE_UNLOCK();
	ret = aw_cmd(phdl, cmd);
	AW_HANDLE_UNLOCK(phdl->handle_mutex);
	if (ret)
		ret = AW_ERR_CIPHER_OP_FAILED;
	return ret;
}

