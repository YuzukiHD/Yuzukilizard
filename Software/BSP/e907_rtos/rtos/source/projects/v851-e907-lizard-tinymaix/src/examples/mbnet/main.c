/* Copyright 2022 Sipeed Technology Co., Ltd. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <openamp/sunxi_helper/openamp.h>
#include <stdio.h>

#include "../../TinyMaix/tinymaix.h"

// https://github.com/fchollet/deep-learning-models/releases

#define IMG_L 96

#if IMG_L == 128
#include "pic128.h"
#if TM_MDL_TYPE == TM_MDL_FP32
#include "../tools/tmdl/mbnet128_0.25_f.h"
#elif TM_MDL_TYPE == TM_MDL_FP16
#include "../tools/tmdl/mbnet128_0.25_fp16.h"
#elif TM_MDL_TYPE == TM_MDL_INT8
#include "../tools/mbtestv2.h"
#elif TM_MDL_TYPE == TM_MDL_FP8_143
#include "../tools/tmdl/mbnet128_0.25_fp8_143.h"
#elif TM_MDL_TYPE == TM_MDL_FP8_152
#include "../tools/tmdl/mbnet128_0.25_fp8_152.h"
#else
#error "wrong mdl type!"
#endif
#elif IMG_L == 96
#include "pic96.h"
#if TM_MDL_TYPE == TM_MDL_FP32
#include "../tools/tmdl/mbnet96_0.25_f.h"
#elif TM_MDL_TYPE == TM_MDL_FP16
#include "../tools/tmdl/mbnet96_0.25_fp16.h"
#elif TM_MDL_TYPE == TM_MDL_INT8
#include "../tools/tmdl/mbnet96_0.25_q.h"
#else
#error "wrong mdl type!"
#endif
#else
#error "err pic size"
#endif

extern const char *mbnet_labels[1000];

static uint32_t _t0, _t1;
static tm_err_t layer_cb(tm_mdl_t *mdl, tml_head_t *lh) { // dump middle result
  int h = lh->out_dims[1];
  int w = lh->out_dims[2];
  int ch = lh->out_dims[3];
  mtype_t *output = TML_GET_OUTPUT(mdl, lh);
  _t1 = TM_GET_US();
  _t0 = TM_GET_US();
  return TM_OK;
}

static void parse_output(tm_mat_t *outs) {
  tm_mat_t out = outs[0];
  float *data = out.dataf;
  float maxp = 0;
  int maxi = -1;
  for (int i = 0; i < 1000; i++) {
    if (data[i] > maxp) {
      maxi = i;
      maxp = data[i];
    }
  }
  TM_PRINTF("### Predict output is: Class %d (%s), Prob %.3f\n", maxi,
            mbnet_labels[maxi], maxp);
  return;
}

int tinymaix_mbnet_demo(int argc, char **argv) {
  TM_DBGT_INIT();
  TM_PRINTF("mbnet demo\n");
  tm_mdl_t mdl;

  tm_mat_t in_uint8 = {3, IMG_L, IMG_L, 3, {(mtype_t *)pic_mbnet}};
  tm_mat_t in = {3, IMG_L, IMG_L, 3, {NULL}};
  tm_mat_t outs[1];
  tm_err_t res;
  tm_stat((tm_mdlbin_t *)mbnet_mdl_data);

  res = tm_load(&mdl, mbnet_mdl_data, NULL, layer_cb, &in);
  if (res != TM_OK) {
    TM_PRINTF("tm model load err %d\n", res);
    return -1;
  }
#if (TM_MDL_TYPE == TM_MDL_INT8) || (TM_MDL_TYPE == TM_MDL_INT16)
  res = tm_preprocess(&mdl, TMPP_UINT2INT, &in_uint8, &in);
#else
  res = tm_preprocess(&mdl, TMPP_UINT2FP01, &in_uint8, &in);
#endif
  TM_DBGT_START();
  _t0 = clock();
  res = tm_run(&mdl, &in, outs);
  TM_DBGT("tm_run");
  if (res == TM_OK)
    parse_output(outs);
  else
    TM_PRINTF("tm run error: %d\n", res);
  tm_unload(&mdl);
  return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(tinymaix_mbnet_demo, tinymaix_mbnet_demo,
                            TinyMaix mbnet demo)