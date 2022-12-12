#ifndef __ANS_LIB_H__ 
#define __ANS_LIB_H__

#ifdef __cplusplus
extern "C"
{ 
#endif

typedef void NsHandle;
typedef void DenoiseState;

void Lstm_process_frame(NsHandle **NsHandles, DenoiseState *st, short *processBuffer, short *shBufferIn);
int Rnn_Process_init(NsHandle **NsHandles, DenoiseState **sts, unsigned int sampleRate);
int Rnn_Process_Create( void ***NsHandles, void ***sts);
int Rnn_Process_free(NsHandle **NS_inst, DenoiseState **sts);


//#include <rnn_data.h>
//#include <rnn.h>
//#include <rnnoise.h>
//#include <noise_suppression.h>


#ifdef __cplusplus
}
#endif

#endif

