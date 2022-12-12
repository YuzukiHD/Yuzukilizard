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

#define AW_ERR_CIPHER(format, arg...) \
		printf( "%s,%d: \n" format , __FUNCTION__, __LINE__, ## arg)
#define AW_INFO_CIPHER(format, arg...) \
		printf( "%s,%d: \n" format , __FUNCTION__, __LINE__, ## arg)

#define AES_KEY_SIZE_128        16
#define AES_KEY_SIZE_192        24
#define AES_KEY_SIZE_256        32
#define AES_BLOCK_SIZE   16

#define BYTES_PER_UPDATE 1024

/*simple function test first*/

static unsigned char g_key[AES_KEY_SIZE_256] = {
				0xFE, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
				0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
				0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
				0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
static unsigned char g_iv[AES_BLOCK_SIZE] = {
				0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
				0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

#define TS_FILE_PATH "/mnt/extsd/clear.bin"
#define TS_FILE_ENC_PATH "/mnt/extsd/enc.bin"

static  int getfile_size(char *path)
{

	FILE *file ;
	int size ;

	if(!(file=fopen(path, "rb"))){
		return -1 ;
	}

	if(fseek(file, 0L, SEEK_END) == -1)
		return -1 ;

	size= ftell(file);

	fclose(file);
	return size ;
}

int aw_cipher_entry(int argc, char* argv[])
{
	int ret = 0;
	unsigned int i = 0;
	int olen = 0, fsize = 0;
	unsigned char tmpbuf[BYTES_PER_UPDATE];
	unsigned char pt_out[BYTES_PER_UPDATE];
	void * ptesthandle = NULL;
	AW_CIPHER_CFG_S cfg;
	AW_CIPHER_OPS_S cmd;
	AW_CIPHER_TYPE_S type ;
	int fd = -1 ,fdout = -1;
	aw_construct_type(type, AW_CIPHER_HW, AW_CIPHER_SUBTYPE1_CRYPTO,
	AW_CIPHER_ALG_AES, AW_CIPHER_MODE_CBC_CTS, AW_CIPHER_KEY_AES_256BIT,
	AW_CIPHER_BIT_WIDTH_128BIT);
	
	AW_CIPHER_Init();
	ret = AW_CIPHER_CreateHandle(&ptesthandle, type);
	if ((ret) || (!ptesthandle))
	{
		AW_ERR_CIPHER("Create handle failed\n");
	}
	memset( &cfg, 0, sizeof(cfg));
	cfg.enc = 1;
	cfg.enKeySrc = AW_CIPHER_KEY_SRC_RAM;
	cfg.ivlen = AES_BLOCK_SIZE;
	cfg.klen = AES_KEY_SIZE_256;
	cfg.u8pIv = g_iv;
	cfg.u8pKey = g_key;
	ret = AW_CIPHER_ConfigHandle(ptesthandle, &cfg);
	AW_INFO_CIPHER("Try AW_CIPHER_ConfigHandle ret is %d\n",ret);

	fsize = getfile_size(TS_FILE_PATH);

	if (fsize > 0)
		fd = open(TS_FILE_PATH, O_RDONLY, 0644);
	else
	{	
		AW_ERR_CIPHER("No ts file to be encrypted found!!\
		,path:%s",TS_FILE_PATH);
		goto NOFILE;
	}
			
	olen = 0;
	fdout = open(TS_FILE_ENC_PATH, O_CREAT | O_TRUNC |O_RDWR, 0777);
	assert(fdout > 0);
	int rd_len = 0;
	while(1)
	{
		rd_len = read(fd, tmpbuf, BYTES_PER_UPDATE);
		if (rd_len <= 0)
			break;
		else if ((rd_len % 16) != 0)
		{
				rd_len -= (rd_len % 16);
				rd_len += 16;
		}

		rd_len = (rd_len > BYTES_PER_UPDATE) ? BYTES_PER_UPDATE : rd_len;
		memset(&cmd, 0, sizeof(cmd));
		cmd.ops.crypto_ops.cmd = AW_CIPHER_CMD_STRM_UPDATE;
		cmd.ops.crypto_ops.packet_desc.in = tmpbuf;
		cmd.ops.crypto_ops.packet_desc.ilen = rd_len;
		cmd.ops.crypto_ops.packet_desc.olen = &olen;
		cmd.ops.crypto_ops.packet_desc.out = pt_out;
		ret = AW_CIPHER_SendCmd(ptesthandle, &cmd);
		if (olen == rd_len)
		{
			write(fdout, pt_out, olen);	
		}	
		else
		{
			AW_ERR_CIPHER("!!! olen: %d , rd_len :%d",olen, rd_len);
			break;
		}
		AW_INFO_CIPHER("Try AW_CIPHER_SendCmd :AW_CIPHER_CMD_STRM_UPDATE\
						ret is %d i: %d\n",ret,i);
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.ops.crypto_ops.cmd = AW_CIPHER_CMD_STRM_FINAL;
	cmd.ops.crypto_ops.packet_desc.olen = &olen;
	cmd.ops.crypto_ops.packet_desc.out = pt_out;
	ret = AW_CIPHER_SendCmd(ptesthandle, &cmd);
	
	AW_INFO_CIPHER("failed AW_CIPHER_SendCmd :AW_CIPHER_CMD_STRM_FINAL\
					ret is %d\n",ret);
					
	if (olen > 0)
		write(fdout, pt_out, olen);
	else
		AW_ERR_CIPHER("!!! olen: %d , rd_len :%d",olen, rd_len);

	ret = AW_CIPHER_DestroyHandle(&ptesthandle);
	if(ret) 
		AW_INFO_CIPHER("failed AW_CIPHER_DestroyHandle");
NOFILE:		
	close(fd);
	close(fdout);

	AW_CIPHER_DeInit();
	return 0;
}

