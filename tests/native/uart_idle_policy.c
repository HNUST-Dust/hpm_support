/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../../drivers/serial/uart_hpm_idle_policy.h"
int main(void) {
    uint8_t bits = 42;
    assert(uart_hpm_idle_threshold(115200, 1000, &bits) == 0 && bits == 116);
    assert(uart_hpm_idle_threshold(100000, 1000, &bits) == 0 && bits == 100);
    assert(uart_hpm_idle_threshold(921600, 1000, &bits) == -ERANGE);
    assert(uart_hpm_idle_threshold(1000000, 255, &bits) == 0 && bits == 255);
    assert(uart_hpm_idle_threshold(1000000, 256, &bits) == -ERANGE);
    assert(uart_hpm_idle_threshold(115200, 1, &bits) == 0 && bits == 1);
    assert(uart_hpm_idle_threshold(115200, -1, &bits) == 0 && bits == 0);
    assert(uart_hpm_idle_threshold(0, 1000, &bits) == -EINVAL);
    assert(uart_hpm_idle_threshold(115200, 0, &bits) == -EINVAL);
    assert(uart_hpm_idle_threshold(115200, -2, &bits) == -EINVAL);
    assert(uart_hpm_idle_threshold(UINT32_MAX, INT32_MAX, &bits) == -ERANGE);
    assert(uart_hpm_idle_threshold(115200, 1000, NULL) == -EINVAL);
    uint32_t length = 999;
    assert(uart_hpm_idle_delta(128, 0, 110, &length) == 0 && length == 18);
    /* 同一个 idle 重复处理不能重复发布，下一段只报告新字节。 */
    assert(uart_hpm_idle_delta(128, 18, 110, &length) == 0 && length == 0);
    assert(uart_hpm_idle_delta(128, 18, 96, &length) == 0 && length == 14);
    assert(uart_hpm_idle_delta(128, 32, 0, &length) == -EAGAIN);
    assert(uart_hpm_idle_delta(128, 32, 129, &length) == -EIO);
    assert(uart_hpm_idle_delta(128, 33, 96, &length) == -EIO);
    assert(uart_hpm_idle_resume(1, 1, true));
    assert(!uart_hpm_idle_resume(1, 2, true));
    assert(!uart_hpm_idle_resume(1, 1, false));
    assert(!uart_hpm_idle_resume(UINT32_MAX, 0, true));
    puts("UART native idle timing / partial publication contracts PASS");
}
