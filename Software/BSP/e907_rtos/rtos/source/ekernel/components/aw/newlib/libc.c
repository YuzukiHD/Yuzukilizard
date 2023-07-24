#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>

#ifdef RT_USING_PTHREADS
#include <pthread.h>
#endif

#ifdef RT_USING_DFS
#include <dfs_posix.h>

#ifdef RT_USING_DFS_DEVFS
/* #include <devfs.h> */
#endif

#endif

int libc_system_init(void)
{
    return 0;
}
INIT_COMPONENT_EXPORT(libc_system_init);
