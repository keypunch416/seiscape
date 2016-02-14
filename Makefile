CFLAGS+=-Wall -Werror
LDLIBS+= -lpthread -lprussdrv

all: seiscape.bin seiscape

clean:
	rm -f seiscape *.o *.bin

seiscape.bin: seiscape.p
	pasm -b $^

seiscape: seiscape.o

MH-SEISCAPE-00A0.dtbo: MH-SEISCAPE.dts
	dtc -@ -I dts -O dtb -o $@ $<

