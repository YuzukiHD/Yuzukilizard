#ifndef __SYS_SVC__
#define __SYS_SVC__

//--------------------------------------------------------------------
//      resource management sevices
//--------------------------------------------------------------------
// work mode for resource request.
#define RESOURCE_REQ_MODE_WAIT     0
#define RESOURCE_REQ_MODE_NWAIT    1

// resources define
#define RESOURCE_VE_HW              (1<<0)

int32_t svc_init(void);
int32_t svc_exit(void);

void    SVC_default(void);



#endif
