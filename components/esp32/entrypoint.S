    .section .initiram.cpu0_entrypoint, "ax"
    .align 4
    .global cpu0_entrypoint
cpu0_entrypoint:
    movi a0, 0
    wsr a0, WINDOWSTART
    wsr a0, WINDOWBASE
    rsync
    movi a0, 0
    movi sp, _cpu0_initstack_top
    j cpu0_init

    .section .initiram.cpu1_entrypoint, "ax"
    .align 4
    .global cpu1_entrypoint
cpu1_entrypoint:
    movi a0, 0
    movi sp, _cpu1_initstack_top
    j cpu1_init
