/*
 * ===========================================================================================
 *
 *       Filename:  armeabi.c
 *
 *    Description:  armeabi test.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-12 16:31:38
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-05-27 16:29:46
 *
 * ===========================================================================================
 */

#include <rtthread.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>
#include <pipe.h>
#include <log.h>

int main_sample_multi_vi2venc2muxer(int argc, char** argv);
int main_sample_vi2venc2muxer(int argc, char** argv);
int main_sample_vi_g2d(int argc, char *argv[]);
int main_sample_demux2vdec2vo(int argc, char** argv);

int main_sample_multi_vi2venc2muxer_burnin(int argc, char** argv)
{
    char *argvv[1] =
    {
	"product",
    };

    unsigned int counter = 0;

    while(1)
    {
        main_sample_multi_vi2venc2muxer(1, argvv);
        counter ++;
        printf("%s line %d, test %d times.\n", __func__, __LINE__, counter);

        rt_thread_delay(10);
    }
    return 0;
}

int main_sample_vi2venc2muxer_h265_burnin(int argc, char** argv)
{
    char *argvv[2] =
    {
	"product",
	"h265",
    };

    unsigned int counter = 0;

    while(1)
    {
        main_sample_vi2venc2muxer(2, argvv);
        counter ++;
        printf("%s line %d, test %d times.\n", __func__, __LINE__, counter);

        rt_thread_delay(10);
    }
    return 0;
}

int main_sample_vi2venc2muxer_h264_burnin(int argc, char** argv)
{
    char *argvv[1] =
    {
	"product",
    };

    unsigned int counter = 0;

    while(1)
    {
        main_sample_vi2venc2muxer(1, argvv);
        counter ++;
        printf("%s line %d, test %d times.\n", __func__, __LINE__, counter);

        rt_thread_delay(10);
    }
    return 0;
}

int main_sample_vi_g2d_burnin(int argc, char** argv)
{
    char *argvv[3] =
    {
	"product",
	"-path",
	"/mnt/E/sample_vi_g2d.conf"
    };

    unsigned int counter = 0;

    while(1)
    {
        main_sample_vi_g2d(3, argvv);
        counter ++;
        printf("%s line %d, test %d times.\n", __func__, __LINE__, counter);

        rt_thread_delay(10);
    }
    return 0;
}

int main_sample_burnin_play(int argc, char** argv)
{
    char *exe = "burnin_player";
    static int times = 0;

    while(1)
    {
	main_sample_demux2vdec2vo(1, &exe);
	times ++;
	printf("%s line %d, burnin test %d times\n", __func__, __LINE__, times);
	rt_thread_delay(10);
    }
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(main_sample_multi_vi2venc2muxer_burnin, __cmd_sample_multi_vi2venc2muxer_burnin, burnin test multiple video encode);
FINSH_FUNCTION_EXPORT_ALIAS(main_sample_vi2venc2muxer_h265_burnin, __cmd_sample_vi2venc2muxer_h265_burnin, burnin test h265 video encode);
FINSH_FUNCTION_EXPORT_ALIAS(main_sample_vi2venc2muxer_h264_burnin, __cmd_sample_vi2venc2muxer_h264_burnin, burnin test h264 video encode);
FINSH_FUNCTION_EXPORT_ALIAS(main_sample_vi_g2d_burnin, __cmd_sample_vi_g2d_burnin, burnin test vi rotate);
FINSH_FUNCTION_EXPORT_ALIAS(main_sample_burnin_play, __cmd_sample_burnin_play, burnin test of play stream);
