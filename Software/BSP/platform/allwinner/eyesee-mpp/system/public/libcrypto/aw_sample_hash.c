#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <assert.h>
#include <sys/time.h>

#include <crypto/aw_cipher.h>

#define AW_ERR_CIPHER(format, arg...)    \
		printf( "%s,%d: " format , __FUNCTION__, __LINE__, ## arg)
#define AW_INFO_CIPHER(format, arg...)    \
		printf( "%s,%d: " format , __FUNCTION__, __LINE__, ## arg)

static void print_time (const char *pu8Name, unsigned int u32ByteNum)
{
    struct timeval tv;
    struct timeval interval;
    static struct timeval last_tv = {0};

    if (pu8Name != NULL)
    {
        gettimeofday (&tv, NULL);

        if (pu8Name != NULL)
        {
            if(tv.tv_usec < last_tv.tv_usec)
            {
                interval.tv_sec = tv.tv_sec - 1 - last_tv.tv_sec;
                interval.tv_usec = tv.tv_usec + 1000000 - last_tv.tv_usec;
            }
            else
            {
                interval.tv_sec = tv.tv_sec - last_tv.tv_sec;
                interval.tv_usec = tv.tv_usec - last_tv.tv_usec;
            }
            printf("[%s] Bytes: %ud, %02d.%02d, interval: %02d.%06d\r\n",
            		pu8Name, u32ByteNum,
                	(unsigned int)tv.tv_sec, (unsigned int)tv.tv_usec,
                	(unsigned int)interval.tv_sec,
                	(unsigned int)interval.tv_usec);
        }
    }
    gettimeofday (&last_tv, NULL);

}

static int printBuffer(const unsigned char *string, const unsigned char
						*pu8Input, unsigned int u32Length)
{
    unsigned int i = 0;

    if ( NULL != string )
    {
        printf("%s\n", string);
    }

    for ( i = 0 ; i < u32Length; i++ )
    {
        if( (i % 16 == 0) && (i != 0)) printf("\n");
        printf("0x%02x ", pu8Input[i]);
    }
    printf("\n");

    return 0;
}

static const char src_vect[][128] = {
    {"abc"},
    {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"},
    {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"}
};

static const unsigned char dst_vect[][20] =
{
	/*dst_vect[0]*/
    {0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
    0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D},
    /*dst_vect[1]*/
    {0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E, 0xBA, 0xAE,
    0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70,0xF1},
    /*dst_vect[2]*/
    {0xaf, 0xc5, 0x3a, 0x4e, 0xa2, 0x08, 0x56, 0xf9, 0x8e, 0x08,
    0xdc, 0x6f, 0x3a, 0x5c, 0x98, 0x33, 0x13, 0x77, 0x68, 0xed},
};

static unsigned char hmac_key[16] = {0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				0xaa, 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa};

static unsigned char hmac_data[50]= {
	 0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,
	 0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,
	 0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,
	 0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,
	 0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,
	 0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,
	 0xdd,0xdd};

static unsigned char hmac_sha1_result[20] = \
			 {0xd7,0x30,0x59,0x4d,0x16,0x7e,0x35,0xd5,0x95,0x6f,\
			  0xd8,0x00,0x3d,0x0d,0xb3,0xd3,0xf4,0x6d,0xc7,0xbb};

#define TEST_ROUND_CNT 20
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define TEST_HANDLE_NUM 20
void* g_phdl[TEST_HANDLE_NUM] ;
int aw_hash_testroot()
{

    int ret = 0;

	unsigned int i= 0 ,j =0;
	void * ptesthandle = NULL;
	AW_CIPHER_TYPE_S type ;
	aw_construct_type(type, AW_CIPHER_HW, AW_CIPHER_SUBTYPE1_HASH,
	AW_CIPHER_ALG_SHA, AW_HASH_SHA1, 0, 0);


	AW_INFO_CIPHER("Handle resource leak test begin \n");
	AW_INFO_CIPHER("\n stage 1 begin... \n");
	for( i = 0; i < TEST_ROUND_CNT; i++)
	{ 
	    ret = AW_CIPHER_Init();
	    if(ret)
	    	AW_INFO_CIPHER("failed \n");
	    ret = AW_CIPHER_CreateHandle(&ptesthandle, type);

		if(ret)
	   		AW_INFO_CIPHER("failed :ptesthandle is %p\n", ptesthandle);
	    
	    ret = AW_CIPHER_DestroyHandle(&ptesthandle);
	    if(ret)
	    	AW_INFO_CIPHER("failed :ptesthandle is %p\n", ptesthandle);
	    
		AW_CIPHER_DeInit();
	}
	AW_INFO_CIPHER("\n stage 1 end... \n");
	
	AW_INFO_CIPHER("\nstage 2 begin.... \n");
	for( i = 0; i < TEST_ROUND_CNT; i++)
	{ 
	    ret += AW_CIPHER_Init();
	    for(j = 0; j < TEST_HANDLE_NUM; j++)
	    {
			ret += AW_CIPHER_CreateHandle(&g_phdl[j], type);
			AW_INFO_CIPHER("ptesthandle is %p\n", ptesthandle);
	    }

	    for(j = 0; j < TEST_HANDLE_NUM; j++)
	    	ret += AW_CIPHER_DestroyHandle(&g_phdl[j]);
	    
		ret += AW_CIPHER_DeInit();
	}
	AW_INFO_CIPHER("\nstage 2 end .... ret acc is %d \n",ret);

	
	AW_INFO_CIPHER("stage 3 begin... \n");
	aw_construct_type(type, AW_CIPHER_HW, AW_CIPHER_SUBTYPE1_CRYPTO,
	AW_CIPHER_ALG_AES, AW_CIPHER_MODE_ECB, AW_CIPHER_KEY_AES_128BIT,
	AW_CIPHER_BIT_WIDTH_128BIT);

	for( i = 0; i < TEST_ROUND_CNT; i++)
	{ 
	    AW_CIPHER_Init();
        if(ret)
    		AW_INFO_CIPHER("failed \n");
    		
	    ret = AW_CIPHER_CreateHandle(&ptesthandle, type);
		if(ret)
			AW_INFO_CIPHER("failed :ptesthandle is %p\n", ptesthandle);
			
	    if (i < 5)
	    AW_INFO_CIPHER("ptesthandle is %p\n", ptesthandle);
	    
	    ret = AW_CIPHER_DestroyHandle(&ptesthandle);
		if(ret)
			AW_INFO_CIPHER("failed :ptesthandle is %p\n", ptesthandle);
			
		AW_CIPHER_DeInit();
	}
	/*full test must cover all valid algs modes keylens...*/
	AW_INFO_CIPHER("Handle resource leak test end\n");

	ret = 0;
	AW_CIPHER_CFG_S cfg;
	AW_CIPHER_OPS_S cmd;
	unsigned int len = 0;
	unsigned char md[20] = {0};

	for(j = 0 ;j < AW_CIPHER_TYPE_BUTT; j++)
	{
		for(i= 0; i < ARRAY_SIZE(src_vect); i++)
		{
			aw_construct_type(type,(AW_CIPHER_TYPE_E)j,
			AW_CIPHER_SUBTYPE1_HASH, AW_CIPHER_ALG_SHA, AW_HASH_SHA1, 0, 0);

			AW_CIPHER_Init();
			ret = AW_CIPHER_CreateHandle(&ptesthandle, type);

			if (ret)
			{
				AW_INFO_CIPHER("!!!creat  handle failed ret is %d\n",ret);
				break;
			}
			
			AW_INFO_CIPHER("creat hash handle success ret is %d\n",ret);
			
			memset( &cfg, 0, sizeof(cfg));
			ret = AW_CIPHER_ConfigHandle(ptesthandle, &cfg);
			AW_INFO_CIPHER("Try AW_CIPHER_ConfigHandle ret is %d\n",ret);

			memset( &cmd, 0, sizeof(cmd));
			cmd.ops.hash_ops.cmd = AW_HASH_CMD_DIGEST;
		
			cmd.ops.hash_ops.hash_desc.ilen = strlen(&src_vect[i][0]);
			cmd.ops.hash_ops.hash_desc.in =(unsigned char*)&src_vect[i][0];
			cmd.ops.hash_ops.hash_desc.md = md;
			cmd.ops.hash_ops.hash_desc.p_mdlen = &len; 
			ret = AW_CIPHER_SendCmd(ptesthandle, &cmd);
			if (memcmp(&dst_vect[i][0], md, 20) == 0)
			{
				AW_INFO_CIPHER("Sha1 success ret:%d len:%d\n", ret, len);
			}
			else
			{
				AW_INFO_CIPHER("Sha1 failed ret:%d len:%d\n", ret, len);
				printBuffer((const unsigned char*)"message degist is:",
				md, 20);
			}
			AW_CIPHER_DestroyHandle(&ptesthandle);
			AW_CIPHER_DeInit();
		}
	}
	
	AW_INFO_CIPHER("hmac-sha1 begin \n");
	aw_construct_type(type, AW_CIPHER_SW,AW_CIPHER_SUBTYPE1_HASH,
	AW_CIPHER_ALG_SHA, AW_HASH_HMAC_SHA1, 0, 0);

	AW_CIPHER_Init();
	ret = AW_CIPHER_CreateHandle(&ptesthandle, type);
	AW_INFO_CIPHER("hmac-sha1 begin \n");
	if (ret)
	{
		AW_INFO_CIPHER("!!!creat  handle failed ret is %d\n",ret);
	}
		
	AW_INFO_CIPHER("creat hash handle success ret is %d\n",ret);
	
	memset( &cfg, 0, sizeof(cfg));
	cfg.enKeySrc = AW_CIPHER_KEY_SRC_RAM;
	cfg.klen = 16;
	cfg.u8pKey = hmac_key;
	ret = AW_CIPHER_ConfigHandle(ptesthandle, &cfg);
	
	AW_INFO_CIPHER("Try AW_CIPHER_ConfigHandle ret is %d\n",ret);

	memset( &cmd, 0, sizeof(cmd));
	cmd.ops.hash_ops.cmd = AW_HASH_CMD_DIGEST;

	cmd.ops.hash_ops.hash_desc.ilen = 50;
	cmd.ops.hash_ops.hash_desc.in =hmac_data;
	cmd.ops.hash_ops.hash_desc.md = md;
	cmd.ops.hash_ops.hash_desc.p_mdlen = &len; 
	ret = AW_CIPHER_SendCmd(ptesthandle, &cmd);
	if (memcmp(hmac_sha1_result, md, 20) == 0)
	{
		AW_INFO_CIPHER("hmac success ret:%d len:%d\n", ret, len);
	}
	else
	{
		AW_INFO_CIPHER("hmac failed ret:%d len:%d\n", ret, len);
		printBuffer((const unsigned char*)"message degist is:",
		md, 20);
	}
	AW_CIPHER_DestroyHandle(&ptesthandle);
	AW_CIPHER_DeInit();

	AW_INFO_CIPHER("hmac-sha1 end \n");	
    return ret;
}

int aw_cipher_sha_entry(int argc, char* argv[])
{
	AW_INFO_CIPHER("AW HASH test begin....................\n");
	aw_hash_testroot();
	AW_INFO_CIPHER("AW HASH test end....................\n"); 
    return 0;
}
