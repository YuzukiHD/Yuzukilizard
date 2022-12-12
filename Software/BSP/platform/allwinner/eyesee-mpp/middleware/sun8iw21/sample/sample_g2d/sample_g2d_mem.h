#ifndef __G2D_MEM_H__
#define __G2D_MEM_H__

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */


int g2d_MemOpen(void);
int g2d_MemClose(void);
unsigned char* g2d_allocMem(unsigned int size); 
int g2d_freeMem(void *vir_ptr);
unsigned int g2d_getPhyAddrByVirAddr(void *vir_ptr);
int g2d_flushCache(void *vir_ptr, unsigned int size); 


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //  #ifndef __G2D_MEM_H__
