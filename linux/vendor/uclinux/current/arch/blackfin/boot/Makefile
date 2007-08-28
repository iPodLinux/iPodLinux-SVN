rootfs.o: rootfs.img
	$(CROSS_COMPILE)gcc -c empty.S -o empty.o
	$(OBJCOPY) --add-section=.ramdisk=$< empty.o $@

linux.dxe: $(TOPDIR)/linux rootfs.o
	$(LD) $(LINKFLAGS) $(TOPDIR)/linux rootfs.o -o $@

clean:
	rm -f *.[oa]

dep depend:
	:
