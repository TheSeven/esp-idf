    .section .text.cpu0_entrypoint, "ax"
    .align 4
    .global cpu0_entrypoint
cpu0_entrypoint:
    movi a0, 0
    movi a1, _initstack_top
    j call_start_cpu0
