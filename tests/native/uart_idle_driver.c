/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "../../drivers/serial/uart_hpm_idle_policy.h"
#define CONFIG_UART_ASYNC_API 1
#define CONFIG_UART_HPMICRO_RX_NATIVE_IDLE 1
#define uart_intr_rx_line_idle 1
#define uart_intr_rx_data_avail_or_timeout 2
#define uart_intr_id_rx_timeout 12
#define LOG_ERR(...) ((void)0)
struct device { const void *config; void *data; };
struct uart_registers { bool idle, detection; unsigned interrupts; };
struct uart_hpm_cfg { struct uart_registers *base; bool async_capable; };
struct dma_status { bool busy; uint32_t pending_length; };
struct dma_info { uint8_t *current_buf, *next_dst_addr; uint32_t current_total_size,
    current_offset, current_size, next_buff_size; bool current_mode; };
typedef struct dma_info dma_info_t;
enum { UART_RX_RDY, UART_RX_BUF_RELEASED, UART_RX_DISABLED, UART_RX_STOPPED };
struct uart_event { int type; union {
    struct { uint8_t *buf; uint32_t len, offset; } rx;
    struct { uint8_t *buf; } rx_buf;
    struct { int reason; struct {uint8_t *buf; uint32_t len, offset;} data; } rx_stop;
} data; };
struct uart_hpm_data {
    bool rx_native_idle_mode, rx_line_idle_mode, rx_circular_mode;
    uint32_t rx_generation;
    dma_info_t dma_rxinfo;
    struct { void *dma_dev; uint32_t channel; } dma_rx;
    void (*idle_user_callback)(const struct device *, struct uart_event *, void *);
    void *idle_user_data;
    void (*user_cb)(const struct device *, void *);
    void *user_data;
};
static struct dma_status dma;
static unsigned resumed, released, ready, disabled, stopped_count;
static int dma_error;
static bool terminal_during_suspend, restart_on_disabled, stop_on_ready;
static uint8_t a[128], b[128], fresh[128];
static unsigned irq_lock(void) { return 0; }
static void irq_unlock(unsigned key) { (void)key; }
static void uart_disable_irq(struct uart_registers *p, unsigned mask) { p->interrupts &= ~mask; }
static void uart_disable_rxline_idle_detection(struct uart_registers *p) { p->detection = false; }
static void uart_clear_rxline_idle_flag(struct uart_registers *p) { p->idle = false; }
static bool uart_is_rxline_idle(struct uart_registers *p) { return p->idle; }
static uint8_t uart_get_irq_id(struct uart_registers *p) { (void)p; return 0; }
static int dma_get_status(void *dev, unsigned channel, struct dma_status *status)
{ (void)dev; (void)channel; *status = dma; return dma_error; }
static int dma_suspend(void *dev, unsigned channel)
{ (void)dev; (void)channel; if (terminal_during_suspend) dma.pending_length = 0; return 0; }
static int dma_resume(void *dev, unsigned channel)
{ (void)dev; (void)channel; ++resumed; return 0; }
static int dma_stop(void *dev, unsigned channel)
{ (void)dev; (void)channel; dma.busy = false; return 0; }
static void uart_hpm_handle_rx_timeout(const struct device *dev) { (void)dev; assert(false); }
int uart_hpm_rx_line_idle_flush(const struct device *dev);
#include "uart_idle_functions.inc"

static void Callback(const struct device *dev, struct uart_event *event, void *user)
{
    (void)user;
    struct uart_hpm_data *data = dev->data;
    if (event->type == UART_RX_RDY) {
        assert(event->data.rx.buf == a);
        assert(event->data.rx.offset == ready);
        ready += event->data.rx.len;
        if (stop_on_ready) { stop_on_ready = false; uart_hpm_rx_disable(dev); }
    } else if (event->type == UART_RX_BUF_RELEASED) {
        assert(event->data.rx_buf.buf == a || event->data.rx_buf.buf == b);
        ++released;
    } else if (event->type == UART_RX_STOPPED) {
        assert(event->data.rx_stop.reason == 0);
        ++stopped_count;
    } else {
        ++disabled;
        if (restart_on_disabled) {
            ++data->rx_generation;
            data->dma_rxinfo = (dma_info_t){.current_buf=fresh, .current_mode=true,
                .current_total_size=128};
            data->rx_native_idle_mode = true;
            dma = (struct dma_status){true, 128};
        }
    }
}
static void Reset(struct device *dev)
{
    struct uart_hpm_data *data = dev->data;
    memset(data, 0, sizeof(*data));
    data->rx_native_idle_mode = true;
    data->rx_generation = 1;
    data->dma_rxinfo = (dma_info_t){.current_buf=a, .next_dst_addr=b,
        .current_total_size=128, .next_buff_size=128, .current_mode=true};
    data->idle_user_callback = Callback;
    dma = (struct dma_status){true, 110};
    resumed=ready=released=disabled=stopped_count=0;
    dma_error=0;
    terminal_during_suspend=restart_on_disabled=stop_on_ready=false;
}
int main(void)
{
    struct uart_registers registers = {0};
    struct uart_hpm_cfg config = {&registers, true};
    struct uart_hpm_data data;
    struct device dev = {&config, &data};
    Reset(&dev);
    registers.idle = true;
    uart_hpm_isr(&dev);
    assert(!registers.idle && ready == 18 && resumed == 1 && released == 0);
    registers.idle = true;
    uart_hpm_isr(&dev);
    assert(ready == 18 && resumed == 2);
    dma.pending_length = 96;
    assert(uart_hpm_rx_line_idle_flush(&dev) == 14 && ready == 32);
    Reset(&dev);
    terminal_during_suspend = true;
    assert(uart_hpm_rx_line_idle_flush(&dev) == -EAGAIN && ready == 0 && released == 0);
    Reset(&dev);
    stop_on_ready = restart_on_disabled = true;
    assert(uart_hpm_rx_line_idle_flush(&dev) == 18);
    assert(ready == 18 && released == 2 && disabled == 1 && resumed == 0);
    assert(data.dma_rxinfo.current_buf == fresh && data.dma_rxinfo.current_mode);
    Reset(&dev);
    dma_error = -EIO;
    assert(uart_hpm_rx_line_idle_flush(&dev) == -EIO && !ready && !resumed);
    Reset(&dev);
    dma_error = -EIO;
    registers.idle = true;
    uart_hpm_isr(&dev);
    assert(stopped_count == 1 && disabled == 1 && released == 2 && !ready && !resumed);
    assert(!data.rx_native_idle_mode && !data.dma_rxinfo.current_mode);
    Reset(&dev);
    data.rx_circular_mode = true;
    assert(uart_hpm_rx_line_idle_flush(&dev) == -EAGAIN && !ready && !resumed);
    Reset(&dev);
    uart_hpm_rx_disable(&dev);
    assert(!data.rx_native_idle_mode && !data.dma_rxinfo.current_mode);
    assert(ready == 18 && released == 2 && disabled == 1);
    puts("Real UART driver idle ISR / DMA partial flush / callback restart contracts PASS");
}
