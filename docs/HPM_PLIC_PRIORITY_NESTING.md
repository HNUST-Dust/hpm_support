# HPM PLIC priority nesting

The HPM6750 PLIC driver can allow a higher-priority external interrupt to
preempt a lower-priority PLIC handler. It is opt-in:

```conf
CONFIG_HPM_PLIC_PRIORITY_NESTING=y
CONFIG_HPM_PLIC_MAX_NESTING_DEPTH=4
CONFIG_ISR_STACK_SIZE=4096
```

This first implementation is enabled only for the HPM67 family and is limited
to non-SMP builds. The replacement HPM PLIC driver remains active when nesting
is disabled and then follows Zephyr's original non-nested claim, dispatch and
complete behavior.

## Dispatch policy

After claiming an interrupt, the driver:

1. saves the current PLIC threshold and `mie` mask;
2. raises the threshold to the claimed source's priority;
3. temporarily leaves only the machine-external interrupt enabled in `mie`;
4. enables machine interrupts and dispatches the child ISR;
5. disables machine interrupts, restores `mie` and the previous threshold;
6. completes the claimed source.

The PLIC only presents sources whose priority is strictly greater than the
threshold. Equal- and lower-priority sources therefore remain pending. Machine
timer and software interrupts cannot enter during this nesting window.

At `CONFIG_HPM_PLIC_MAX_NESTING_DEPTH`, the current interrupt still runs but
machine interrupts are not reopened. This bounds interrupt-stack growth.

Every ISR reachable during nesting must be reentrant with respect to the
higher-priority handlers that can preempt it. Size the ISR stack for the worst
allowed depth and validate stack margin on hardware before enabling it.
