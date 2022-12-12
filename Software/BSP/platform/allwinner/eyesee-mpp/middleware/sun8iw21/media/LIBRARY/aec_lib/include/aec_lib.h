enum {
  kAecNlpConservative = 0,
  kAecNlpModerate,
  kAecNlpAggressive
};

enum {
  kAecFalse = 0,
  kAecTrue
};


#if 1
typedef struct PowerLevel {
  float sfrsum;
  int sfrcounter;
  float framelevel;
  float frsum;
  int frcounter;
  float minlevel;
  float averagelevel;
} PowerLevel;

typedef struct Stats {
  float instant;
  float average;
  float min;
  float max;
  float sum;
  float hisum;
  float himean;
  int counter;
  int hicounter;
} Stats; 

enum Wrap {
  SAME_WRAP,
  DIFF_WRAP
};

typedef struct RingBuffer {
  size_t read_pos;
  size_t write_pos;
  size_t element_count;
  size_t element_size;
  enum Wrap rw_wrap;
  char* data;
}RingBuffer;
enum {
  kExtendedNumPartitions = 32
};


enum {
  kMaxDelayBlocks = 60
};
enum {
  kLookaheadBlocks = 15
};
enum {
  kHistorySizeBlocks = kMaxDelayBlocks + kLookaheadBlocks
};

typedef float complex_t[2];
#endif /* 0 */



#define FRAME_LEN       80             /* Total frame length, 10 ms. */

#define PART_LEN        64             /* Length of partition. */
#define PART_LEN_SHIFT  7              /* Length of (PART_LEN * 2) in base 2. */

#define PART_LEN1       (PART_LEN + 1)  /* Unique fft coefficients. */
#define PART_LEN2       (PART_LEN << 1) /* Length of partition * 2. */
#define PART_LEN4       (PART_LEN << 2) /* Length of partition * 4. */
#define FAR_BUF_LEN     PART_LEN4       /* Length of buffers. */
#define MAX_DELAY       100


#if 1
typedef struct AecCore {
  int farBufWritePos, farBufReadPos;

  int knownDelay;
  int inSamples, outSamples;
  int delayEstCtr;

  RingBuffer* nearFrBuf;
  RingBuffer* outFrBuf;

  RingBuffer* nearFrBufH;
  RingBuffer* outFrBufH;

  float dBuf[PART_LEN2];  // nearend
  float eBuf[PART_LEN2];  // error

  float dBufH[PART_LEN2];  // nearend

  float xPow[PART_LEN1];
  float dPow[PART_LEN1];
  float dMinPow[PART_LEN1];
  float dInitMinPow[PART_LEN1];
  float* noisePow;

  float xfBuf[2][kExtendedNumPartitions * PART_LEN1];  // farend fft buffer
  float wfBuf[2][kExtendedNumPartitions * PART_LEN1];  // filter fft
  complex_t sde[PART_LEN1];  // cross-psd of nearend and error
  complex_t sxd[PART_LEN1];  // cross-psd of farend and nearend
  // Farend windowed fft buffer.
  complex_t xfwBuf[kExtendedNumPartitions * PART_LEN1];

  float sx[PART_LEN1], sd[PART_LEN1], se[PART_LEN1];  // far, near, error psd
  float hNs[PART_LEN1];
  float hNlFbMin, hNlFbLocalMin;
  float hNlXdAvgMin;
  int hNlNewMin, hNlMinCtr;
  float overDrive, overDriveSm;
  int nlp_mode;
  float outBuf[PART_LEN];
  int delayIdx;

  short stNearState, echoState;
  short divergeState;

  int xfBufBlockPos;

  RingBuffer* far_buf;
  RingBuffer* far_buf_windowed;
  int system_delay;  // Current system delay buffered in AEC.

  int mult;  // sampling frequency multiple
  int sampFreq;
  unsigned int seed;

  float normal_mu;               // stepsize
  float normal_error_threshold;  // error threshold

  int noiseEstCtr;

  PowerLevel farlevel;
  PowerLevel nearlevel;
  PowerLevel linoutlevel;
  PowerLevel nlpoutlevel;

  int metricsMode;
  int stateCounter;
  Stats erl;
  Stats erle;
  Stats aNlp;
  Stats rerl;

  // Quantities to control H band scaling for SWB input
  int freq_avg_ic;       // initial bin for averaging nlp gain
  int flag_Hband_cn;     // for comfort noise
  float cn_scale_Hband;  // scale for comfort noise in H band

  int delay_histogram[kHistorySizeBlocks];
  int delay_logging_enabled;
  void* delay_estimator_farend;
  void* delay_estimator;

  // 1 = extended filter mode enabled, 0 = disabled.
  int extended_filter_enabled;
  // Runtime selection of number of filter partitions.
  int num_partitions;

#ifdef WEBRTC_AEC_DEBUG_DUMP
  RingBuffer* far_time_buf;
  FILE* farFile;
  FILE* nearFile;
  FILE* outFile;
  FILE* outLinearFile;
#endif
}AecCore;
#endif /* 0 */



typedef struct {
  int16_t nlpMode;      // default kAecNlpModerate
  int16_t skewMode;     // default kAecFalse
  int16_t metricsMode;  // default kAecFalse
  int delay_logging;    // default kAecFalse
  // float realSkew;
} AecConfig;



typedef struct aecpc_t{
  int delayCtr;
  int sampFreq;
  int splitSampFreq;
  int scSampFreq;
  float sampFactor;  // scSampRate / sampFreq
  short skewMode;
  int bufSizeStart;
  int knownDelay;
  int rate_factor;

  short initFlag;  // indicates if AEC has been initialized

  // Variables used for averaging far end buffer size
  short counter;
  int sum;
  short firstVal;
  short checkBufSizeCtr;

  // Variables used for delay shifts
  short msInSndCardBuf;
  short filtDelay;  // Filtered delay estimate.
  int timeForDelayChange;
  int startup_phase;
  int checkBuffSize;
  short lastDelayDiff;

#ifdef WEBRTC_AEC_DEBUG_DUMP
  RingBuffer* far_pre_buf_s16;  // Time domain far-end pre-buffer in int16_t.
  FILE* bufFile;
  FILE* delayFile;
  FILE* skewFile;
#endif

  // Structures
  void* resampler;

  int skewFrCtr;
  int resample;  // if the skew is small enough we don't resample
  int highSkewCtr;
  float skew;

  RingBuffer* far_pre_buf;  // Time domain far-end pre-buffer.
  int lastError;

  int farend_started;

  AecCore* aec;
} aecpc_t;





/*
 * Allocates the memory needed by the AEC. The memory needs to be initialized
 * separately using the WebRtcAec_Init() function.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void **aecInst               Pointer to the AEC instance to be created
 *                              and initialized
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * int32_t return               0: OK
 *                             -1: error
 */
int32_t WebRtcAec_Create(void** aecInst);




/*
 * Initializes an AEC instance.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void           *aecInst      Pointer to the AEC instance
 * int32_t        sampFreq      Sampling frequency of data
 * int32_t        scSampFreq    Soundcard sampling frequency
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * int32_t        return        0: OK
 *                             -1: error
 */
int32_t WebRtcAec_Init(void* aecInst, int32_t sampFreq, int32_t scSampFreq);


int32_t WebRtcAec_Free(void* aecInst);


/*
 * This function enables the user to set certain parameters on-the-fly.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void           *handle       Pointer to the AEC instance
 * AecConfig      config        Config instance that contains all
 *                              properties to be set
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * int            return         0: OK
 *                              -1: error
 */
int WebRtcAec_set_config(void* handle, AecConfig config);




/*
 * Inserts an 80 or 160 sample block of data into the farend buffer.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void           *aecInst      Pointer to the AEC instance
 * int16_t        *farend       In buffer containing one frame of
 *                              farend signal for L band
 * int16_t        nrOfSamples   Number of samples in farend buffer
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * int32_t        return        0: OK
 *                             -1: error
 */
int32_t WebRtcAec_BufferFarend(void* aecInst,
                               const int16_t* farend,
                               int16_t nrOfSamples);


							   
							   
							   
/*
 * Runs the echo canceller on an 80 or 160 sample blocks of data.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void          *aecInst       Pointer to the AEC instance
 * int16_t       *nearend       In buffer containing one frame of
 *                              nearend+echo signal for L band
 * int16_t       *nearendH      In buffer containing one frame of
 *                              nearend+echo signal for H band
 * int16_t       nrOfSamples    Number of samples in nearend buffer
 * int16_t       msInSndCardBuf Delay estimate for sound card and
 *                              system buffers
 * int16_t       skew           Difference between number of samples played
 *                              and recorded at the soundcard (for clock skew
 *                              compensation)
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * int16_t       *out           Out buffer, one frame of processed nearend
 *                              for L band
 * int16_t       *outH          Out buffer, one frame of processed nearend
 *                              for H band
 * int32_t       return         0: OK
 *                             -1: error
 */
int32_t WebRtcAec_Process(void* aecInst,
                          const int16_t* nearend,
                          const int16_t* nearendH,
                          int16_t* out,
                          int16_t* outH,
                          int16_t nrOfSamples,
                          int16_t msInSndCardBuf,
                          int32_t skew);
