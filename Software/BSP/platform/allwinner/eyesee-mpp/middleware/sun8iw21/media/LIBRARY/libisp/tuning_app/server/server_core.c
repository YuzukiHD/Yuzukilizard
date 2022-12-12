#include "../socket_protocol.h"
#include "../thread_pool.h"
#include "../log_handle.h"
#include "linux/videodev2.h"

#include "server_core.h"
#include "capture_image.h"
#include "mini_shell.h"
#include "isp_handle.h"
#include "register_opt.h"
#ifdef ANDROID_VENCODE
#include "ispSimpleCode.h"
#endif


struct ToolsIniTuning_cfg IniTuning_cfg;
void SetIniTuningEn(struct ToolsIniTuning_cfg cfg) {
	IniTuning_cfg = cfg;
}
struct ToolsIniTuning_cfg GetIniTuningEn(void) {
	return IniTuning_cfg;
}

/*
 * read socket , then check and reply
 * returns sock_rw_check_ret
 */
static int sock_read_check_reply(const char *func_name, int sock_fd, sock_packet *comm_packet,
	sock_command_code sock_cmd, int timeout)
{
	int ret = -1, ret1 = -1;
	ret = sock_read_check_packet(func_name, sock_fd, comm_packet, sock_cmd, timeout);

	if (SOCK_RW_RECV_CLOSE == ret) {
		//LOG("%s#%s: recv close flag\n", __FUNCTION__, func_name);
	} else if (SOCK_RW_ACK_STATUS == ret) {
		//LOG("%s#%s: ACK status\n", __FUNCTION__, func_name);
	} else if (SOCK_RW_CHECK_OK == ret || SOCK_RW_RET_ERROR == ret) {
		// reply
		if (SOCK_RW_CHECK_OK == ret) {
			comm_packet->ret = htonl(SOCK_CMD_RET_OK);
		} else {
			comm_packet->ret = htonl(SOCK_CMD_RET_FAILED);
		}
		ret1 = sock_write(sock_fd, (const void *)comm_packet, sizeof(sock_packet), timeout);
		if (ret1 != sizeof(sock_packet)) {
			if (SHOULD_TRY_AGAIN()) {
				LOG("%s#%s: failed to write, try again\n", __FUNCTION__, func_name);
				ret = SOCK_RW_TRY_AGAIN;
			} else {
				LOG("%s#%s: failed to write(%d, %s)\n", __FUNCTION__, func_name, errno, strerror(errno));
				ret = SOCK_RW_WRITE_ERROR;
			}
		} else {
			//ret = SOCK_RW_CHECK_OK;
		}
	} else {
		LOG("%s#%s: failed to read and check\n", __FUNCTION__, func_name);
	}
	return ret;
}

#define SERVER_TIMEOUT_MAX   10

int g_thread_stop_flag = 0;
#define NOTIFY_ALL_STOPS() \
	{ g_thread_stop_flag = 1; }
#define CHECK_THREAD_STOP() \
	if (g_thread_stop_flag) { \
		LOG("%s: recv stop flag\n", __FUNCTION__); \
		break; \
	}

int g_thread_status = 0x0;
inline int CheckThreadsStatus() {
	return g_thread_status;
}

void *sock_handle_heart_jump_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int timeout_times = 0;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_stop_flag = 0;
	g_thread_status |= TH_STATUS_HEART_JUMP;

#ifdef ANDROID_VENCODE
	/* malloc vencoder param buff */
	vencoder_tuning_param = (vencoder_tuning_param_cfg_t *)malloc(sizeof(vencoder_tuning_param_cfg_t));
	if (vencoder_tuning_param == NULL) {
		LOG("%s:malloc vencoder param buff fail, please check the total free memory\n", __FUNCTION__);
	} else {
		memset(vencoder_tuning_param, 0, sizeof(vencoder_tuning_param_cfg_t));
	}
	vencoder_tuning_param->common_cfg.EncodeFormat = VENC_CODEC_H264;
	vencoder_tuning_param->common_cfg.mBitRate = 10*1024*1024;
	vencoder_tuning_param->common_cfg.MaxKeyInterval = 30;
	vencoder_tuning_param->common_cfg.mMixQp = 10;
	vencoder_tuning_param->common_cfg.mMaxQp = 50;
	vencoder_tuning_param->common_cfg.mInitQp = 30;

	vencoder_tuning_param->proc_cfg.ProcEnable = 1;
	vencoder_tuning_param->proc_cfg.ProcFreq = 30;
	vencoder_tuning_param->proc_cfg.StatisBitRateTime = 1000;

	vencoder_tuning_param->fixqp_cfg.mIQp = 35;
	vencoder_tuning_param->fixqp_cfg.mPQp = 35;

	vencoder_tuning_param->proc_cfg.ProcEnable = 1;
	vencoder_tuning_param->proc_cfg.ProcFreq = 60;
#endif
	while (1) {
		CHECK_THREAD_STOP();
		//msleep(1000);  // check interval
		// ready to receive heart jump
		ret = sock_read_check_reply(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_HEART_JUMP, SOCK_DEFAULT_TIMEOUT);
		//LOG("%s: %d, %d\n", __FUNCTION__, ret, errno);
		if (SHOULD_CLOSE_SOCK(ret)) {
			LOG("%s: should close socket %d(%d)\n", __FUNCTION__, sock_fd, ret);
			break;
		} else {
			if (ETIMEDOUT == errno) {
				errno = 0;
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	g_thread_status &= ~TH_STATUS_HEART_JUMP;
#ifdef ANDROID_VENCODE
	/* free vencoder param buff */
	free(vencoder_tuning_param);
#endif
	//NOTIFY_ALL_STOPS();
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_preview_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int timeout_times = 0, tmp = 0, init_flag = 0;
	capture_format cap_fmt;
	//cap_fmt.buffer = (unsigned char *)malloc(1 << 23); // 16M

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_PREVIEW;
	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_PREVIEW, SOCK_DEFAULT_TIMEOUT/10);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			//g_thread_status |= TH_STATUS_PREVIEW;
			timeout_times = 0;
			cap_fmt.format = ntohl(comm_packet.reserved[0]);
			tmp = ntohl(comm_packet.reserved[1]);
			cap_fmt.width = (tmp >> 16) & 0x0000ffff;
			cap_fmt.height = tmp & 0x0000ffff;
			cap_fmt.channel = ntohl(comm_packet.reserved[2]);
			tmp = ntohl(comm_packet.reserved[3]);
			cap_fmt.fps = (tmp >> 16) & 0x0000ffff;
			cap_fmt.wdr = tmp & 0x0000ffff;
			cap_fmt.length = 0;
			cap_fmt.index =  ntohl(comm_packet.index);
			memset(cap_fmt.width_stride, 0, sizeof(cap_fmt.width_stride));
			switch (cap_fmt.format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV21:
			case V4L2_PIX_FMT_YUV420:
			case V4L2_PIX_FMT_YVU420:
				cap_fmt.planes_count = 1;
				break;
			case V4L2_PIX_FMT_NV12M:
			case V4L2_PIX_FMT_NV21M:
				cap_fmt.planes_count = 2;
				break;
			case V4L2_PIX_FMT_YUV420M:
			case V4L2_PIX_FMT_YVU420M:
				cap_fmt.planes_count = 3;
				break;
			default:
				cap_fmt.planes_count = 1;
				break;
			}
			if (!init_flag) {
				cap_fmt.buffer = (unsigned char *)malloc(((cap_fmt.width/16+1)*16) * ((cap_fmt.height/16+1)*16) * 2);
				if(cap_fmt.buffer == NULL)
					LOG("%s:malloc cap_fmt.buffer fail, please check the total free memory\n", __FUNCTION__);
				init_flag = 1;
			}
			/*LOG("%s: ready to get preview - fmt:%d,%dx%d@%d,wdr:%d,ch:%d,planes:%d\n", __FUNCTION__,
					cap_fmt.format, cap_fmt.width, cap_fmt.height,
					cap_fmt.fps, cap_fmt.wdr,
					cap_fmt.channel, cap_fmt.planes_count);*/
			ret = get_capture_buffer(&cap_fmt);
			if (CAP_ERR_NONE != ret) {
				LOG("%s: failed to get preview - fmt:%d,%dx%d,ch:%d,planes:%d\n", __FUNCTION__,
					cap_fmt.format, cap_fmt.width, cap_fmt.height,
					cap_fmt.channel, cap_fmt.planes_count);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			} else {
				comm_packet.data_length = htonl(cap_fmt.length);
				comm_packet.reserved[0] = htonl(cap_fmt.format);
				comm_packet.reserved[1] = htonl((cap_fmt.width << 16) | (cap_fmt.height & 0x0000ffff));
				comm_packet.reserved[2] = htonl((cap_fmt.width_stride[0] << 16) | (cap_fmt.width_stride[1] & 0x0000ffff));
				comm_packet.reserved[3] = htonl((cap_fmt.width_stride[2] << 16));
				/*LOG("%s: get preview - fmt:%d,%dx%d,length:%d,ch:%d[%d, %d, %d]\n", __FUNCTION__,
					cap_fmt.format, cap_fmt.width, cap_fmt.height, cap_fmt.length, cap_fmt.channel,
					cap_fmt.width_stride[0], cap_fmt.width_stride[1], cap_fmt.width_stride[2]);*/
				ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_PREVIEW, SOCK_DEFAULT_TIMEOUT);
				if (SOCK_RW_CHECK_OK == ret) {
					ret = sock_write(sock_fd, (const void *)cap_fmt.buffer, cap_fmt.length, SOCK_DEFAULT_TIMEOUT);
				}
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				//if (timeout_times == 1) {
				//	g_thread_status &= ~TH_STATUS_PREVIEW;
					//LOG("%s: timeout, run loop_thread\n", __FUNCTION__);
				//}
				if (timeout_times > SERVER_TIMEOUT_MAX*10) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(cap_fmt.buffer);
	cap_fmt.buffer = NULL;
	g_thread_status &= ~TH_STATUS_PREVIEW;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

//#define SAVE_FLOW
//#define TRANSFER_SIZE (1<<22) //4M
void *sock_handle_capture_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int timeout_times = 0, tmp = 0;
	capture_format cap_fmt;
	unsigned int TRANSFER_SIZE;
	//cap_fmt.buffer = (unsigned char *)malloc(1 << 24); // 16M

#ifdef SAVE_FLOW
	int i = 0;
	char save_file_name[256];
	FILE *fp = NULL;
#endif

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_CAPTURE;
	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_CAPTURE, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			cap_fmt.format = ntohl(comm_packet.reserved[0]);
			tmp = ntohl(comm_packet.reserved[1]);
			cap_fmt.width = (tmp >> 16) & 0x0000ffff;
			cap_fmt.height = tmp & 0x0000ffff;
			cap_fmt.channel = ntohl(comm_packet.reserved[2]);
			tmp = ntohl(comm_packet.reserved[3]);
			cap_fmt.fps = (tmp >> 16) & 0x0000ffff;
			cap_fmt.wdr = tmp & 0x0000ffff;
			cap_fmt.length = 0;
			cap_fmt.framecount = ntohl(comm_packet.framecount);
			cap_fmt.index =  ntohl(comm_packet.index);
			memset(cap_fmt.width_stride, 0, sizeof(cap_fmt.width_stride));
			TRANSFER_SIZE = ((cap_fmt.width/16+1)*16) * ((cap_fmt.height/16+1)*16) * 2;
			cap_fmt.buffer = (unsigned char *)malloc(TRANSFER_SIZE * cap_fmt.framecount); // 4M*cap_fmt.framecount
			switch (cap_fmt.format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV21:
			case V4L2_PIX_FMT_YUV420:
			case V4L2_PIX_FMT_YVU420:
				cap_fmt.planes_count = 1;
				#ifdef SAVE_FLOW
				i = sprintf(save_file_name, "/mnt/extsd/yuv_");
				get_sys_time(save_file_name + i, "%04d-%02d-%02d_%02d-%02d-%02d");
				sprintf(save_file_name + i + 19, ".yuv");
				#endif
				break;
			case V4L2_PIX_FMT_NV12M:
			case V4L2_PIX_FMT_NV21M:
				cap_fmt.planes_count = 2;
				#ifdef SAVE_FLOW
				i = sprintf(save_file_name, "/mnt/extsd/yuv_");
				get_sys_time(save_file_name + i, "%04d-%02d-%02d_%02d-%02d-%02d");
				sprintf(save_file_name + i + 19, ".yuv");
				#endif
				break;
			case V4L2_PIX_FMT_YUV420M:
			case V4L2_PIX_FMT_YVU420M:
				cap_fmt.planes_count = 3;
				#ifdef SAVE_FLOW
				i = sprintf(save_file_name, "/mnt/extsd/yuv_");
				get_sys_time(save_file_name + i, "%04d-%02d-%02d_%02d-%02d-%02d");
				sprintf(save_file_name + i + 19, ".yuv");
				#endif
				break;
			default:
				cap_fmt.planes_count = 1;
				#ifdef SAVE_FLOW
				i = sprintf(save_file_name, "/mnt/extsd/raw_");
				get_sys_time(save_file_name + i, "%04d-%02d-%02d_%02d-%02d-%02d");
				sprintf(save_file_name + i + 19, ".raw");
				#endif
				break;
			}
			//LOG("%s: ready to get capture - fmt:%d,%dx%d@%d,wdr:%d,ch:%d,planes:%d\n", __FUNCTION__,
			//		cap_fmt.format, cap_fmt.width, cap_fmt.height,
			//		cap_fmt.fps, cap_fmt.wdr,
			//		cap_fmt.channel, cap_fmt.planes_count);
			ret = get_capture_buffer_transfer(&cap_fmt);
#ifdef SAVE_FLOW
			fp = fopen(save_file_name, "wb");
			if (fp) {
				LOG("%s: ready to write to file: %s\n", __FUNCTION__, save_file_name);
				fwrite(cap_fmt.buffer, cap_fmt.length, 1, fp);
				fclose(fp);
				fp = NULL;
				LOG("%s: all write done\n", __FUNCTION__);
			}
#endif
			if (CAP_ERR_NONE != ret) {
				LOG("%s: failed to get capture - fmt:%d,%dx%d,ch:%d,planes:%d\n", __FUNCTION__,
					cap_fmt.format, cap_fmt.width, cap_fmt.height,
					cap_fmt.channel, cap_fmt.planes_count);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			} else {
				LOG("%s: get capture - fmt:%d,%dx%d,length:%d,ch:%d[%d, %d, %d]\n", __FUNCTION__,
					cap_fmt.format, cap_fmt.width, cap_fmt.height, cap_fmt.length, cap_fmt.channel,
					cap_fmt.width_stride[0], cap_fmt.width_stride[1], cap_fmt.width_stride[2]);

				comm_packet.data_length = htonl(cap_fmt.length);
				comm_packet.reserved[0] = htonl(cap_fmt.format);
				comm_packet.reserved[1] = htonl((cap_fmt.width << 16) | (cap_fmt.height & 0x0000ffff));
				comm_packet.reserved[2] = htonl((cap_fmt.width_stride[0] << 16) | (cap_fmt.width_stride[1] & 0x0000ffff));
				comm_packet.reserved[3] = htonl((cap_fmt.width_stride[2] << 16));
				ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_CAPTURE, SOCK_DEFAULT_TIMEOUT);
				if (SOCK_RW_CHECK_OK == ret) {
					unsigned char *buffer = (unsigned char *)malloc(TRANSFER_SIZE); // 4M
					int length = cap_fmt.length;
					int len = 0;
					LOG("begin transfer......\n");
					while((length > TRANSFER_SIZE) && (length > 0))
					{
						memset(buffer, 0, TRANSFER_SIZE * sizeof(unsigned char));
						memcpy(buffer, &cap_fmt.buffer[len], TRANSFER_SIZE);
						ret = sock_write(sock_fd, (const void *)buffer, TRANSFER_SIZE, SOCK_DEFAULT_TIMEOUT);
						length -= TRANSFER_SIZE;
						len += TRANSFER_SIZE;
						msleep(1000);
					}
					if(length > 0)
					{
						memset(buffer, 0, TRANSFER_SIZE * sizeof(unsigned char));
						memcpy(buffer, &cap_fmt.buffer[len], length);
						ret = sock_write(sock_fd, (const void *)buffer, length, SOCK_DEFAULT_TIMEOUT);
						len += length;
						LOG("++++++++++ len = %d, cap_fmt.length = %d +++++++++++++++\n", len, cap_fmt.length);
					}
					LOG("end transfer......\n");
					free(buffer);
					buffer = NULL;
					//ret = sock_write(sock_fd, (const void *)cap_fmt.buffer, cap_fmt.length, SOCK_DEFAULT_TIMEOUT);
				}
			}
			free(cap_fmt.buffer);
			cap_fmt.buffer = NULL;
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	//free(cap_fmt.buffer);
	//cap_fmt.buffer = NULL;
	g_thread_status &= ~TH_STATUS_CAPTURE;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_blockinfo_thread(void *params)
{
	int ret = 0, i, total_buf;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int timeout_times = 0, tmp = 0, init_flag = 0, type, m_GrayBlocksFlag = 0, block_sel, status, GrayBlocksFlag_current = 0, GrayBlocksFlag_last = 0;
	capture_format cap_fmt;
	SRegion region[20];
	SRegion region_last[20];
	memset(region, 0, sizeof(SRegion) * 20);
	memset(region_last, 0, sizeof(SRegion) * 20);
	//cap_fmt.buffer = (unsigned char *)malloc(1 << 23); // 16M
	cap_fmt.buffer = NULL;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_BLOCK_INFO;
	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_BLOCKINFO, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			type = comm_packet.cmd_ids[1];
			if (type == SOCK_CMD_SET_REGION) {
				tmp = ntohl(comm_packet.reserved[0]);
				block_sel = (tmp >> 16) & 0x0000ffff;
				status = tmp & 0x0000ffff;
				tmp = ntohl(comm_packet.reserved[1]);
				m_GrayBlocksFlag = tmp & 0x000fffff;
				tmp = ntohl(comm_packet.reserved[2]);
				region[block_sel].left = (tmp >> 16) & 0x0000ffff;
				region[block_sel].top = tmp & 0x0000ffff;
				tmp = ntohl(comm_packet.reserved[3]);
				region[block_sel].right = (tmp >> 16) & 0x0000ffff;
				region[block_sel].bottom = tmp & 0x0000ffff;
				if (status)
					GrayBlocksFlag_current |= (1 << block_sel);
				else
					GrayBlocksFlag_current &= ~(1 << block_sel);

				comm_packet.ret = htonl(SOCK_CMD_RET_OK);
				//comm_packet.reserved[0] = htonl(GrayBlocksFlag_current);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				LOG("%s:Set Block[%d] Region done.status %d.m_GrayBlocksFlag = 0x%08x, cur = 0x%08x(Region:%d, %d, %d, %d)\n", __FUNCTION__,
					block_sel, status, m_GrayBlocksFlag, GrayBlocksFlag_current, region[block_sel].left, region[block_sel].top,
					region[block_sel].right, region[block_sel].bottom);
			} else if (type == SOCK_CMD_GET_BLOCKINFO) {
				cap_fmt.format = ntohl(comm_packet.reserved[0]);
				tmp = ntohl(comm_packet.reserved[1]);
				cap_fmt.width = (tmp >> 16) & 0x0000ffff;
				cap_fmt.height = tmp & 0x0000ffff;
				cap_fmt.channel = ntohl(comm_packet.reserved[2]);
				tmp = ntohl(comm_packet.reserved[3]);
				cap_fmt.fps = (tmp >> 16) & 0x0000ffff;
				cap_fmt.wdr = tmp & 0x0000ffff;
				cap_fmt.length = 0;
				cap_fmt.index =  ntohl(comm_packet.index);
				memset(cap_fmt.width_stride, 0, sizeof(cap_fmt.width_stride));
				switch (cap_fmt.format) {
				case V4L2_PIX_FMT_NV12:
				case V4L2_PIX_FMT_NV21:
				case V4L2_PIX_FMT_YUV420:
				case V4L2_PIX_FMT_YVU420:
					cap_fmt.planes_count = 1;
					break;
				case V4L2_PIX_FMT_NV12M:
				case V4L2_PIX_FMT_NV21M:
					cap_fmt.planes_count = 2;
					break;
				case V4L2_PIX_FMT_YUV420M:
				case V4L2_PIX_FMT_YVU420M:
					cap_fmt.planes_count = 3;
					break;
				default:
					cap_fmt.planes_count = 1;
					break;
				}
				//check tunningapp and pctools block num
				if (GrayBlocksFlag_current != m_GrayBlocksFlag) {
					LOG("%s: block num error - (0x%08x, 0x%08x)\n", __FUNCTION__, m_GrayBlocksFlag, GrayBlocksFlag_current);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					comm_packet.reserved[0] = htonl(GrayBlocksFlag_current);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
					continue;
				}

				if (cap_fmt.buffer != NULL && (memcmp(region, region_last, 20 * sizeof(SRegion)) || GrayBlocksFlag_last != GrayBlocksFlag_current)) {
					LOG("%s: ready to re_malloc cap_fmt.buffer\n", __FUNCTION__);
					free(cap_fmt.buffer);
					init_flag = 0;
				}

				if (!init_flag) {
					total_buf = 0;
					for (i = 0; i < 20; i++) {
						if (m_GrayBlocksFlag & (1 << i)) {
							total_buf += (region[i].right - region[i].left + 1) * (region[i].bottom - region[i].top + 1);
						}
					}
					cap_fmt.buffer = (unsigned char *)malloc(total_buf);
					if(cap_fmt.buffer == NULL)
						LOG("%s:malloc cap_fmt.buffer fail, please check the total free memory\n", __FUNCTION__);

					memcpy(region_last, region, 20 * sizeof(SRegion));
					GrayBlocksFlag_last = GrayBlocksFlag_current;
					init_flag = 1;
				}
				/*LOG("%s: ready to get preview - fmt:%d,%dx%d@%d,wdr:%d,ch:%d,planes:%d\n", __FUNCTION__,
						cap_fmt.format, cap_fmt.width, cap_fmt.height,
						cap_fmt.fps, cap_fmt.wdr,
						cap_fmt.channel, cap_fmt.planes_count);*/
				ret = get_capture_blockinfo(&cap_fmt, GrayBlocksFlag_current, region);
				if (CAP_ERR_NONE != ret) {
					LOG("%s: failed to get blockinfo - fmt:%d,%dx%d,ch:%d,planes:%d\n", __FUNCTION__,
						cap_fmt.format, cap_fmt.width, cap_fmt.height,
						cap_fmt.channel, cap_fmt.planes_count);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					comm_packet.data_length = htonl(cap_fmt.length);
					comm_packet.reserved[0] = htonl(cap_fmt.format);
					comm_packet.reserved[1] = htonl((cap_fmt.width << 16) | (cap_fmt.height & 0x0000ffff));
					comm_packet.reserved[2] = htonl((cap_fmt.width_stride[0] << 16) | (cap_fmt.width_stride[1] & 0x0000ffff));
					comm_packet.reserved[3] = htonl((cap_fmt.width_stride[2] << 16));
					ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_BLOCKINFO, SOCK_DEFAULT_TIMEOUT);
					if (SOCK_RW_CHECK_OK == ret) {
						ret = sock_write(sock_fd, (const void *)cap_fmt.buffer, cap_fmt.length, SOCK_DEFAULT_TIMEOUT);
					}
				}
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(cap_fmt.buffer);
	cap_fmt.buffer = NULL;
	g_thread_status &= ~TH_STATUS_BLOCK_INFO;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_tuning_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int isp_sel = 0, cfg_length = 0, type = 0;
	HW_U8 group_id = 0;
	HW_U32 cfg_ids = 0;
	unsigned char *buffer = (unsigned char *)malloc(sizeof(struct isp_params_cfg)); // enough memory
	int buf_length = 0;
	int timeout_times = 0;
	struct ToolsIniTuning_cfg ini_cfg = GetIniTuningEn();
	if (ini_cfg.enable) {
		ini_cfg.params = (struct isp_param_config *)malloc(sizeof(struct isp_param_config));
		SetIniTuningEn(ini_cfg);
	}

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_TUNING;

	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_TUNING, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			type = comm_packet.cmd_ids[1];
			group_id = comm_packet.cmd_ids[2];
			cfg_ids = ((comm_packet.cmd_ids[3] << 24) | (comm_packet.cmd_ids[4] << 16)
						| (comm_packet.cmd_ids[5] << 8) | comm_packet.cmd_ids[6]);
			cfg_length = ntohl(comm_packet.data_length);
			isp_sel = ntohl(comm_packet.reserved[0]);
			if (SOCK_CMD_GET_CFG == type) {
				LOG("%s: get cfg(isp %d) - group:%02x, cfgs:%08x, length:%d\n", __FUNCTION__, isp_sel, group_id, cfg_ids, cfg_length);
				if (!ini_cfg.enable) {
					ret = select_isp(isp_sel, 0);
					if (ret < 0) {
						LOG("%s: failed to select isp - %d\n", __FUNCTION__, isp_sel);
						comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
						ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
						continue;
					}
					ret = isp_get_cfg(isp_sel, group_id, cfg_ids, (void *)buffer);
				} else {
					ret = ini_tuning_get_cfg(group_id, cfg_ids, (void *)buffer);
				}
				if (ret > 0) { // get config ok
					LOG("function = %s , ret = %d \n", __FUNCTION__, ret);
					buf_length = ret;
					comm_packet.data_length = htonl(buf_length);
					convert_tuning_cfg_to_network(group_id, cfg_ids, buffer);
					ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_TUNING, SOCK_DEFAULT_TIMEOUT);
					if (SOCK_RW_CHECK_OK == ret) {
						ret = sock_write(sock_fd, (const void *)buffer, buf_length, SOCK_DEFAULT_TIMEOUT);
					}
				} else {
					LOG("%s: failed to get config(isp %d)\n", __FUNCTION__, isp_sel);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else if (SOCK_CMD_SET_CFG == type) {
				LOG("%s: set cfg(isp %d) - group:%02x, cfgs:%08x, length:%d\n", __FUNCTION__, isp_sel, group_id, cfg_ids, cfg_length);
				if (!ini_cfg.enable) {
					ret = select_isp(isp_sel, 0);
					if (ret < 0) {
						LOG("%s: failed to select isp - %d\n", __FUNCTION__, isp_sel);
						comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
						ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
						continue;
					}
				}
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				if (sizeof(comm_packet) == ret) {
					// read config
					ret = sock_read(sock_fd, (void *)buffer, cfg_length, SOCK_DEFAULT_TIMEOUT);
					if (ret == cfg_length) {
						convert_tuning_cfg_to_local(group_id, cfg_ids, buffer);
						//printf("%s: ready to set isp cfg\n", __FUNCTION__);
						if (!ini_cfg.enable) {
							ret = isp_set_cfg(isp_sel, group_id, cfg_ids, (void *)buffer);
						} else {
							ret = ini_tuning_set_cfg(group_id, cfg_ids, (void *)buffer);
						}
						//printf("%s: set isp cfg done\n", __FUNCTION__);
						if (ret > 0) {  // set config ok
							comm_packet.ret = htonl(SOCK_CMD_RET_OK);
						} else {
							comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
							LOG("%s: failed to set config(isp %d)\n", __FUNCTION__, isp_sel);
						}
						ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
					} else {
						LOG("%s: failed to read configs from client(isp %d, %d)\n", __FUNCTION__, isp_sel, ret);
					}
				} else {
					LOG("%s: failed to write set config reply(isp %d, %d)\n", __FUNCTION__, isp_sel, ret);
				}
			} else if (SOCK_CMD_UPDATE_CFG == type) {
				LOG("%s: update isp - %d\n", __FUNCTION__, isp_sel);
				if (!ini_cfg.enable) {
					ret = select_isp(isp_sel, 0);
					if (ret < 0) {
						LOG("%s: failed to select isp - %d\n", __FUNCTION__, isp_sel);
						comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
						ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
						continue;
					}
					ret = isp_update(isp_sel);
				} else {
					ret = ini_tuning_update_cfg();
				}
				if (ret < 0) {
					LOG("%s: failed to update isp - %d\n", __FUNCTION__, isp_sel);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				} else {
					comm_packet.ret = htonl(SOCK_CMD_RET_OK);
				}
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			} else if (SOCK_CMD_ISP_SEL == type) {
				LOG("%s: set isp - %d\n", __FUNCTION__, isp_sel);
				ret = select_isp(isp_sel, 0);
				if (ret < 0) {
					LOG("%s: failed to select isp - %d\n", __FUNCTION__, isp_sel);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				} else {
					comm_packet.ret = htonl(SOCK_CMD_RET_OK);
				}
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			} else {
				LOG("%s: unknown cmd - %d, %d, %d, %d\n", __FUNCTION__, type, group_id, cfg_ids, cfg_length);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(buffer);
	buffer = NULL;
	if (ini_cfg.enable) {
		free(ini_cfg.params);
		ini_cfg.params = NULL;
		SetIniTuningEn(ini_cfg);
	}
	g_thread_status &= ~TH_STATUS_TUNING;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_log_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int isp_sel = 0;
	unsigned char *buffer = (unsigned char *)malloc(sizeof(struct isp_log_cfg));
	int timeout_times = 0;
	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_LOG;

	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_GET_LOG, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			isp_sel = ntohl(comm_packet.reserved[0]);

			ret = get_log_params(isp_sel, buffer);
			if (ret >= 0) {
				comm_packet.data_length = htonl(sizeof(struct isp_log_cfg));
				ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_GET_LOG, SOCK_DEFAULT_TIMEOUT);
				if (SOCK_RW_CHECK_OK == ret) {
					ret = sock_write(sock_fd, (const void *)buffer, sizeof(struct isp_log_cfg), SOCK_DEFAULT_TIMEOUT);
				}
			} else {
				LOG("%s: failed to get log(isp_sel = %d)\n", __FUNCTION__, isp_sel);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}

		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(buffer);
	buffer = NULL;
	g_thread_status &= ~TH_STATUS_LOG;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_statistics_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int isp_sel = 0, type = 0, stats_length = 0;
	void *stats_info = NULL;
	struct isp_stats_context *stats_context = (struct isp_stats_context *)malloc(sizeof(struct isp_stats_context));
	int timeout_times = 0;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_STATISTICS;

	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_GET_STAT, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			type = comm_packet.cmd_ids[1];
			isp_sel = ntohl(comm_packet.reserved[0]);

			if (SOCK_CMD_STAT_AE == type || SOCK_CMD_STAT_AWB == type || SOCK_CMD_STAT_AF == type || SOCK_CMD_STAT_TDNF == type) {
				ret = select_isp(isp_sel, 0);
				ret = isp_stats_req(isp_sel, stats_context);
				if (ret >= 0) {   // get OK
					if (SOCK_CMD_STAT_AE == type) {
						stats_info = &stats_context->stats.ae_stats;
						stats_length = sizeof(struct isp_ae_stats_s);
					} else if (SOCK_CMD_STAT_AWB == type) {
						stats_info = &stats_context->stats.awb_stats;
						stats_length = sizeof(struct isp_awb_stats_s);
					} else if (SOCK_CMD_STAT_AF == type) {
						stats_info = &stats_context->stats.af_stats;
						stats_length = sizeof(struct isp_af_stats_s);
					} else {
						stats_info = &stats_context->stats.d3d_k_stats;
						stats_length = sizeof(struct isp_d3d_k_stats_s);
					}
					//output_3a_info(stats_info, type);
					hton_3a_info(stats_info, type);

					comm_packet.data_length = htonl(stats_length);
					ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_GET_STAT, SOCK_DEFAULT_TIMEOUT);
					if (SOCK_RW_CHECK_OK == ret) {
						ret = sock_write(sock_fd, stats_info, stats_length, SOCK_DEFAULT_TIMEOUT);
					}
				} else {  // failed
					LOG("%s: failed to get statistics(%d, %d)\n", __FUNCTION__, isp_sel, type);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else {
				LOG("%s: unknown type(%d, %d)\n", __FUNCTION__, isp_sel, type);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(stats_context);
	stats_context = NULL;
	g_thread_status &= ~TH_STATUS_STATISTICS;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

/*
 * shell scripts format
 * delete format chars, some like "[1;32m", "[0m"...
 * returns new length of buffer, not includes '\0'
 */
int shell_format(char *buffer)
{
	int length = 0;
	char tmp_buf[1024];
	char *dst_ptr = tmp_buf;
	char *src_ptr = buffer;
	while (*src_ptr != 0) {
		if (*src_ptr == 27 && *(src_ptr+1) == '[') {  // '['
			while (*src_ptr++ != 'm');
		} else {
			*dst_ptr++ = *src_ptr++;
			length++;
		}
	}
	*dst_ptr = '\0';
	memcpy(buffer, tmp_buf, length + 1);
	return length;
}

void *sock_handle_script_thread(void *params)
{
	int ret = -1;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	char *buffer = (char *)malloc(1 << 10);  // 1K
	int recv_length = 0;
	int timeout_times = 0;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_SCRIPT;
	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_reply(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_SCRIPT, SOCK_DEFAULT_TIMEOUT);
 		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			recv_length = ntohl(comm_packet.data_length);
			if (recv_length > 0) {
				ret = sock_read(sock_fd, (void *)buffer, recv_length, SOCK_DEFAULT_TIMEOUT);
				if (ret != recv_length) {
					LOG("%s: failed to read scirpt(%d)\n", __FUNCTION__, ret);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					buffer[recv_length] = '\0';
					LOG("%s: recv script - %s\n", __FUNCTION__, buffer);
#if 1 // super command to quit app
					if (!strncmp(buffer, "ExitHawkview", 8)) {
						sprintf(buffer, "Server is ready to exit...");
						ret = 0;
						g_thread_status |= 0x01000000;
						NOTIFY_ALL_STOPS();
						LOG("============ ********** recv SUPER EXIT command! ********** ======================\n");
					} else {
						ret = mini_shell_exec(buffer, buffer);
					}
#else
					ret = mini_shell_exec(buffer, buffer);
#endif
					buffer[1023] = '\0';
					if (0 == ret) {
						LOG("%s: exec ret - %s\n", __FUNCTION__, buffer);
						recv_length = shell_format(buffer);
						comm_packet.ret = htonl(SOCK_CMD_RET_OK);
						comm_packet.data_length = htonl(recv_length);
						ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_SCRIPT, SOCK_DEFAULT_TIMEOUT);
						if (SOCK_RW_CHECK_OK == ret) {
							ret = sock_write(sock_fd, (const void *)buffer, recv_length, SOCK_DEFAULT_TIMEOUT);
						}
					} else {
						LOG("%s: failed to execute scirpt\n", __FUNCTION__);
						comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
						ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
					}
				}
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(buffer);
	buffer = NULL;
	g_thread_status &= ~TH_STATUS_SCRIPT;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_register_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	char sensor_name[128];
	int type = 0, sensor_name_length = 0;
	int addr_width = 0, data_width = 0;
	int reg_addr = 0, reg_data = 0;
	int timeout_times = 0;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_REGISTER;

	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_REGOPT, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			type = comm_packet.cmd_ids[1];
			addr_width = ntohl(comm_packet.reserved[0]);
			data_width = ntohl(comm_packet.reserved[1]);
			reg_addr = ntohl(comm_packet.reserved[2]);
			reg_data = ntohl(comm_packet.reserved[3]);
			sensor_name_length = ntohl(comm_packet.data_length);
			if (SOCK_CMD_REG_READ == type) {
				LOG("%s: read 0x%04x\n", __FUNCTION__, reg_addr);
				reg_data = read_reg(reg_addr, data_width);
				if (reg_data >= 0) { // read ok
					comm_packet.reserved[3] = htonl(reg_data);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					LOG("%s: failed to read 0x%04x(%d)\n", __FUNCTION__, reg_addr, reg_data);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else if (SOCK_CMD_REG_WRITE == type) {
				LOG("%s: write 0x%04x, 0x%04x\n", __FUNCTION__, reg_addr, reg_data);
				ret = write_reg(reg_addr, reg_data);
				if (!ret) { // write ok
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					LOG("%s: failed to write 0x%04x, 0x%04x\n", __FUNCTION__, reg_addr, reg_data);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else if (SOCK_CMD_REG_SETCFG == type) {
				LOG("%s: set reg(%d, %d)\n", __FUNCTION__, addr_width, data_width);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				if (sizeof(comm_packet) == ret) {
					// read sensor name
					ret = sock_read(sock_fd, (void *)sensor_name, sensor_name_length, SOCK_DEFAULT_TIMEOUT);
					if (ret == sensor_name_length) {
						sensor_name[sensor_name_length] = '\0';
						LOG("%s: sensor name - %s\n", __FUNCTION__, sensor_name);
						ret = set_reg_opt(sensor_name, addr_width, data_width);
						if (!ret) {  // set config ok
							comm_packet.ret = htonl(SOCK_CMD_RET_OK);
						} else {
							comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
							LOG("%s: failed to set config\n", __FUNCTION__);
						}
						ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
					} else {
						LOG("%s: failed to read configs from client(%d)\n", __FUNCTION__, ret);
					}
				} else {
					LOG("%s: failed to write set config reply(%d)\n", __FUNCTION__, ret);
				}
			} else {
				LOG("%s: unknown cmd - %d\n", __FUNCTION__, type);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	g_thread_status &= ~TH_STATUS_REGISTER;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_aelv_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int lv = 0, isp_sel = 0;
	int timeout_times = 0;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_AELV;

	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_AE_LV, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			isp_sel = ntohl(comm_packet.reserved[0]);
			lv = isp_get_lv(isp_sel);
			LOG("%s: get lv %d\n", __FUNCTION__, lv);
			if (lv > 0) { // get lv ok
				comm_packet.reserved[0] = htonl(lv);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			} else {
				LOG("%s: failed to get lv\n", __FUNCTION__);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	g_thread_status &= ~TH_STATUS_AELV;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_set_input_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	sensor_input sensor_in;
	int timeout_times = 0, tmp = 0;
	struct ToolsIniTuning_cfg ini_cfg = GetIniTuningEn();

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_SET_INPUT;

	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_SET_INPUT, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			tmp = ntohl(comm_packet.reserved[0]);
			sensor_in.isp = (tmp >> 16) & 0x0000ffff;
			sensor_in.channel = tmp & 0x0000ffff;
			tmp = ntohl(comm_packet.reserved[1]);
			sensor_in.width = (tmp >> 16) & 0x0000ffff;
			sensor_in.height = tmp & 0x0000ffff;
			tmp = ntohl(comm_packet.reserved[2]);
			sensor_in.fps = (tmp >> 16) & 0x0000ffff;
			sensor_in.wdr = tmp & 0x0000ffff;
			sensor_in.index =  ntohl(comm_packet.index);
			sensor_in.format = ntohl(comm_packet.reserved[3]);
			LOG("%s: isp %d, vich %d, %dx%d@%d, wdr %d.\n", __FUNCTION__,
				sensor_in.isp, sensor_in.channel,
				sensor_in.width, sensor_in.height,
				sensor_in.fps, sensor_in.wdr);
			ini_cfg.sensor_cfg = sensor_in;
			SetIniTuningEn(ini_cfg);
			if (!ini_cfg.enable) {
				ret = set_sensor_input(&sensor_in);
				if (CAP_ERR_NONE == ret) { // set ok
					msleep(1000);
					ret = select_isp(sensor_in.isp, 1);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					LOG("%s: failed to set input(isp %d, ch %d)\n", __FUNCTION__,
						sensor_in.isp, sensor_in.channel);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else {
				char path[100];
				sprintf(path, "%s%d_%d_%d_%d", ini_cfg.base_path, ini_cfg.sensor_cfg.width, ini_cfg.sensor_cfg.height,
							ini_cfg.sensor_cfg.fps, ini_cfg.sensor_cfg.wdr);
				if (access(path, F_OK) == 0) {
					msleep(1000);
				} else {
					LOG("%s: failed to set input, PATH:%s is not existed!!!\n", __FUNCTION__, path);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				}
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
		break;   // only do one time
	}
	close(sock_fd);
	g_thread_status &= ~TH_STATUS_SET_INPUT;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}


void *sock_handle_raw_flow_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	capture_format cap_fmt;
	cap_fmt.buffer = (unsigned char *)malloc(1 << 24); // 16M
	//char save_file_name[256];
	int timeout_times = 0;
	int type = 0, tmp = 0;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_RAW_FLOW;

	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_RAW_FLOW, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			type = comm_packet.cmd_ids[1];
			cap_fmt.format = ntohl(comm_packet.reserved[0]);
			tmp = ntohl(comm_packet.reserved[1]);
			cap_fmt.width = (tmp >> 16) & 0x0000ffff;
			cap_fmt.height = tmp & 0x0000ffff;
			cap_fmt.channel = ntohl(comm_packet.reserved[2]);
			cap_fmt.length = 0;
			cap_fmt.index =  ntohl(comm_packet.index);
			memset(cap_fmt.width_stride, 0, sizeof(cap_fmt.width_stride));
			if (SOCK_CMD_RAW_FLOW_START == type) {
				ret = start_raw_flow(&cap_fmt);
				if (CAP_ERR_NONE == ret) { // start ok
					//tmp = sprintf(save_file_name, "/mnt/extsd/raw_");
					//get_sys_time(save_file_name + tmp, "%04d-%02d-%02d_%02d-%02d-%02d");
					//save_raw_flow(save_file_name);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					LOG("%s: failed to start raw flow\n", __FUNCTION__);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else if (SOCK_CMD_RAW_FLOW_STOP == type) {
				ret = stop_raw_flow(cap_fmt.channel);
				if (CAP_ERR_NONE == ret) { // stop ok
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					LOG("%s: failed to stop raw flow\n", __FUNCTION__);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else if (SOCK_CMD_RAW_FLOW_GET == type) {
				ret = get_raw_flow_frame(&cap_fmt);
				if (CAP_ERR_NONE == ret) {
					comm_packet.data_length = htonl(cap_fmt.length);
					comm_packet.reserved[0] = htonl(cap_fmt.format);
					comm_packet.reserved[1] = htonl((cap_fmt.width << 16) | (cap_fmt.height & 0x0000ffff));
					comm_packet.reserved[2] = htonl((cap_fmt.width_stride[0] << 16) | (cap_fmt.width_stride[1] & 0x0000ffff));
					comm_packet.reserved[3] = htonl((cap_fmt.width_stride[2] << 16));
					LOG("%s: get raw flow - fmt:%d,%dx%d,length:%d,ch:%d[%d, %d, %d]\n", __FUNCTION__,
						cap_fmt.format, cap_fmt.width, cap_fmt.height, cap_fmt.length, cap_fmt.channel,
						cap_fmt.width_stride[0], cap_fmt.width_stride[1], cap_fmt.width_stride[2]);
					ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_RAW_FLOW, SOCK_DEFAULT_TIMEOUT);
					if (SOCK_RW_CHECK_OK == ret) {
						ret = sock_write(sock_fd, (const void *)cap_fmt.buffer, cap_fmt.length, SOCK_DEFAULT_TIMEOUT);
					}
				} else {
					LOG("%s: failed to get raw flow\n", __FUNCTION__);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				}
			} else {
				LOG("%s: unknown cmd - %d\n", __FUNCTION__, type);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(cap_fmt.buffer);
	cap_fmt.buffer = 0;
	g_thread_status &= ~TH_STATUS_RAW_FLOW;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}

void *sock_handle_isp_version_thread(void *params)
{
	int ret = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int timeout_times = 0;
	char *buffer = (char *)malloc(sizeof(char)*500); // enough memory
	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);

	while(1)
	{
		CHECK_THREAD_STOP();
		ret = sock_read_check_reply(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_ISP_VERSION, SOCK_DEFAULT_TIMEOUT);
		LOG("%s: %d, %d\n", __FUNCTION__, ret, errno);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			get_isp_version(buffer);
			LOG("isp version = %s, function = %s\n", buffer, __FUNCTION__);
			ret = sock_write(sock_fd, (const void *)buffer, strlen(buffer), SOCK_DEFAULT_TIMEOUT);
			if(ret != strlen(buffer))
			{
				LOG("%s: write isp version failed!, ret:%d\n", __FUNCTION__, ret);
				break;
			}
		} else{
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	close(sock_fd);
	free(buffer);
	buffer = NULL;
	g_thread_status &= ~TH_STATUS_TUNING;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return NULL;
}

#ifdef ANDROID_VENCODE
static int encoderEventHandler(
            VideoEncoder* pEncoder,
            void* pAppData,
            VencEventType eEvent,
            unsigned int nData1,
            unsigned int nData2,
            void* pEventData)
{
	int isp_sel;
    if(eEvent == VencEvent_UpdateVeToIspParam)
    {
        #if 0
        VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)pEventData;
        LOG("is_overflow = %d, moveLevel = %d\n",
            pVe2IspParam->mMovingLevelInfo.is_overflow,
            pVe2IspParam->mMovingLevelInfo.moving_level_table[ISP_TABLE_LEN*ISP_TABLE_LEN - 1])
        #endif
        //* TODO
		isp_sel = *(int *)pAppData;
		if (pEventData)
			isp_set_attr_cfg(isp_sel, ISP_CTRL_VENC2ISP_PARAM, pEventData);
    }
    return 0;
}

static EncoderCbType encoderCallback;

void *sock_handle_preview_vencode_thread(void *params)
{
	int ret = 0, frame_count = 0;
	sock_packet comm_packet;
	int sock_fd = (int)params;
	int timeout_times = 0, tmp = 0, encode_init = 0, buf_init = 0, type = 0, isp_sel = 0;
	int encpp_en, color_space;
	struct sEncppSharpParam encpp_sharp_param;
	struct s3DfilterParam encoder_3d_param;
	struct s2DfilterParam encoder_2d_param;
	capture_format cap_fmt;
	encode_param_t encode_param;
    unsigned char head_num;
	int cur_enc_mode = 0; /* 0:default  ;  1:online flow  ;  2:get yuv*/
	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);
	g_thread_status |= TH_STATUS_PREVIEW_VENCODE;
#if 0
	cap_fmt.buffer = (unsigned char *)malloc(1 << 24); // 16M
	if(cap_fmt.buffer == NULL)
	{
		LOG("malloc cap_fmt.buffer fail, please check the total free memory\n");
		return 0;
	}
#endif
	while (1) {
		CHECK_THREAD_STOP();
		ret = sock_read_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_VENCODE, SOCK_DEFAULT_TIMEOUT);
		if (SHOULD_CLOSE_SOCK(ret)) {
			break;
		} else if (SOCK_RW_CHECK_OK == ret) {
			timeout_times = 0;
			type = comm_packet.cmd_ids[1];
			cap_fmt.format = ntohl(comm_packet.reserved[0]);
			tmp = ntohl(comm_packet.reserved[1]);
			cap_fmt.width = (tmp >> 16) & 0x0000ffff;
			cap_fmt.height = tmp & 0x0000ffff;
			tmp = ntohl(comm_packet.reserved[2]);
			isp_sel = (tmp >> 16) & 0x0000ffff;
			cap_fmt.channel = tmp & 0x0000ffff;
			tmp = ntohl(comm_packet.reserved[3]);
			cap_fmt.fps = (tmp >> 16) & 0x0000ffff;
			cap_fmt.wdr = tmp & 0x0000ffff;
			cap_fmt.length = 0;
			cap_fmt.index =  ntohl(comm_packet.index);
			memset(cap_fmt.width_stride, 0, sizeof(cap_fmt.width_stride));
			switch (cap_fmt.format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV21:
			case V4L2_PIX_FMT_YUV420:
			case V4L2_PIX_FMT_YVU420:
				cap_fmt.planes_count = 1;
				break;
			case V4L2_PIX_FMT_NV12M:
			case V4L2_PIX_FMT_NV21M:
				cap_fmt.planes_count = 2;
				break;
			case V4L2_PIX_FMT_YUV420M:
			case V4L2_PIX_FMT_YVU420M:
				cap_fmt.planes_count = 3;
				break;
			default:
				cap_fmt.planes_count = 1;
				break;
			}

			if (!buf_init) {
				cap_fmt.buffer = (unsigned char *)malloc(((cap_fmt.width/16+1)*16) * ((cap_fmt.height/16+1)*16) * 3 / 2);
				if(cap_fmt.buffer == NULL) {
					LOG("%s:malloc cap_fmt.buffer fail, please check the total free memory\n", __FUNCTION__);
					g_thread_status &= ~TH_STATUS_PREVIEW_VENCODE;
					return 0;
				}
				buf_init = 1;
			}

			if (cur_enc_mode == 0) {
				if (type == SOCK_CMD_VENCODE_PPSSPS || type == SOCK_CMD_VENCODE_STREAM)
					cur_enc_mode = 1;
				else if (type == SOCK_CMD_VENCODE_ENCPP_YUV)
					cur_enc_mode = 2;
			} else if (cur_enc_mode == 1) {
				if (type == SOCK_CMD_VENCODE_PPSSPS && encode_init) {
					EncoderClose(&encode_param);
					encode_init = 0;
				}
				if (type == SOCK_CMD_VENCODE_ENCPP_YUV && encode_init) {
					EncoderClose(&encode_param);
					encode_init = 0;
					cur_enc_mode = 2;
				}
			} else if (cur_enc_mode == 2) {
				if ((type == SOCK_CMD_VENCODE_PPSSPS || type == SOCK_CMD_VENCODE_STREAM) && encode_init) {
					EncoderClose(&encode_param);
					cur_enc_mode = 1;
					encode_init = 0;
				}
			}

			if (cur_enc_mode == 1)
				g_thread_status |= TH_STATUS_PREVIEW_VENCODE;
			else if (cur_enc_mode == 2)
				g_thread_status &= ~TH_STATUS_PREVIEW_VENCODE;

			//just need to init vencoder by 1 time
			if (encode_init == 0) {
				ret = set_vencode_config(&cap_fmt, &encode_param, type);
				isp_get_attr_cfg(isp_sel, ISP_CTRL_COLOR_SPACE, &color_space);
				if (color_space == 0) {
					encode_param.eColorSpace = VENC_BT601;
					encode_param.bColorSpaceFullFlag = 1;
				} else if (color_space == 1) {
					encode_param.eColorSpace = VENC_BT709;
					encode_param.bColorSpaceFullFlag = 1;
				} else if (color_space == 2) {
					encode_param.eColorSpace = VENC_BT2020;
					encode_param.bColorSpaceFullFlag = 1;
				} else if (color_space == 3) {
					encode_param.eColorSpace = VENC_BT601;
					encode_param.bColorSpaceFullFlag = 0;
				} else if (color_space == 4) {
					encode_param.eColorSpace = VENC_BT709;
					encode_param.bColorSpaceFullFlag = 0;
				} else if (color_space == 5) {
					encode_param.eColorSpace = VENC_BT2020;
					encode_param.bColorSpaceFullFlag = 0;
				} else {
					LOG("%s: encode_param.eColorSpace fail. %d\n", __FUNCTION__, encode_param.eColorSpace);
				}

				if (type == SOCK_CMD_VENCODE_PPSSPS || type == SOCK_CMD_VENCODE_STREAM)
					encode_param.bEnableGetWbYuv = 0;
				else if (type == SOCK_CMD_VENCODE_ENCPP_YUV)
					encode_param.bEnableGetWbYuv = 1;

				if (ret) {
					LOG("%s: update vencode config fail\n", __FUNCTION__);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
					continue;
				}
				ret = EncoderOpen(encode_param.encode_format);
				if (ret) {
					LOG("%s: detect openEnc error, exit progress@%d\n", __FUNCTION__, __LINE__);
					EncoderClose(&encode_param);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
					continue;
				} else {

                    encoderCallback.EventHandler = encoderEventHandler;
                    EncoderSetCallbacks(&encode_param, &encoderCallback, &isp_sel);

					ret = EncoderPrepare(&encode_param);
					if (ret) {
						LOG("%s: detect prepareEnc error, exit progress@%d\n", __FUNCTION__, __LINE__);
						EncoderClose(&encode_param);
						comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
						sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
						continue;
					} else {
						//vencoder init successfully
						LOG("init video encoder successfully\n");
						encode_init = 1;
					}
				}
			}

			if (encode_init == 1 && (type == SOCK_CMD_VENCODE_PPSSPS || type == SOCK_CMD_VENCODE_STREAM || type == SOCK_CMD_VENCODE_ENCPP_YUV)) {
				isp_get_encpp_cfg(isp_sel, ISP_CTRL_ENCPP_EN, &encpp_en);
				isp_get_encpp_cfg(isp_sel, ISP_CTRL_ENCODER_3DNR_CFG, &encoder_3d_param);
				isp_get_encpp_cfg(isp_sel, ISP_CTRL_ENCODER_2DNR_CFG, &encoder_2d_param);
				EncoderSetParamEnableSharp(&encode_param, encpp_en);
				EncoderSetParam3DFliter(&encode_param, &encoder_3d_param);
				EncoderSetParam2DFliter(&encode_param, &encoder_2d_param);
				if (encpp_en) {
					isp_get_encpp_cfg(isp_sel, ISP_CTRL_ENCPP_STATIC_CFG, &encpp_sharp_param.mStaticParam);
					isp_get_encpp_cfg(isp_sel, ISP_CTRL_ENCPP_DYNAMIC_CFG, &encpp_sharp_param.mDynamicParam);
					EncoderSetParamSharpConfig(&encode_param, &encpp_sharp_param);
				}

				ret = get_capture_vencode_buffer(&cap_fmt, &encode_param, type);
				if (CAP_ERR_NONE != ret) {
					LOG("%s: ret=%d failed to get preview vencode - fmt:%d,%dx%d,ch:%d,planes:%d\n", __FUNCTION__, ret,
						cap_fmt.format, cap_fmt.width, cap_fmt.height,
						cap_fmt.channel, cap_fmt.planes_count);
					comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
					ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				} else {
					if(++frame_count % 50 == 0) {
						LOG("%s: %d frames of ISP image have been encoded\n", __FUNCTION__, frame_count);
					}
					comm_packet.data_length = htonl(cap_fmt.length);
					comm_packet.reserved[0] = htonl(cap_fmt.format);
					comm_packet.reserved[1] = htonl((cap_fmt.width << 16) | (cap_fmt.height & 0x0000ffff));
					comm_packet.reserved[2] = htonl((cap_fmt.width_stride[0] << 16) | (cap_fmt.width_stride[1] & 0x0000ffff));
					comm_packet.reserved[3] = htonl((cap_fmt.width_stride[2] << 16));
					/*LOG("%s: get preview - fmt:%d,%dx%d,length:%d,ch:%d[%d, %d, %d]\n", __FUNCTION__,
						cap_fmt.format, cap_fmt.width, cap_fmt.height, cap_fmt.length, cap_fmt.channel,
						cap_fmt.width_stride[0], cap_fmt.width_stride[1], cap_fmt.width_stride[2]);*/
					ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_VENCODE, SOCK_DEFAULT_TIMEOUT);
					if (SOCK_RW_CHECK_OK == ret) {
						/*LOG("%s: send cap_fmt.buffer to PC bufferdata = 0x%x 0x%x 0x%x\n", __FUNCTION__,
										*(cap_fmt.buffer+3), *(cap_fmt.buffer+4), *(cap_fmt.buffer+5));*/
						ret = sock_write(sock_fd, (const void *)cap_fmt.buffer, cap_fmt.length, SOCK_DEFAULT_TIMEOUT);
					} else {
						LOG("%s: fail to send cap_fmt.buffer to PC, ret=%d\n",__FUNCTION__, ret);
					}
				}
			} else if(type == SOCK_CMD_VENCODE_STOP) {
				VencVe2IspParam pVe2IspParam;
				memset(&pVe2IspParam, 0, sizeof(VencVe2IspParam));
				isp_set_attr_cfg(isp_sel, ISP_CTRL_VENC2ISP_PARAM, &pVe2IspParam);
				LOG("reveive stop socket command, break loop and close vencoder\n");
				comm_packet.ret = htonl(SOCK_CMD_RET_OK);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
				g_thread_status &= ~TH_STATUS_PREVIEW_VENCODE;
				//break;
			} else {
				LOG("%s: unknown cmd = %d vencode init status = %d\n", __FUNCTION__, type, encode_init);
				comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
				ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
			}
		} else {
			if (ETIMEDOUT == errno) {
				timeout_times++;
				if (timeout_times > SERVER_TIMEOUT_MAX) {
					LOG("%s: timeout too many times\n", __FUNCTION__);
					break;
				}
			} else {
				timeout_times = 0;
			}
		}
	}
	if (encode_init == 1)
		EncoderClose(&encode_param);

	close(sock_fd);
	free(cap_fmt.buffer);
	cap_fmt.buffer = NULL;
	g_thread_status &= ~TH_STATUS_PREVIEW_VENCODE;
	LOG("%s: exits - %d\n", __FUNCTION__, sock_fd);

	return 0;
}
#endif
