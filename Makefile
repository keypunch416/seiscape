CFLAGS+=-Wall -Werror
LDLIBS+= -lm -lpthread -lprussdrv libmseed/libmseed.a

all: seiscape seiscape.bin

clean:
	rm -f seiscape *.o *.bin kiss_fft130/*.o

seiscape.bin: seiscape.p
	pasm -b $^

seiscape: seiscape.o kiss_fft130/kiss_fft.o kiss_fft130/kiss_fftr.o

MH-SEISCAPE-00A0.dtbo: MH-SEISCAPE.dts
	dtc -@ -I dts -O dtb -o $@ $<

