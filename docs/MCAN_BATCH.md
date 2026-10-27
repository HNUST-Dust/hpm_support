# HPM MCAN 批次提交与撤销

`drivers/can/mcan_hpmicro.c` 为 HPM5E31 的 MCAN 增加专用接口，声明位于
`include/drivers/can/hpmicro_mcan_batch.h`。CASTCAN 的 `hpm_can_send_batch()`
保持不变；新接口不以逐帧 `can_send()` 模拟批次。

## 提交

`hpm_mcan_send_batch(dev, frames, count, timeout, callback, user)`：

- Classic CAN，支持标准/扩展帧和 RTR，最多 16 帧，并受实际配置的 dedicated TX buffer
  数量限制。CAN FD 模式默认只有 4 个 dedicated buffer，不能承诺 16 帧。
- 检查所有 ID、标志、DLC、数量和设备类型，再接纳整个批次。
- 将各帧装入 dedicated buffer 0..count-1，全部成功后只写一次 TXBAR。
  装载失败不请求发送；CAN 总线仲裁仍可能按 ID 改变线上的发送顺序。
- 每控制器最多一个在途批次。只有全部成员终止才释放接纳令牌并回调一次，
  不能在 buffer 0 完成时提前复用其他 buffer。
- 回调必填，返回零表示驱动已接纳，不代表总线已完成或电机确认。
  整批完成回调为 0；存在未成功发送的撤销成员则为 `-ECANCELED`。
- 接口不可从 ISR 调用。回调可能在 ISR 执行，应用应唤醒工作线程，不在回调里阻塞发送。

`hpm_mcan_send_batch_until(..., deadline_us, ...)` 额外接受绝对微秒启动时间，
与 `k_ticks_to_us_floor64(k_uptime_ticks())` 同域。deadline 必须非零；
在所有消息 RAM 装载完成、紧邻 TXBAR 前复核，过期返回 `-ETIMEDOUT`，
不请求硬件发送、不调用完成回调，并释放提交令牌。普通 batch 接口保留无截止时间语义。

原有 Zephyr `can_send()` 也经过同一接纳/完成逻辑，保留其同步与异步调用方式。
同步等待者取走结果前保留接纳令牌，避免后一批覆盖上一批的信号量或结果。

## 撤销和停止

`hpm_mcan_abort_tx(dev, timeout_us)` 只接受 1..20000 us，不接受无限等待，也不能从 ISR 调用。
它写 TXBCR 并等待对应 TXBRP 位清零；等待预算包含互斥锁获取，实际返回延迟仍受内核
调度/计时粒度影响，不是硬实时延迟保证。成功返回仅表示原硬件请求已不再 pending，
不能撤回已开始或已经发到总线上的帧。

撤销超时返回 `-ETIMEDOUT`，不会伪造完成或提前释放旧批次。上层应进入保护，
重试撤销或停止控制器，不能继续投递业务帧。显式 `can_stop()` 关闭控制器后终止旧批次，
以 `-ENETDOWN` 通知等待者，避免原先停止后发送令牌永久占用的问题。

完成跟踪在 spinlock 下处理，回调在驱动锁外调用；带 generation 的撤销快照不能误完成
随后已接纳的新批次。ISR 在回调前确认本轮中断快照，避免回调/工作线程产生的新事件被
旧 W1C 操作清掉。这里没有实现 EtherCAT 来源租约和软件业务队列，它们由固件集成层负责。

## 验证边界

```sh
./tests/native/test_mcan_tx.sh
```

原生 ASan/UBSan 测试直接使用驱动实际调用的完成状态模块，覆盖 1..16 帧边界、
分次/乱序完成、部分撤销、发送与撤销同时置位、无关事件、重复终止、generation 过期和回绕。
另直接提取真实提交函数体，模拟消息装载耗时，验证最终截止边界拒绝、令牌回收和不产生伪回调。
测试没有模拟整个 RTOS、TXBAR/TXBRP 电路或真实 CAN 仲裁。

已在 dust-adapter 的 dust-hpm5e31 目标中编译完整驱动，未刷板或连接电机。
dust-adapter EtherCAT 应用现已使用带截止时间的接口发送业务，并使用普通批次发送本地保护帧。
后续仍需台架验证实际批次、无 ACK 时撤销、停止/重新启动、回调恰好一次和保护帧交付。
