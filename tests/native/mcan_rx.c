/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#define BIT(n) (1U << (n))
#define CAN_FRAME_IDE 1
#define CAN_FRAME_RTR 2
#define CAN_FRAME_FDF 4
#define CAN_FRAME_BRS 8
#define MCAN_INT_RXFIFO0_NEW_MSG 1
#define MCAN_INT_RXFIFO1_NEW_MSG 2
#define MCAN_INT_MSG_STORE_TO_RXBUF 4
#define MCAN_EVENT_TRANSMIT 8
#define MCAN_INT_TX_CANCEL_FINISHED 16
#define MCAN_EVENT_ERROR 32
#define MCAN_RXBUF_SIZE_CAN_DEFAULT 16
#define MCAN_RXF0S_F0FL_GET(x) ((x)&127)
#define MCAN_RXF1S_F1FL_GET(x) ((x)&127)
#define LOG_DBG(...) (void)0
#define status_success 0
struct device;
struct can_frame { uint32_t id; uint8_t flags, dlc, data[64]; };
struct can_filter { uint32_t mask; };
typedef void (*can_rx_callback_t)(const struct device*, struct can_frame*, void*);
typedef struct { uint32_t filter_id, filter_mask; } mcan_filter_elem_t;
typedef struct { uint32_t std_id, ext_id; bool use_ext_id, canfd_frame, rtr, bitrate_switch; uint8_t dlc, data_8[64]; } mcan_rx_message_t;
typedef struct { uint32_t RXF0S, RXF1S, IR; } MCAN_Type;
struct hpm_mcan_config { MCAN_Type *base; };
struct hpm_mcan_data {
    uint32_t std_filter_count, ext_filter_count, filter_rtr, filter_rtr_mask;
    mcan_filter_elem_t std_filters[128], ext_filters[64];
    can_rx_callback_t rx_cb_std[128], rx_cb_ext[64];
    void *rx_cb_arg_std[128], *rx_cb_arg_ext[64];
};
struct device { const struct hpm_mcan_config *config; struct hpm_mcan_data *data; };
static mcan_rx_message_t s_can_rx_buf;
static MCAN_Type registers;
static struct { mcan_rx_message_t frames[128]; unsigned head, count; } fifo[2];
static unsigned received, completions, errors;
static bool inject, fail_read;
static void push(unsigned f, unsigned value) {
    mcan_rx_message_t *r = &fifo[f].frames[(fifo[f].head+fifo[f].count)%128];
    memset(r,0,sizeof(*r)); r->std_id=0x13; r->dlc=8; r->data_8[0]=value;
    ++fifo[f].count;
    if (f) { registers.RXF1S=fifo[f].count; registers.IR |= MCAN_INT_RXFIFO1_NEW_MSG; }
    else { registers.RXF0S=fifo[f].count; registers.IR |= MCAN_INT_RXFIFO0_NEW_MSG; }
}
static int mcan_read_rxfifo(MCAN_Type *can, uint32_t f, mcan_rx_message_t *out) {
    if (fail_read || !fifo[f].count) return -1;
    *out=fifo[f].frames[fifo[f].head]; fifo[f].head=(fifo[f].head+1)%128; --fifo[f].count;
    if (f) can->RXF1S=fifo[f].count; else can->RXF0S=fifo[f].count;
    if (inject) { inject=false; push(f,99); }
    return 0;
}
static uint32_t mcan_get_interrupt_flags(MCAN_Type *can) { return can->IR; }
static void mcan_clear_interrupt_flags(MCAN_Type *can, uint32_t flags) { can->IR &= ~flags; }
static bool mcan_is_rxbuf_data_available(MCAN_Type *can, uint32_t i) { (void)can; (void)i; return false; }
static void mcan_clear_rxbuf_data_available_flag(MCAN_Type *can, uint32_t i) { (void)can; (void)i; }
static void hpm_mcan_get_message_from_rxbuf(const struct device *dev, uint32_t i) { (void)dev; (void)i; assert(false); }
static void hpm_mcan_tc_event_handler(const struct device *dev) { (void)dev; ++completions; }
static void hpm_mcan_state_change_handler(const struct device *dev) { (void)dev; ++errors; }
static unsigned can_dlc_to_bytes(unsigned dlc) { return dlc; }
#include "mcan_rx.inc"
static void callback(const struct device *dev, struct can_frame *frame, void *arg) {
    (void)dev; (void)arg; assert(frame->id == 0x13); ++received;
}
int main(void) {
    struct hpm_mcan_config cfg = {.base=&registers};
    struct hpm_mcan_data data = {0}; struct device dev = {.config=&cfg,.data=&data};
    struct can_filter mask = {.mask=0x7ff};
    assert(convert_std_mask(&mask) == 0x7ff);
    mask.mask=0x1fffffff; assert(convert_ext_mask(&mask) == 0x1fffffff);
    mask.mask=0; assert(convert_std_mask(&mask) == 0);
    data.std_filter_count=1; data.std_filters[0]=(mcan_filter_elem_t){0x13,0x7ff};
    data.rx_cb_std[0]=callback;
    mcan_rx_message_t sample = {.std_id=0x13};
    assert(hpm_mcan_get_first_filter_index(&sample,&data)==0);
    sample.std_id=0x201; assert(hpm_mcan_get_first_filter_index(&sample,&data)==(uint32_t)-EINVAL);
    data.std_filters[0]=(mcan_filter_elem_t){0x120,0x7f0}; sample.std_id=0x12f;
    assert(hpm_mcan_get_first_filter_index(&sample,&data)==0);
    sample.std_id=0x130; assert(hpm_mcan_get_first_filter_index(&sample,&data)==(uint32_t)-EINVAL);
    data.ext_filter_count=1; data.ext_filters[0]=(mcan_filter_elem_t){0x1234567,0x1fffffff};
    sample.use_ext_id=true; sample.ext_id=0x1234567;
    assert(hpm_mcan_get_first_filter_index(&sample,&data)==0);
    sample.ext_id++; assert(hpm_mcan_get_first_filter_index(&sample,&data)==(uint32_t)-EINVAL);
    data.std_filters[0]=(mcan_filter_elem_t){0x13,0x7ff};
    for (unsigned i=0;i<8;++i) { push(0,i); push(1,i); }
    registers.IR |= MCAN_EVENT_TRANSMIT | MCAN_INT_TX_CANCEL_FINISHED | MCAN_EVENT_ERROR;
    hpm_mcan_isr(&dev);
    assert(received==16 && fifo[0].count==0 && fifo[1].count==0 && completions==1 && errors==1);
    for (unsigned i=0;i<8;++i) push(0,i);
    inject=true; hpm_mcan_isr(&dev);
    assert(received==24 && fifo[0].count==1 && (registers.IR & MCAN_INT_RXFIFO0_NEW_MSG));
    hpm_mcan_isr(&dev); assert(received==25 && fifo[0].count==0);
    // Read failure must never deliver stale scratch data to callbacks.
    push(0,1); fail_read=true; hpm_mcan_isr(&dev); assert(received==25);
    fail_read=false; registers.IR |= MCAN_INT_RXFIFO0_NEW_MSG; hpm_mcan_isr(&dev); assert(received==26);
    // Every occupied slot is consumed even if software rejects the identifier.
    push(0,2); fifo[0].frames[fifo[0].head].std_id=0x201;
    push(0,3); hpm_mcan_isr(&dev); assert(received==27 && fifo[0].count==0);
}
