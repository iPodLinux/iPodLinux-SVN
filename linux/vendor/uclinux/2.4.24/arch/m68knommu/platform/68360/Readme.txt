M68360 port of uClinux:
    May 29, 2001.
    This port was done by SED Systems, a division of Calian Ltd. for use
    on the SIOS hardware. This port is based on the 68328 port and uClinux
    2.0.38.pre7 M68360 support. The devices we have tested are:
        -Timer 1 as system clock (this should be changed to the PIT)
        -Console on SMC 2.
    Included is serial port support for the SCCs. We have had problems with
    these devices (When both SCCs and SMCs are select, the kernel hangs during
    boot, I have not tried SCCs only).

