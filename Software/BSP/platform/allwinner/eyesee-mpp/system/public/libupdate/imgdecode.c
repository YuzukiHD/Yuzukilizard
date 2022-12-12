#include <string.h>
#include <malloc.h>
#include "imgdecode.h"
#include "imagefile_new.h"

#define HEAD_ID             0       //头加密接口索引
#define TABLE_ID            1       //表加密接口索引
#define DATA_ID             2       //数据加密接口索引
#define IF_CNT              3       //加密接口个数  现在只有头加密，表加密，数据加密3种
#define MAX_KEY_SIZE        32      //密码长度

#pragma pack(push, 1)
typedef struct tag_IMAGE_HANDLE
{
    ImageHead_t  ImageHead;     //img头信息
    ImageItem_t *ItemTable;     //item信息表
}IMAGE_HANDLE;

#define INVALID_INDEX       0xFFFFFFFF


typedef struct tag_ITEM_HANDLE{
    uint    index;                  //在ItemTable中的索引
    uint    reserved[3];
//  long long pos;
}ITEM_HANDLE;

#define ITEM_PHOENIX_TOOLS    "PXTOOLS "

FILE *img_file_hd;
//------------------------------------------------------------------------------------------------------------
//image解析插件的接口
//------------------------------------------------------------------------------------------------------------
HIMAGE  Img_Open    (char * ImageFile)
{
    IMAGE_HANDLE * pImage = NULL;
    uint ItemTableSize;                 //固件索引表的大小

    img_file_hd = fopen(ImageFile, "rb");
    if(img_file_hd == NULL)
    {
        printf("sunxi sprite error: unable to open firmware file\n");

        return NULL;
    }
    pImage = (IMAGE_HANDLE *)malloc(sizeof(IMAGE_HANDLE));
    if (NULL == pImage)
    {
        printf("sunxi sprite error: fail to malloc memory for img head\n");

        return NULL;
    }
    memset(pImage, 0, sizeof(IMAGE_HANDLE));
    //------------------------------------------------
    //读img头
    //------------------------------------------------
    //debug("try to read mmc start %d\n", img_file_start);
    if(!fread(&pImage->ImageHead, IMAGE_HEAD_SIZE, 1, img_file_hd))
    {
        printf("sunxi sprite error: read iamge head fail\n");

        goto _img_open_fail_;
    }
    //------------------------------------------------
    //比较magic
    //------------------------------------------------
    if (memcmp(pImage->ImageHead.magic, IMAGE_MAGIC, 8) != 0)
    {
        printf("sunxi sprite error: iamge magic is bad\n");

        goto _img_open_fail_;
    }
    //------------------------------------------------
    //为索引表开辟空间
    //------------------------------------------------
    ItemTableSize = pImage->ImageHead.itemcount * sizeof(ImageItem_t);
    pImage->ItemTable = (ImageItem_t*)malloc(ItemTableSize);
    if (NULL == pImage->ItemTable)
    {
        printf("sunxi sprite error: fail to malloc memory for item table\n");

        goto _img_open_fail_;
    }
    //------------------------------------------------
    //读出索引表
    //------------------------------------------------
    if(!fread(pImage->ItemTable, ItemTableSize, 1, img_file_hd))
    {
        printf("sunxi sprite error: read iamge item table fail\n");

        goto _img_open_fail_;
    }

    return pImage;

_img_open_fail_:
    if(pImage->ItemTable)
    {
        free(pImage->ItemTable);
    }
    if(pImage)
    {
        free(pImage);
    }

    return NULL;
}
long long Img_GetSize   (HIMAGE hImage)
{
    long long       size;

    if (NULL == hImage)
    {
        printf("sunxi sprite error : hImage is NULL\n");

        return -1;
    }
	IMAGE_HANDLE* pImage = (IMAGE_HANDLE *)hImage;
    size = pImage->ImageHead.lenHi;
    size <<= 32;
    size |= pImage->ImageHead.lenLo;

    return size;
}
HIMAGEITEM  Img_OpenItem    (HIMAGE hImage, char * MainType, char * subType)
{
    IMAGE_HANDLE* pImage = (IMAGE_HANDLE *)hImage;
    ITEM_HANDLE * pItem  = NULL;
    uint          i;

    if (NULL == pImage || NULL == subType)
    {
        return NULL;
    }

    pItem = (ITEM_HANDLE *) malloc(sizeof(ITEM_HANDLE));
    if (NULL == pItem)
    {
        printf("sunxi sprite error : cannot malloc memory for item\n");

        return NULL;
    }
    pItem->index = INVALID_INDEX;

    for (i = 0; i < pImage->ImageHead.itemcount ; i++)
    {
        if(!memcmp(subType,  pImage->ItemTable[i].subType,  SUBTYPE_LEN))
        {
            pItem->index = i;
            return pItem;
        }
    }

    printf("sunxi sprite error : cannot find item %s %s\n", MainType, subType);

    free(pItem);
    pItem = NULL;

    return NULL;
}
long long Img_GetItemSize   (HIMAGE hImage, HIMAGEITEM hItem)
{
    IMAGE_HANDLE* pImage = (IMAGE_HANDLE *)hImage;
    ITEM_HANDLE * pItem  = (ITEM_HANDLE  *)hItem;
    long long       size;

    if (NULL == pItem)
    {
        printf("sunxi sprite error : item is NULL\n");

        return 0;
    }

    size = pImage->ItemTable[pItem->index].filelenHi;
    size <<= 32;
    size |= pImage->ItemTable[pItem->index].filelenLo;

    return size;
}

uint Img_GetItemStart   (HIMAGE hImage, HIMAGEITEM hItem)
{
    IMAGE_HANDLE* pImage = (IMAGE_HANDLE *)hImage;
    ITEM_HANDLE * pItem  = (ITEM_HANDLE  *)hItem;
    long long       start;
    long long       offset;

    if (NULL == pItem)
    {
        printf("sunxi sprite error : item is NULL\n");

        return 0;
    }
    offset = pImage->ItemTable[pItem->index].offsetHi;
    offset <<= 32;
    offset |= pImage->ItemTable[pItem->index].offsetLo;
    start = offset/512;

    return (uint)start;
}

uint Img_ReadItem(HIMAGE hImage, HIMAGEITEM hItem, void *buffer, uint buffer_size)
{
    IMAGE_HANDLE* pImage = (IMAGE_HANDLE *)hImage;
    ITEM_HANDLE * pItem  = (ITEM_HANDLE  *)hItem;
    long long     start;
    long long     offset;
    uint          file_size;

    if (NULL == pItem)
    {
        printf("sunxi sprite error : item is NULL\n");

        return 0;
    }
    if(pImage->ItemTable[pItem->index].filelenHi)
    {
        printf("sunxi sprite error : the item too big\n");

        return 0;
    }
    file_size = pImage->ItemTable[pItem->index].filelenLo;
    file_size = (file_size + 1023) & (~(1024 - 1));
    if(file_size > buffer_size)
    {
        printf("sunxi sprite error : buffer is smaller than data size\n");

        return 0;
    }
    offset = pImage->ItemTable[pItem->index].offsetHi;
    offset <<= 32;
    offset |= pImage->ItemTable[pItem->index].offsetLo;
    //start = offset/512;
    start = offset;

    fseek(img_file_hd, (uint)start, SEEK_SET);
    if(!fread(buffer, file_size, 1, img_file_hd))
    {
        printf("sunxi sprite error : read item data failed\n");

        return 0;
    }

    return file_size;
}

uint Img_ReadItem_Continue(HIMAGE hImage, HIMAGEITEM hItem, void *buffer, uint wanted_size, uint file_offset)
{
    IMAGE_HANDLE* pImage = (IMAGE_HANDLE *)hImage;
    ITEM_HANDLE * pItem  = (ITEM_HANDLE  *)hItem;
    long long     offset;

    if (NULL == pItem)
    {
        printf("sunxi sprite error : item is NULL\n");

        return 0;
    }
    if(pImage->ItemTable[pItem->index].filelenHi)
    {
        printf("sunxi sprite error : the item too big\n");

        return 0;
    }

    offset = pImage->ItemTable[pItem->index].offsetHi;
    offset <<= 32;
    offset |= pImage->ItemTable[pItem->index].offsetLo;

    fseek(img_file_hd, (uint)offset + file_offset, SEEK_SET);
    if(!fread(buffer, wanted_size, 1, img_file_hd))
    {
        printf("sunxi sprite error : read item data failed\n");

        return 0;
    }

    return wanted_size;
}
int Img_CloseItem   (HIMAGE hImage, HIMAGEITEM hItem)
{
    ITEM_HANDLE * pItem = (ITEM_HANDLE *)hItem;
    if (NULL == pItem)
    {
        printf("sunxi sprite error : item is null when close it\n");

        return -1;
    }
    free(pItem);
    pItem = NULL;

    return 0;
}
void  Img_Close (HIMAGE hImage)
{
    IMAGE_HANDLE * pImage = (IMAGE_HANDLE *)hImage;

    if (NULL == pImage)
    {
        printf("sunxi sprite error : imghead is null when close it\n");

        return ;
    }

    if (NULL != pImage->ItemTable)
    {
        free(pImage->ItemTable);
        pImage->ItemTable = NULL;
    }

    memset(pImage, 0, sizeof(IMAGE_HANDLE));
    free(pImage);
    pImage = NULL;

    if(img_file_hd)
    {
        fclose(img_file_hd);
    }

    return ;
}
