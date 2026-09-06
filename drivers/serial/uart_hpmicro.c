/*
 * Copyright (c) 2022 HPMicro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#define DT_DRV_COMPAT hpmicro_hpm_uart

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/util.h>
#include <soc.h>
#include <hpm_common.h>
#include <hpm_uart_drv.h>
#include <hpm_clock_drv.h>
#ifdef CONFIG_UART_ASYNC_API
#endif
#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>
#include <drivers/uart_hpmicro.h>
LOG_MODULE_REGISTER(uart_hpmicro);
#ifdef CONFIG_UART_ASYNC_API

typedef struct uart_info {
    UART_Type *ptr;
    uint32_t baudrate;
    uint32_t *buff_addr;
} uart_info_t;

typedef struct dma_info {
	struct device *dev;
    uint8_t ch;
    uint8_t *dst_addr;
    uint32_t buff_size;
	uint8_t *next_dst_addr;
	uint32_t next_buff_size;
	uint8_t	current_mode; /*0: need config dma, 1: needn't config dma*/
	uint8_t *current_buf;
	uint32_t current_offset;
	uint32_t current_size; /* current release buff size */
	uint32_t current_total_size;
} dma_info_t;

typedef struct dmamux_info {
    uint8_t ch;
    uint8_t src;
} dmamux_info_t;

struct stream {
	const struct device *dma_dev;
	uint32_t channel; /* stores the channel for dma */
	struct dma_config dma_cfg;
	struct dma_block_config dma_blk_cfg;
};
#endif

struct uart_hpm_cfg {
	UART_Type *base;
	uint32_t clock_name;
	uint32_t clock_src;
	uint32_t parity;
	uint32_t baud_rate;
	uint8_t flow_ctrl;
	uint8_t data_bits;
	bool loopback_en;
	uint8_t stop_bits;
#ifdef CONFIG_UART_ASYNC_API
	bool async_capable;
#endif
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_config_func_t irq_config_func;
	int irq_num;
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
	const struct pinctrl_dev_config *pincfg;
};

struct uart_hpm_data {
	struct uart_config uart_config;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t user_cb;
	void *user_data;
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
#ifdef CONFIG_UART_ASYNC_API
	uart_callback_t idle_user_callback;
	void *idle_user_data;
    volatile bool uart_rx_idle;
    volatile uint32_t uart_receive_data_size;
    uart_info_t uart_info;
    dma_info_t dma_rxinfo;
	dma_info_t dma_txinfo;
	uint32_t idle_time_out_us;
	/* true: HDMA linked descriptor 正在持续写应用提供的环形内存。 */
	bool rx_circular_mode;
	volatile uint32_t status_flags;
	struct stream dma_rx;
	struct stream dma_tx;
#endif
};

#ifdef CONFIG_UART_ASYNC_API
static void uart_hpm_async_rx_flush(const struct device *dev);
static void uart_hpm_handle_rx_timeout(const struct device *dev);
static void uart_hpm_dma_callback(const struct device *dev, void *arg,
				  uint32_t channel, int status);

static int uart_hpm_dma_tx_load(const struct device *dev, const uint8_t *buf, size_t len)
{
	const struct uart_hpm_cfg *cfg = dev->config;
	struct uart_hpm_data *data = dev->data;
	struct dma_block_config *blk_cfg;
	UART_Type *base = cfg->base;

	assert(buf != NULL);

	/* remember active TX DMA channel (used in callback) */
	struct stream *stream = &data->dma_tx;

	blk_cfg = &stream->dma_blk_cfg;

	/* prepare the block for this TX DMA channel */
	memset(blk_cfg, 0, sizeof(struct dma_block_config));

	/* tx direction has memory as source and peripheral as dest. */
	blk_cfg->source_address = (uint32_t)buf;
	stream->dma_cfg.channel_direction = MEMORY_TO_PERIPHERAL;
	blk_cfg->source_addr_adj = DMA_ADDR_ADJ_INCREMENT;
	blk_cfg->dest_addr_adj   = DMA_ADDR_ADJ_NO_CHANGE;

	/* Dest is SPI DATA reg */
	blk_cfg->dest_address = (uint32_t)&(base->THR);
	blk_cfg->block_size = len;
	/* Transfer 1 byte each DMA loop */
	stream->dma_cfg.source_burst_length = 1;

	stream->dma_cfg.head_block = &stream->dma_blk_cfg;
	/* give the client dev as arg, as the callback comes from the dma */
	stream->dma_cfg.user_data = data;
	/* pass our client origin to the dma: data->dma_tx.dma_channel */
	return dma_config(data->dma_tx.dma_dev, data->dma_tx.channel, &stream->dma_cfg);
}

static int uart_hpm_dma_rx_load(const struct device *dev, uint8_t *buf, size_t len)
{
	const struct uart_hpm_cfg *cfg = dev->config;
	struct uart_hpm_data *data = dev->data;
	struct dma_block_config *blk_cfg;
	UART_Type *base = cfg->base;

	assert(buf != NULL);

	/* retrieve active RX DMA channel (used in callback) */
	struct stream *stream = &data->dma_rx;

	blk_cfg = &stream->dma_blk_cfg;

	/* prepare the block for this RX DMA channel */
	memset(blk_cfg, 0, sizeof(struct dma_block_config));

	/*
	 * RX DMA 的职责只有一层：UART RBR/FIFO -> 应用提供的线性 DMA buffer。
	 * 后续由应用选择复制到 ring buffer，或等待 BUF_RELEASED 后零拷贝处理。
	 */
	blk_cfg->dest_address = (uint32_t)buf;
	stream->dma_cfg.channel_direction = PERIPHERAL_TO_MEMORY;
	blk_cfg->source_addr_adj = DMA_ADDR_ADJ_NO_CHANGE;
	blk_cfg->dest_addr_adj   = DMA_ADDR_ADJ_INCREMENT;

	blk_cfg->block_size = len;
	/* 源地址固定为 UART 接收缓冲寄存器 RBR，每个 DMA request 搬 1 字节。 */
	blk_cfg->source_address = (uint32_t)&(base->RBR);
	stream->dma_cfg.source_burst_length = 1;

	stream->dma_cfg.head_block = blk_cfg;
	stream->dma_cfg.user_data = data;

	/* pass our client origin to the dma: data->dma_rx.channel */
	return dma_config(data->dma_rx.dma_dev, data->dma_rx.channel, &stream->dma_cfg);
}

#endif

#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
static int uart_hpm_configure_get(const struct device *dev, struct uart_config *cfg)
{
	struct uart_hpm_data *data = dev->data;
	*cfg = data->uart_config;
	return 0;
}
#endif /* CONFIG_UART_USE_RUNTIME_CONFIGURE */

static int uart_hpm_configure_init(const struct device *dev, const struct uart_config *cfg)
{
	const struct uart_hpm_cfg *const config = dev->config;
	// struct uart_hpm_data * data = dev->data;
	parity_setting_t parity;
	hpm_stat_t stat = status_success;
	uart_config_t uart_config = {0};
	UART_Type * base = (UART_Type *)config->base;

#ifdef CONFIG_UART_ASYNC_API
	// if (!device_is_ready(data->dma_tx.dma_dev)) {
	// 	LOG_ERR("%s device is not ready", data->dma_tx.dma_dev->name);
	// 	return -ENODEV;
	// }

	// if (!device_is_ready(data->dma_rx.dma_dev)) {
	// 	LOG_ERR("%s device is not ready", data->dma_rx.dma_dev->name);
	// 	return -ENODEV;
	// }
#endif
	clock_set_source_divider(config->clock_name, config->clock_src, 1U);
	clock_add_to_group(config->clock_name, 0);

	uart_default_config(base, &uart_config);
	uart_config.src_freq_in_hz =	 clock_get_frequency(config->clock_name);
	uart_config.baudrate = cfg->baudrate;
#ifdef CONFIG_UART_ASYNC_API
	if (config->async_capable) {
		uart_config.fifo_enable = true;
		uart_config.dma_enable = true;
		uart_config.tx_fifo_level = uart_tx_fifo_trg_not_full;
		/*
		 * HPM6750 RX FIFO 为 16 字节，这里选择超过 3/4 的接收阈值。
		 * 达到阈值会产生常规 RX DMA request；不足阈值但连续 4 个字节时间
		 * 没有新数据时，UART 也会产生 timeout DMA request，把 FIFO 尾数送出。
		 */
		uart_config.rx_fifo_level = uart_rx_fifo_trg_gt_three_quarters;
	}
#endif
	switch (cfg->parity) {
	case UART_CFG_PARITY_NONE:
		parity = parity_none;
		break;
	case UART_CFG_PARITY_ODD:
		parity = parity_odd;
		break;
	case UART_CFG_PARITY_EVEN:
		parity = parity_even;
		break;
	default:
		return -ENOTSUP;
	}
	switch (cfg->data_bits) {
	case UART_CFG_DATA_BITS_5:
		uart_config.word_length  = word_length_5_bits;
		break;
	case UART_CFG_DATA_BITS_6:
		uart_config.word_length  = word_length_6_bits;
		break;
	case UART_CFG_DATA_BITS_7:
		uart_config.word_length  = word_length_7_bits;
		break;
	case UART_CFG_DATA_BITS_8:
		uart_config.word_length  = word_length_8_bits;
		break;
	default:
		return -ENOTSUP;
	}
	switch (cfg->stop_bits) {
	case UART_CFG_STOP_BITS_1:
		uart_config.num_of_stop_bits = stop_bits_1;
		break;
	case UART_CFG_STOP_BITS_1_5:
		uart_config.num_of_stop_bits = stop_bits_1_5;
		break;
	case UART_CFG_STOP_BITS_2:
		uart_config.num_of_stop_bits = stop_bits_2;
		break;
	default:
		return -ENOTSUP;
	}
	switch (cfg->flow_ctrl) {
	case UART_CFG_FLOW_CTRL_NONE:
		uart_config.modem_config.auto_flow_ctrl_en = false;
		break;
	case UART_CFG_FLOW_CTRL_RTS_CTS:
		uart_config.modem_config.auto_flow_ctrl_en = true;
		break;
	default:
		return -ENOTSUP;
	}
	uart_config.parity = parity;
	if (config->loopback_en) {
		uart_modem_enable_loopback(base);
	} else {
		uart_modem_disable_loopback(base);
	}
	stat = uart_init(base, &uart_config);
	return stat;
}

static int uart_hpm_init(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;
	struct uart_hpm_data *const data = dev->data;
	struct uart_config *uart_api_config = &data->uart_config;
	int ret;

	ret = pinctrl_apply_state(cfg->pincfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}
	uart_api_config->baudrate = cfg->baud_rate;
	uart_api_config->parity = cfg->parity;
	uart_api_config->stop_bits = cfg->stop_bits;
	uart_api_config->data_bits = cfg->data_bits;
	uart_api_config->flow_ctrl = cfg->flow_ctrl;
	uart_hpm_configure_init(dev, uart_api_config);
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	cfg->irq_config_func(dev);
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

	return 0;
}

static int uart_hpm_poll_in(const struct device *dev, unsigned char *c)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (status_success != uart_receive_byte((UART_Type *)cfg->base, c)) {
		return 1;
	} else {
		return 0;
	}
}

static void uart_hpm_poll_out(const struct device *dev, unsigned char c)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_send_byte((UART_Type *)cfg->base, c);
}

static int uart_hpm_err_check(const struct device *dev)
{
	int errors = 0;

	return errors;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

int uart_hpm_fifo_fill(const struct device *dev, const uint8_t *tx_data,
			 int len)
{
	const struct uart_hpm_cfg *const cfg = dev->config;
	uint8_t num_tx = 0U;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_transmitter_empty) != 0) {
		while ((len - num_tx > 0) && (num_tx < 16)) {
			uart_send_byte((UART_Type *)cfg->base, tx_data[num_tx++]);
		}
	}

	return num_tx;
}

int uart_hpm_fifo_read(const struct device *dev, uint8_t *rx_data,
			 const int size)
{
	const struct uart_hpm_cfg *const cfg = dev->config;
	uint8_t num_rx = 0U;

	while ((size - num_rx > 0) &&
		(uart_check_status((UART_Type *)cfg->base, uart_stat_data_ready) != 0)) {
		uart_receive_byte((UART_Type *)cfg->base, &rx_data[num_rx++]);
	}

	return num_rx;
}

void uart_hpm_irq_tx_enable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	irq_enable(cfg->irq_num);
	uart_enable_irq((UART_Type *)cfg->base, uart_intr_tx_slot_avail);
}

void uart_hpm_irq_tx_disable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_disable_irq((UART_Type *)cfg->base, uart_intr_tx_slot_avail);
}

int uart_hpm_irq_tx_ready(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_transmitter_empty) != 0) {
		return 1;
	} else {
		return 0;
	}
}

int uart_hpm_irq_tx_complete(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_transmitter_empty) != 0) {
		return 1;
	} else {
		return 0;
	}
}

void uart_hpm_irq_rx_enable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	irq_enable(cfg->irq_num);
	uart_enable_irq((UART_Type *)cfg->base, uart_intr_rx_data_avail_or_timeout);
}

void uart_hpm_irq_rx_disable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_disable_irq((UART_Type *)cfg->base, uart_intr_rx_data_avail_or_timeout);
}

int uart_hpm_irq_rx_ready(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_data_ready) != 0) {
		return 1;
	} else {
		return 0;
	}
}

void uart_hpm_irq_err_enable(const struct device *dev)
{
	/*This feature is not supported by the hpmicro driver */
}

void uart_hpm_irq_err_disable(const struct device *dev)
{
	/*This feature is not supported by the hpmicro driver */
}

int uart_hpm_irq_is_pending(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;
	uint32_t irq_id;

	irq_id = uart_get_irq_id((UART_Type *)cfg->base);
	if ((irq_id & uart_intr_id_rx_data_avail) ||
	(irq_id & uart_intr_id_tx_slot_avail)) {
		return 1;
	} else {
		return 0;
	}
}

void uart_hpm_irq_callback_set(const struct device *dev,
				 uart_irq_callback_user_data_t cb,
				 void *user_data)
{
	struct uart_hpm_data *const dev_data = dev->data;

	dev_data->user_cb = cb;
	dev_data->user_data = user_data;
}

__attribute__((section(".isr"))) static void uart_hpm_isr(const struct device *dev)
{
	struct uart_hpm_data * const dev_data = dev->data;
#ifdef CONFIG_UART_ASYNC_API
	const struct uart_hpm_cfg *const cfg = dev->config;
	const uint8_t irq_id = uart_get_irq_id(cfg->base);
	if (cfg->async_capable && (irq_id == uart_intr_id_rx_timeout)) {
		/*
		 * 路径一：UART RX timeout，DMA buffer 尚未收满。
		 * 同一个“4 字节时间无新数据”条件还会让 UART 发出 timeout DMA
		 * request，把低于 FIFO 阈值的尾数送入 DMA buffer。这里暂停 DMA、
		 * 读取 pending_length，并把当前已经到达 DMA buffer、但尚未通过
		 * UART_RX_RDY 发布的字节交给应用。默认模式恢复同一 DMA；若当前设备
		 * 启用了 timeout-switch，则释放 current 并立即启动 next buffer。
		 * timeout DMA request 与 CPU ISR 的具体先后由硬件仲裁；flush 只按
		 * 当时 DMA 的实际进度发布。
		 */
		/*
		 * 循环 DMA 模式绝不能在 timeout 中暂停通道。硬件产生的 timeout
		 * DMA request 会自行把不足 FIFO 阈值的尾数搬入 ring；读取 IIR
		 * 得到 irq_id 已经完成中断应答，这里无需再做软件切换。
		 */
		if (!dev_data->rx_circular_mode) {
			uart_hpm_handle_rx_timeout(dev);
		}
	}
#endif

	if (dev_data->user_cb) {
		dev_data->user_cb(dev, dev_data->user_data);
	}
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

#ifdef CONFIG_UART_ASYNC_API
static void async_user_callback(const struct device *dev, struct uart_event *evt)
{
	const struct uart_hpm_data *data = dev->data;

	if (data->idle_user_callback) {
		data->idle_user_callback(dev, evt, data->idle_user_data);
	}
}

static void async_evt_tx_done(const struct device *dev)
{
	struct uart_hpm_data *data = dev->data;

	LOG_DBG("TX done: %d", data->dma_txinfo.buff_size);
	struct uart_event event = {
		.type = UART_TX_DONE,
		.data.tx.buf = data->dma_txinfo.dst_addr,
		.data.tx.len = data->dma_txinfo.buff_size
	};

	/* Reset TX Buffer */
	data->dma_txinfo.dst_addr = NULL;
	data->dma_txinfo.buff_size = 0U;

	async_user_callback(dev, &event);
}

static void async_evt_rx_rdy(const struct device *dev)
{
	struct uart_hpm_data * const data = dev->data;
	dma_info_t dma_rxinfo = data->dma_rxinfo;

	/*
	 * 用快照构造事件，offset 指向本次新增数据的开头，len 是新增长度。
	 * 先推进驱动内部 offset，再同步调用应用回调；下一次 flush 只报告后续字节。
	 */
	struct uart_event event = {
		.type = UART_RX_RDY,
		.data.rx.buf = dma_rxinfo.current_buf,
		.data.rx.len = dma_rxinfo.current_size,
		.data.rx.offset = dma_rxinfo.current_offset
	};

	/* Update the current pos for new data */
	data->dma_rxinfo.current_offset += dma_rxinfo.current_size;

	/* Only send event for new data */
	if (event.data.rx.len > 0) {
		async_user_callback(dev, &event);
	}
}

static void async_evt_rx_buf_request(const struct device *dev)
{
	/* 请求应用提前提供 next buffer；此调用会同步进入应用回调。 */
	struct uart_event evt = {
		.type = UART_RX_BUF_REQUEST,
	};

	async_user_callback(dev, &evt);
}

static void async_evt_rx_buf_release(const struct device *dev)
{
	struct uart_hpm_data *data = dev->data;
	struct uart_event evt = {
		.type = UART_RX_BUF_RELEASED,
		.data.rx_buf.buf = data->dma_rxinfo.current_buf,
	};
	async_user_callback(dev, &evt);
}

static void uart_hpm_async_rx_flush(const struct device *dev)
{
	struct uart_hpm_data *data = dev->data;
	size_t rx_rcv_len;
	struct dma_status stat;
	int ret;

	ret = dma_get_status(data->dma_rx.dma_dev, data->dma_rx.channel, &stat);
	if (ret) {
		LOG_ERR("ERR dma get status.\r\n");
		data->dma_rxinfo.current_size = 0U;
		return;
	}
	/*
	 * DMA pending_length 表示本次 96 字节传输还剩多少字节未搬运。
	 * 已搬总数 = total - pending；再减去之前已通过 UART_RX_RDY 发布的
	 * current_offset，得到本次新增长度。例如 total=96、offset=0、
	 * pending=14，则本次发布 82 字节。
	 */
	rx_rcv_len = data->dma_rxinfo.current_total_size - 
				 data->dma_rxinfo.current_offset - 
				 stat.pending_length;
	if (rx_rcv_len > 0) {
		data->dma_rxinfo.current_size = rx_rcv_len;
		async_evt_rx_rdy(dev);
	} else {
		data->dma_rxinfo.current_size = 0;
	}
}

static uint32_t uart_hpm_async_tx_flush(const struct device *dev)
{
	struct uart_hpm_data *data = dev->data;
	size_t rx_rcv_len;
	struct dma_status stat;
	int ret;

	ret = dma_get_status(dev, data->dma_tx.channel, &stat);
	if (ret) {
		LOG_ERR("ERR dma get status.\r\n");
	}
	if (!ret) {
		rx_rcv_len = data->dma_txinfo.buff_size - 
					stat.pending_length;
	} else {
		rx_rcv_len = 0;
	}
	return rx_rcv_len;
}

static int uart_hpm_rx_disable(const struct device *dev)
{
	struct uart_hpm_data *data = dev->data;
	const struct uart_hpm_cfg *config = dev->config;

	if (!config->async_capable) {
		return -ENOTSUP;
	}

	const unsigned int key = irq_lock();

	uart_disable_irq(config->base, uart_intr_rx_data_avail_or_timeout);
	
	dma_suspend(data->dma_rx.dma_dev, data->dma_rx.channel);
	uart_hpm_async_rx_flush(dev);
	async_evt_rx_buf_release(dev);

		/* disable dma channel */
	dma_stop(data->dma_rx.dma_dev, data->dma_rx.channel);

	data->dma_rxinfo.next_dst_addr = NULL;
	data->dma_rxinfo.next_buff_size = 0;
	data->dma_rxinfo.current_mode = 0;
	data->dma_rxinfo.current_offset = 0;
	data->dma_rxinfo.current_buf = 0;
	data->dma_rxinfo.current_total_size = 0;
	data->dma_rxinfo.current_size = 0;

	struct uart_event disabled_event = {
		.type = UART_RX_DISABLED
	};

	async_user_callback(dev, &disabled_event);
	irq_unlock(key);
	return 0;
}

static int uart_hpm_dma_replace_rx_buffer(const struct device *dev)
{
	struct uart_hpm_data * data = dev->data;
	unsigned int key = irq_lock();

	if (data->dma_rxinfo.next_dst_addr != NULL) {
		/*
		 * current buffer 收满时走到这里：停止旧 DMA，把
		 * 预登记的 next 提升为 current 并清零 offset。先启动新的 current，再向
		 * 应用请求下一块备用 buffer，缩短 UART 只能依赖硬件 FIFO 的窗口。
		 */
		dma_stop(data->dma_rx.dma_dev, data->dma_rx.channel);
		data->dma_rxinfo.current_buf = data->dma_rxinfo.next_dst_addr;
		data->dma_rxinfo.current_total_size = data->dma_rxinfo.next_buff_size;
		data->dma_rxinfo.current_size = 0;
		data->dma_rxinfo.next_dst_addr = NULL;
		data->dma_rxinfo.next_buff_size = 0;
		data->dma_rxinfo.current_mode = 0;
		data->dma_rxinfo.current_offset = 0;
		uart_hpm_dma_rx_load(dev, data->dma_rxinfo.current_buf, data->dma_rxinfo.current_total_size);
		dma_start(data->dma_rx.dma_dev, data->dma_rx.channel);
		data->dma_rxinfo.current_mode = 1;
		async_evt_rx_buf_request(dev);
		irq_unlock(key);
		return 0;
	} else {
		irq_unlock(key);
		return -EFAULT;
	}
}

static void uart_hpm_handle_rx_timeout(const struct device *dev)
{
	struct uart_hpm_data *data = dev->data;

	/* 冻结 DMA 计数，发布已经到达 current buffer 的新字节，再继续接收。 */
	dma_suspend(data->dma_rx.dma_dev, data->dma_rx.channel);
	uart_hpm_async_rx_flush(dev);
	dma_resume(data->dma_rx.dma_dev, data->dma_rx.channel);
}

static int uart_hpm_callback_set(const struct device *dev, uart_callback_t callback,
				    void *user_data)
{
	struct uart_hpm_data *data = dev->data;

	data->idle_user_callback = callback;
	data->idle_user_data = user_data;

	return 0;
}

int uart_hpm_rx_circular_enable(const struct device *dev, uint8_t *buffer, size_t size)
{
	struct uart_hpm_data *data;
	const struct uart_hpm_cfg *config;
	struct dma_block_config *block;
	volatile uint8_t discard;
	unsigned int key;
	int rc;

	if ((dev == NULL) || (buffer == NULL) || (size == 0U)) {
		return -EINVAL;
	}
	data = dev->data;
	config = dev->config;
	if (!config->async_capable) {
		return -ENOTSUP;
	}

	key = irq_lock();
	if ((data->dma_rxinfo.current_mode != 0U) || data->rx_circular_mode) {
		irq_unlock(key);
		return -EBUSY;
	}

	/* 启动前清掉旧 FIFO 数据和 overrun 状态，ring 的第 0 字节成为新边界。 */
	uart_reset_rx_fifo(config->base);
	while ((config->base->LSR & UART_LSR_OE_MASK) ||
	       (config->base->LSR & UART_LSR_DR_MASK)) {
		discard = config->base->RBR & UART_RBR_RBR_MASK;
	}
	block = &data->dma_rx.dma_blk_cfg;
	memset(block, 0, sizeof(*block));
	block->source_address = (uint32_t)&config->base->RBR;
	block->dest_address = (uint32_t)buffer;
	block->source_addr_adj = DMA_ADDR_ADJ_NO_CHANGE;
	block->dest_addr_adj = DMA_ADDR_ADJ_INCREMENT;
	block->block_size = size;

	data->dma_rx.dma_cfg.channel_direction = PERIPHERAL_TO_MEMORY;
	data->dma_rx.dma_cfg.source_burst_length = 1U;
	data->dma_rx.dma_cfg.block_count = 1U;
	data->dma_rx.dma_cfg.head_block = block;
	data->dma_rx.dma_cfg.user_data = data;
	data->dma_rx.dma_cfg.dma_callback = NULL;
	data->dma_rx.dma_cfg.cyclic = 1U;

	rc = dma_config(data->dma_rx.dma_dev, data->dma_rx.channel,
			&data->dma_rx.dma_cfg);
	if (rc == 0) {
		rc = dma_start(data->dma_rx.dma_dev, data->dma_rx.channel);
	}
	if (rc != 0) {
		data->dma_rx.dma_cfg.cyclic = 0U;
		data->dma_rx.dma_cfg.dma_callback = uart_hpm_dma_callback;
		irq_unlock(key);
		return rc;
	}

	data->dma_rxinfo.current_buf = buffer;
	data->dma_rxinfo.current_total_size = size;
	data->dma_rxinfo.current_mode = 1U;
	data->rx_circular_mode = true;
	uart_enable_irq(config->base, uart_intr_rx_data_avail_or_timeout);
	irq_unlock(key);
	return 0;
}

int uart_hpm_rx_circular_get_position(const struct device *dev, size_t *position)
{
	struct uart_hpm_data *data;
	struct dma_status status = {0};
	int rc;

	if ((dev == NULL) || (position == NULL)) {
		return -EINVAL;
	}
	data = dev->data;
	if (!data->rx_circular_mode) {
		return -EACCES;
	}
	rc = dma_get_status(data->dma_rx.dma_dev, data->dma_rx.channel, &status);
	if (rc == 0) {
		*position = status.write_position;
	}
	return rc;
}

int uart_hpm_rx_circular_disable(const struct device *dev)
{
	struct uart_hpm_data *data;
	const struct uart_hpm_cfg *config;
	unsigned int key;

	if (dev == NULL) {
		return -EINVAL;
	}
	data = dev->data;
	config = dev->config;
	if (!data->rx_circular_mode) {
		return 0;
	}

	key = irq_lock();
	uart_disable_irq(config->base, uart_intr_rx_data_avail_or_timeout);
	dma_stop(data->dma_rx.dma_dev, data->dma_rx.channel);
	data->rx_circular_mode = false;
	data->dma_rxinfo.current_mode = 0U;
	data->dma_rxinfo.current_buf = NULL;
	data->dma_rxinfo.current_total_size = 0U;
	data->dma_rx.dma_cfg.cyclic = 0U;
	data->dma_rx.dma_cfg.dma_callback = uart_hpm_dma_callback;
	irq_unlock(key);
	return 0;
}

static int uart_hpm_tx(const struct device *dev, const uint8_t *buf, size_t len,
			  int32_t timeout_us)
{
	int stat;
	struct uart_hpm_data *data = dev->data;
	const struct uart_hpm_cfg *config = dev->config;
	UART_Type *uart_base = config->base;

	if (!config->async_capable) {
		return -ENOTSUP;
	}

	uart_reset_tx_fifo(uart_base);
	data->dma_txinfo.dst_addr = (uint8_t *)buf;
	data->dma_txinfo.buff_size = len;
	data->dma_txinfo.dev = (struct device*)dev;
    /* Sending data for the first time */
	stat = uart_hpm_dma_tx_load(dev, buf, len);
	if (stat) {
		return stat;
	}
	stat = dma_start(data->dma_tx.dma_dev, data->dma_tx.channel);
	return stat;
}

static int uart_hpm_tx_abort(const struct device *dev)
{
	int stat;
	struct uart_hpm_data *data = dev->data;
	const struct uart_hpm_cfg *config = dev->config;
	uint32_t len;

	if (!config->async_capable) {
		return -ENOTSUP;
	}

	stat = dma_suspend(data->dma_tx.dma_dev, data->dma_tx.channel);
	if (stat) {
		LOG_ERR("ERR dma suspend error.\r\n");
		return stat;
	}
	len = uart_hpm_async_tx_flush(dev);
	struct uart_event tx_aborted_event = {
			.type = UART_TX_ABORTED,
			.data.tx.buf = data->dma_txinfo.dst_addr,
			.data.tx.len = len
	};
	stat = dma_stop(data->dma_tx.dma_dev, data->dma_tx.channel);
	if (stat) {
		LOG_ERR("ERR dma stop error.\r\n");
		return stat;
	}
	async_user_callback(dev, &tx_aborted_event);

	return 0;
}

static int uart_hpm_rx_enable(const struct device *dev, uint8_t *buf, const size_t len,
				 const int32_t timeout_us)
{
	struct uart_hpm_data *data = dev->data;
	const struct uart_hpm_cfg *config = dev->config;
	UART_Type *uart_base = config->base;
	volatile uint8_t buff;

	if (!config->async_capable) {
		return -ENOTSUP;
	}

	unsigned int key = irq_lock();

	/* 记录应用提供的第一个 buffer，它将成为 current DMA buffer。 */
	data->dma_rxinfo.dst_addr = buf;
	data->dma_rxinfo.buff_size = len;
	data->dma_rxinfo.next_dst_addr = NULL;
	data->dma_rxinfo.next_buff_size = 0;
	data->dma_rxinfo.current_mode = 0;
	/*
	 * 当前驱动只保存该 API 参数，没有用它配置寄存器；FIFO 尾数 DMA request
	 * 和 RX timeout 中断采用 UART 硬件定义的 4 字节时间条件。
	 */
	data->idle_time_out_us = timeout_us;
	data->dma_rxinfo.current_offset = 0;
	data->dma_rxinfo.current_buf = data->dma_rxinfo.dst_addr;
	data->dma_rxinfo.current_total_size = data->dma_rxinfo.buff_size;
	data->dma_rxinfo.current_size = 0;
	data->dma_rxinfo.dev = (struct device *)dev;
	/* 启动新会话前清空 UART RX FIFO/溢出状态，避免把旧字节混入新 buffer。 */
	uart_reset_rx_fifo(uart_base);
	while ((uart_base->LSR & UART_LSR_OE_MASK) || \
			(uart_base->LSR & UART_LSR_DR_MASK)) {
		buff = uart_base->RBR & UART_RBR_RBR_MASK;
	}
	/*
	 * 在启动第一个 DMA 前同步请求 next buffer。应用通常在这个回调里调用
	 * uart_rx_buf_rsp()，这样 current 收满时驱动已有备用 buffer 可切换。
	 */
	async_evt_rx_buf_request(dev);
	uart_enable_irq(uart_base, uart_intr_rx_data_avail_or_timeout);
	/* 配置并启动 UART FIFO -> current buffer 的第一笔 DMA 传输。 */
	uart_hpm_dma_rx_load(dev, data->dma_rxinfo.current_buf, data->dma_rxinfo.current_total_size);
	dma_start(data->dma_rx.dma_dev, data->dma_rx.channel);
	data->dma_rxinfo.current_mode = 1;
	irq_unlock(key);

	return 0;
}

static int mcux_lpuart_rx_buf_rsp(const struct device *dev, uint8_t *buf, size_t len)
{
	struct uart_hpm_data *data = dev->data;
	const struct uart_hpm_cfg *config = dev->config;
	unsigned int key;

	if (!config->async_capable) {
		return -ENOTSUP;
	}

	if ((buf == NULL) || (len == 0U)) {
		return -EINVAL;
	}

	/*
	 * next buffer 可能由应用线程提交，需要和 UART/DMA ISR 串行化，
	 * 不能依赖无锁 assert。
	 */
	key = irq_lock();
	if (data->dma_rxinfo.next_dst_addr != NULL) {
		irq_unlock(key);
		return -EBUSY;
	}
	/* 这里只登记 next；current 收满或 timeout-switch 时才提升并启动它。 */
	data->dma_rxinfo.next_dst_addr = buf;
	data->dma_rxinfo.next_buff_size = len;
	irq_unlock(key);

	return 0;
}

static void uart_hpm_dma_callback(const struct device *dev, void *arg, uint32_t channel, int status)
{
	struct uart_hpm_data *data = arg;

	if (status != 0) {
		LOG_ERR("DMA callback error with channel %d.", channel);
	} else {
		/* identify the origin of this callback */
		if (channel == data->dma_tx.channel) {
			/* this part of the transfer ends */
			async_evt_tx_done(data->dma_txinfo.dev);
		} else if (channel == data->dma_rx.channel) {
			/*
			 * 路径二：current DMA buffer 已收满。
			 * 先 flush 尚未发布的尾部并同步产生 UART_RX_RDY，再通知旧 buffer
			 * released，最后切换到 next buffer 并重新启动 DMA。
			 */
			uart_hpm_async_rx_flush(data->dma_rxinfo.dev);
			if (data->dma_rxinfo.current_size > 0) {
				async_evt_rx_buf_release(data->dma_rxinfo.dev);
			}
			if (uart_hpm_dma_replace_rx_buffer(data->dma_rxinfo.dev) != 0) {
				uart_hpm_rx_disable(data->dma_rxinfo.dev);
			}
		} else {
			LOG_ERR("DMA callback channel %d is not valid.",
								channel);
		}
	}
}

#endif /* CONFIG_UART_ASYNC_API */

static const struct uart_driver_api uart_hpm_driver_api = {
	.poll_in = uart_hpm_poll_in,
	.poll_out = uart_hpm_poll_out,
	.err_check = uart_hpm_err_check,
#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
	.configure = uart_hpm_configure_init,
	.config_get = uart_hpm_configure_get,
#endif
#ifdef CONFIG_UART_ASYNC_API
	.callback_set = uart_hpm_callback_set,
	.tx = uart_hpm_tx,
	.tx_abort = uart_hpm_tx_abort,
	.rx_enable = uart_hpm_rx_enable,
	.rx_buf_rsp = mcux_lpuart_rx_buf_rsp,
	.rx_disable = uart_hpm_rx_disable,
#endif /* CONFIG_UART_ASYNC_API */
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill		  = uart_hpm_fifo_fill,
	.fifo_read		  = uart_hpm_fifo_read,
	.irq_tx_enable	  = uart_hpm_irq_tx_enable,
	.irq_tx_disable	  = uart_hpm_irq_tx_disable,
	.irq_tx_ready	  = uart_hpm_irq_tx_ready,
	.irq_tx_complete  = uart_hpm_irq_tx_complete,
	.irq_rx_enable	  = uart_hpm_irq_rx_enable,
	.irq_rx_disable	  = uart_hpm_irq_rx_disable,
	.irq_rx_ready	  = uart_hpm_irq_rx_ready,
	.irq_err_enable	  = uart_hpm_irq_err_enable,
	.irq_err_disable  = uart_hpm_irq_err_disable,
	.irq_is_pending	  = uart_hpm_irq_is_pending,
	.irq_callback_set = uart_hpm_irq_callback_set,
#endif
};


#define UART_HPMICRO_IRQ_FLAGS(n) 0

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
#define DEV_CONFIG_IRQ_FUNC_INIT(n) \
	.irq_config_func = irq_config_func##n,
#define DEV_CONFIG_IRQ_NUM_INIT(n) \
	.irq_num = DT_INST_IRQN(n),
#define UART_HPMICRO_IRQ_FUNC_DECLARE(n) \
	static void irq_config_func##n(const struct device *dev);
#ifndef CONFIG_UART_ASYNC_API
#define UART_HPMICRO_IRQ_FUNC_DEFINE(n)	\
	static void irq_config_func##n(const struct device *dev)	\
	{	\
		ARG_UNUSED(dev);	\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),	\
		uart_hpm_isr, DEVICE_DT_INST_GET(n),	\
		UART_HPMICRO_IRQ_FLAGS(n));		\
		irq_enable(DT_INST_IRQN(n));	\
	}
#else /* CONFIG_UART_ASYNC_API */
#define UART_HPMICRO_IRQ_FUNC_DEFINE_SYNC(n)	\
	static void irq_config_func##n(const struct device *dev)	\
	{	\
		const struct uart_hpm_cfg *cfg = dev->config;	\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),	\
		uart_hpm_isr, DEVICE_DT_INST_GET(n),	\
		UART_HPMICRO_IRQ_FLAGS(n));		\
		if (!cfg->async_capable) {	\
			irq_enable(DT_INST_IRQN(n));	\
		}	\
	}

#define UART_HPMICRO_IRQ_FUNC_DEFINE(n)	\
	UART_HPMICRO_IRQ_FUNC_DEFINE_SYNC(n)

#endif /* !CONFIG_UART_ASYNC_API */

#else /* !CONFIG_UART_INTERRUPT_DRIVEN */
#define DEV_CONFIG_IRQ_FUNC_INIT(n)
#define DEV_CONFIG_IRQ_NUM_INIT(n)
#define UART_HPMICRO_IRQ_FUNC_DECLARE(n)
#define UART_HPMICRO_IRQ_FUNC_DEFINE(n)
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

#ifdef CONFIG_UART_ASYNC_API
#define UART_HPMICRO_ASYNC_CAPABLE(n) \
	UTIL_AND(DT_INST_DMAS_HAS_NAME(n, rx), DT_INST_DMAS_HAS_NAME(n, tx))
#define UART_HPMICRO_ASYNC_CAPABLE_INIT(n) \
		.async_capable = UART_HPMICRO_ASYNC_CAPABLE(n),
#define UART_HPMICRO_ASYNC_DATA_INIT(n)	\
	COND_CODE_1(UART_HPMICRO_ASYNC_CAPABLE(n), (\
		.uart_rx_idle = false,	\
		.uart_receive_data_size = 0,	\
		.uart_info = {	\
					.ptr = (UART_Type *)DT_INST_REG_ADDR(n),	\
					.baudrate = DT_INST_PROP(n, current_speed),	\
		},	\
	), ())

#define UART_DMA_CHANNELS_DATA_INIT(n)		\
	COND_CODE_1(UART_HPMICRO_ASYNC_CAPABLE(n), (\
		.dma_tx = {						\
			.dma_dev = DEVICE_DT_GET(DT_INST_DMAS_CTLR_BY_NAME(n, tx)), \
			.channel =					\
				DT_INST_DMAS_CELL_BY_NAME(n, tx, channel),	\
			.dma_cfg = {					\
				.channel_direction = MEMORY_TO_PERIPHERAL,	\
				.dma_callback = uart_hpm_dma_callback,		\
				.source_data_size = 1,				\
				.dest_data_size = 1,				\
				.block_count = 1,		\
				.dma_slot = DT_INST_DMAS_CELL_BY_NAME(n, tx, source) \
			}							\
		},								\
		.dma_rx = {						\
			.dma_dev = DEVICE_DT_GET(DT_INST_DMAS_CTLR_BY_NAME(n, rx)), \
			.channel =					\
				DT_INST_DMAS_CELL_BY_NAME(n, rx, channel),	\
			.dma_cfg = {				\
				.channel_direction = PERIPHERAL_TO_MEMORY,	\
				.dma_callback = uart_hpm_dma_callback,		\
				.source_data_size = 1,				\
				.dest_data_size = 1,				\
				.block_count = 1,		\
				.dma_slot = DT_INST_DMAS_CELL_BY_NAME(n, rx, source) \
			}							\
		},	\
	), ())

#else
#define UART_HPMICRO_ASYNC_CAPABLE_INIT(n)
#define UART_HPMICRO_ASYNC_DATA_INIT(n)
#define UART_DMA_CHANNELS_DATA_INIT(n)
#endif

#define PINCTRL_INIT(n) .pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),
#define PINCTRL_DEFINE(n) PINCTRL_DT_INST_DEFINE(n);

#define HPM_UART_INIT(n)	\
		PINCTRL_DEFINE(n)	\
		UART_HPMICRO_IRQ_FUNC_DECLARE(n)	\
		static struct uart_hpm_data uart_hpm_data_##n = {	\
		UART_HPMICRO_ASYNC_DATA_INIT(n)	\
		UART_DMA_CHANNELS_DATA_INIT(n)			\
		};									\
		static const struct uart_hpm_cfg uart_hpm_config_##n = {	\
			.base = (UART_Type *)DT_INST_REG_ADDR(n),	\
			.clock_name = DT_INST_CLOCKS_CELL(n, name),	\
			.clock_src = DT_INST_CLOCKS_CELL(n, src),	\
			.parity = DT_INST_ENUM_IDX_OR(n, parity, UART_CFG_PARITY_NONE),	\
			.stop_bits = DT_INST_ENUM_IDX_OR(n, stop_bits, UART_CFG_STOP_BITS_1),	\
			.data_bits = DT_INST_ENUM_IDX_OR(n, data_bits, UART_CFG_DATA_BITS_8),	\
			.loopback_en = DT_INST_PROP(n, loopback),				\
			.baud_rate = DT_INST_PROP(n, current_speed),	\
			.flow_ctrl = DT_INST_PROP(n, hw_flow_control) ?				\
			UART_CFG_FLOW_CTRL_RTS_CTS : UART_CFG_FLOW_CTRL_NONE,		\
			UART_HPMICRO_ASYNC_CAPABLE_INIT(n)	\
			DEV_CONFIG_IRQ_FUNC_INIT(n)		\
			DEV_CONFIG_IRQ_NUM_INIT(n)		\
			PINCTRL_INIT(n)		\
		};	\
		DEVICE_DT_INST_DEFINE(n, uart_hpm_init,	\
					NULL,	\
					&uart_hpm_data_##n,	\
					&uart_hpm_config_##n, PRE_KERNEL_1,	\
					CONFIG_SERIAL_INIT_PRIORITY,	\
					&uart_hpm_driver_api);	\
		UART_HPMICRO_IRQ_FUNC_DEFINE(n)

DT_INST_FOREACH_STATUS_OKAY(HPM_UART_INIT)
