#ifndef _RC_CORE_PRIV_
#define _RC_CORE_PRIV_

#include <stdint.h>
#include <stdbool.h>
#include <sunxi_input.h>

#define CIR_FIFO_SIZE		(128)
typedef void (*_ir_key_proc)(uint32_t key_value);
#define IR_NOT_CHECK_USER_CODE (0x00) //not check user code
#define NEC_IR_ADDR_CODE 0x00ff

typedef struct {
   uint32_t duration;
   unsigned pulse:1;
} cir_raw_event;

typedef struct {
   int state;
   unsigned count;
   uint32_t bits;
   bool is_nec_x;
   bool necx_repeat;
} nec_desc;

typedef struct {
  cir_raw_event event[CIR_FIFO_SIZE];
  nec_desc nec;
  bool pulse;
  bool is_receiving;
  uint32_t value;
  uint8_t index;
  volatile uint8_t pre_value; //previous key value
  volatile uint8_t crt_value; //current key value
  volatile rt_timer_t hTimer; // timer handle,for detect key
  volatile uint32_t user_code;//user code or address, 0:NO CHECK user code or address code.other value: check it!
  volatile uint32_t press_cnt;
  volatile bool valid;
  //Key use routines to send key messages to the upper layer
  _ir_key_proc key_down;
  _ir_key_proc key_up;
  struct sunxi_input_dev *sunxi_ir_dev;
} hal_cir_info_t;

static inline bool geq_margin(unsigned d1, unsigned d2, unsigned margin)
{
   return d1 > (d2 - margin);
}

static inline bool eq_margin(unsigned d1, unsigned d2, unsigned margin)
{
   return ((d1 > (d2 - margin)) && (d1 < (d2 + margin)));
}

void nec_desc_init(nec_desc *nec);
int ir_nec_decode(hal_cir_info_t *info, cir_raw_event ev);
void hal_cir_put_value(hal_cir_info_t *info, uint32_t value);
void hal_cir_long_press_value(hal_cir_info_t *info, uint32_t value);

#endif
