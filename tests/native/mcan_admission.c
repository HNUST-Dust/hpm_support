/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "../../drivers/can/hpm_mcan_tx_state.h"
typedef int k_timeout_t;
typedef int k_spinlock_key_t;
typedef int MCAN_Type;
typedef int mcan_tx_frame_t;
struct device { const void *config; void *data; };
struct can_frame {int ignored;};
typedef void (*can_tx_callback_t)(const struct device *, int, void *);
struct hpm_mcan_config {MCAN_Type *base;};
struct hpm_mcan_data {struct {struct {size_t txbuf_dedicated_txbuf_elem_count;} ram_config;} config;bool started;int tx_sem,tx_mutex,tx_fin_sem[1],tx_lock;struct hpm_mcan_tx_state tx_batch;can_tx_callback_t tx_fin_cb[1];void *tx_fin_cb_arg[1];bool tx_sync_waiting;int tx_result;};
enum can_state {CAN_STATE_ERROR_ACTIVE,CAN_STATE_BUS_OFF};
#define MCAN_EVENT_TRANSMIT 1
#define MCAN_INT_TX_CANCEL_FINISHED 2
#define status_success 0
static uint64_t now,load_delay;
static unsigned requests,callbacks;
static int k_sem_take(int *s,int timeout){(void)timeout;if(!*s)return -EAGAIN;--*s;return 0;}
static void k_sem_give(int *s){++*s;}
static void k_sem_reset(int *s){*s=0;}
static int k_mutex_lock(int *m,int timeout){(void)timeout;assert(!*m);*m=1;return 0;}
static void k_mutex_unlock(int *m){assert(*m);*m=0;}
static int k_spin_lock(int *m){assert(!*m);*m=1;return 0;}
static void k_spin_unlock(int *m,int key){(void)key;assert(*m);*m=0;}
static uint64_t k_uptime_ticks(void){return now;}
static uint64_t k_ticks_to_us_floor64(uint64_t t){return t;}
static int hpm_mcan_validate_frame(const struct hpm_mcan_data *d,const struct can_frame *f){(void)d;(void)f;return 0;}
static int hpm_mcan_get_state(const struct device *d,enum can_state *s,void *unused){(void)d;(void)unused;*s=CAN_STATE_ERROR_ACTIVE;return 0;}
static void convert_can_frame_to_mcan_frame(const struct can_frame *f,mcan_tx_frame_t *b){(void)f;*b=0;}
static int mcan_write_txbuf(MCAN_Type *d,size_t index,const mcan_tx_frame_t *b){(void)d;(void)index;(void)b;now+=load_delay;return 0;}
static void mcan_clear_interrupt_flags(MCAN_Type *d,int flags){(void)d;(void)flags;}
static void mcan_send_add_multiple_requests(MCAN_Type *d,uint32_t mask){(void)d;assert(mask);++requests;}
#include "mcan_submit.inc"
static void Done(const struct device *d,int result,void *user){(void)d;(void)result;(void)user;++callbacks;}
int main(void){
 MCAN_Type hw=0;struct hpm_mcan_config config={&hw};struct hpm_mcan_data data={0};data.config.ram_config.txbuf_dedicated_txbuf_elem_count=16;data.started=true;data.tx_sem=1;struct device dev={&config,&data};struct can_frame frames[2]={{0},{0}};
 now=90;load_delay=5;
 assert(hpm_mcan_submit(&dev,frames,2,0,100,Done,NULL)==-ETIMEDOUT);
 assert(!requests&&!callbacks&&!data.tx_batch.active&&!data.tx_fin_cb[0]&&data.tx_sem==1&&!data.tx_mutex&&!data.tx_lock);
 now=80;assert(hpm_mcan_submit(&dev,frames,2,0,100,Done,NULL)==0);assert(requests==1&&data.tx_batch.active==3&&data.tx_sem==0);
 assert(hpm_mcan_submit(&dev,frames,1,0,100,Done,NULL)==-EAGAIN&&requests==1);
 puts("Real MCAN submission deadline / TXBAR rejection / token recovery contracts PASS");
}
