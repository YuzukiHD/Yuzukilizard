

#include <aec_lib.h>

#define  NN 160	// 8k



// 以16bit 音频位宽 ,8k采样率,mono 声道 为例


// 具体api的参数含义会在:在aec.lib.h中详细说明

void main(void)
{
	
	
	short far_frame[NN];		// 远端语音
	short near_frame[NN];		// 近端语音
	short out_frame[NN];		// aec处理之后的结果

	void *aecmInst = NULL;		// aec实例
	
	
	WebRtcAec_Create(&aecmInst);
	WebRtcAec_Init(aecmInst, 8000, 8000);	//创建初始化,8k采样率

	AecConfig config;
	config.nlpMode = kAecNlpConservative;	//配置aec的算法模式,nlp
	WebRtcAec_set_config(aecmInst, config);

	while(1)	// 循环处理
	{

		WebRtcAec_BufferFarend(aecmInst, far_frame, NN);//对参考声音(回声)的处理
		WebRtcAec_Process(aecmInst, near_frame, NULL, out_frame, NULL, NN,40,0);//回声消除

		
		break;
	}
	
	
	
	

	
}

