include $(MSCDIR)/software/common.mak

UIPDIR=../uip
LIBUIPDIR=../libuip

CFLAGS += $(CPPFLAGS) -I. \
	-I$(UIPDIR) \
	-I$(LIBUIPDIR)

OBJECTS=isr.o main.o

all: uiptest.bin

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
	chmod -x $@
	cp $@ boot.bin

uiptest.elf: $(OBJECTS) libs

%.elf:
	$(LD) $(LDFLAGS) \
		-T $(MSCDIR)/software/libbase/linker-sdram.ld \
		-N -o $@ \
		$(MSCDIR)/software/libbase/crt0-$(CPU).o \
		$(OBJECTS) \
		-L$(MSCDIR)/software/libbase \
		-L$(MSCDIR)/software/libcompiler-rt \
		-L../libuip \
		-lbase -luip -lcompiler-rt
	chmod -x $@

main.o: main.c
	$(compile-dep)

%.o: %.c
	$(compile-dep)

%.o: %.S
	$(assemble)

libs:
	$(MAKE) -C $(MSCDIR)/software/libcompiler-rt
	$(MAKE) -C $(MSCDIR)/software/libbase
	$(MAKE) -C ../libuip

load: uiptest.bin
	$(MAKE) -C $(MSCDIR)/tools
	$(MSCDIR)/tools/flterm --port /dev/ttyUSB0 --kernel uiptest.bin


clean:
	$(RM) $(OBJECTS) $(OBJECTS:.o=.d) uiptest.elf uiptest.bin
	$(RM) .*~ *~

.PHONY: all main.o clean libs load
