#ifndef __VENC_MEM_H__
#define __VENC_MEM_H__

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */


int venc_MemOpen(void);
int venc_MemClose(void);
unsigned char* venc_allocMem(unsigned int size);
int venc_freeMem(void *ptr);
unsigned int venc_getPhyAddrByVirAddr(void *pVirAddr);
int venc_flushCache(void *vir_ptr, unsigned int size);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //  #ifndef __VENC_MEM_H__