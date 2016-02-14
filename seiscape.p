// PRU program to communicate with the AD7175-2 on the SeisCape board.
// (inspired by an example in Derek Molloy's "Exploring Beaglebone")
//
// AD7175-2 connections:
//   (CS tied low through resistor R9)
//   SCLK     <--> P9.31  pr1_pru0_pru_r30_0
//   DIN      <--> P9.30  pr1_pru0_pru_r30_2
//   DOUT/RDY <--> P9.29  pr1_pru0_pru_r31_1
//   SYNC     <--> P9.27  pr1_pru0_pru_r30_5  (currently unused; set and leave high)
//
 
.setcallreg r29.w0
.origin 0                       // start of program in PRU memory
.entrypoint start
 
#include "seiscape_common.h"

#define PRU0_R31_VEC_VALID 32   // allows notification of program completion
#define PRU_EVTOUT_0    3       // the event number that is sent back

#define SCLK_COUNT      500     // Loop count for SCLK half-cycle


start:

// Enable the OCP master port -- allows transfer of data to Linux userspace
LBCO    r0, c4, 4, 4       // load SYSCFG reg into r0 (use c4 const addr)
CLR     r0, r0, 4          // clear bit 4 (STANDBY_INIT)
SBCO    r0, c4, 4, 4       // store the modified r0 back at the load addr

MOV     r1, 0x0            // PRU RAM base address

// Reset the AD7175-2 with 64 SCLKS while DIN=1, then wait ~ 1 ms
SET     r30.t2
MOV     r18, 64
next_reset_clk:
CALL    sclk_low
CALL    sclk_high
SUB     r18, r18, 1
QBNE    next_reset_clk, r18, 0
MOV     r20, 200000
waitloop:
SUB     r20, r20, 1
QBNE    waitloop, r20, 0

next_transaction:
LBBO    r9.b0, r1, 0, 1    // Number of bytes in SPI transaction => r9
QBEQ    start_sampling, r9.b0, 0  // Done if this is the terminating null
LBBO    r10.b0, r1, 1, 1   // Comms register => r10
ADD     r1, r1, 2

write_comreg:
// First operation is always a write (to the comms register).
MOV     r17.b3, r10.b0     // output shift register
// Shift the byte from r17.b3 onto the AD7175-2 SPI bus.
MOV     r18, 8             // # of bits to send
next_bit_w1:
QBBC    data_low_w1, r17.t31
SET     r30.t2
QBA     one_clock_w1
data_low_w1:
CLR     r30.t2
one_clock_w1:
CALL    sclk_low           // drive edge + delay
CALL    sclk_high          // sample edge + delay
LSL     r17, r17, 1
SUB     r18, r18, 1
QBNE    next_bit_w1, r18, 0

// Now proceed with a the remaining bytes, to either a series of byte-
// reads (which we store into RAM) or byte-writes.
QBBC    writes, r10.t6     // Is this a read or a write operation?

reads:
SUB     r9, r9, 1          // Decrement ops count
QBEQ    next_transaction, r9.b0, 0  // Finished when r9 == 0
// Shift in a byte from the AD7175-2 SPI bus to r17.b0.
MOV     r18, 8             // # of bits to receive
next_bit_r:
LSL     r17, r17, 1
CALL    sclk_low           // AD7175 drives, and we delay
QBBC    data_low_r, r31.t1 // read DOUT
SET     r17.t0             // DOUT was high
QBA     one_clock_r
data_low_r:
CLR     r17.t0             // DOUT was low
one_clock_r:
CALL    sclk_high          // we sample, then delay
SUB     r18, r18, 1
QBNE    next_bit_r, r18, 0
SBBO    r17.b0, r1, 0, 1   // Store r17 result into RAM
ADD     r1, r1, 1          // Advance PRU RAM ptr
QBA     reads

writes:
SUB     r9, r9, 1          // Decrement ops count
QBEQ    next_transaction, r9.b0, 0  // Finished when r9 == 0
LBBO    r17.b3, r1, 0, 1   // Fetch next byte into r17.b3
// Shift the byte from r17.b3 onto the AD7175-2 SPI bus.
MOV     r18, 8             // # of bits to send
next_bit_w2:
QBBC    data_low_w2, r17.t31
SET     r30.t2             // drive DIN high
QBA     one_clock_w2
data_low_w2:
CLR     r30.t2             // drive DIN low
one_clock_w2:
CALL    sclk_low           // we drive, then delay
CALL    sclk_high          // AD7175 samples, and we delay
LSL     r17, r17, 1
SUB     r18, r18, 1
QBNE    next_bit_w2, r18, 0
ADD     r1, r1, 1          // Advance PRU RAM ptr
QBA     writes             // Go write next byte


start_sampling:
// Initialization is complete; now collect ADC samples
MOV     r1, BUFFER_START

get_new_block:
MOV     r11, SAMPLES_PER_BLOCK  // How many samples per EVTOUT signal

sample_wait:
// Wait for READY signal
QBBS    sample_wait, r31.t1

// Write 0x44 to comms reg (read reg addr 0x04).
MOV     r17.b3, 0x44
// Shift the byte from r17.b3 onto the AD7175-2 SPI bus.
MOV     r18, 8             // # of bits to send
next_bit_rs1:
QBBC    data_low_rs1, r17.t31
SET     r30.t2
QBA     one_clock_rs1
data_low_rs1:
CLR     r30.t2
one_clock_rs1:
CALL    sclk_low           // drive edge + delay
CALL    sclk_high          // sample edge + delay
LSL     r17, r17, 1
SUB     r18, r18, 1
QBNE    next_bit_rs1, r18, 0

// Read 24-bit data sample.
MOV     r17, 0             // Clear data shift register
MOV     r18, 24            // # of bits to receive
next_bit_rs2:
LSL     r17, r17, 1
CALL    sclk_low           // AD7175 drives, and we delay
QBBC    data_low_rs2, r31.t1 // read DOUT
SET     r17.t0             // DOUT was high
QBA     one_clock_rs2
data_low_rs2:
CLR     r17.t0             // DOUT was low
one_clock_rs2:
CALL    sclk_high          // we sample, then delay
SUB     r18, r18, 1
QBNE    next_bit_rs2, r18, 0
SBBO    r17, r1, 0, 4      // Store r17 result into RAM
ADD     r1, r1, 4          // Advance PRU RAM ptr

// Decrement counter and loop until zero
SUB     r11, r11, 1
QBNE    sample_wait, r11, 0

// Done with this block; tell host
MOV     r2, BUFFER_PTR_SAVE
SBBO    r1, r2, 0, 4       // Pass dataram ptr to host at special addr
MOV     r31.b0, PRU0_R31_VEC_VALID | PRU_EVTOUT_0

// If dataram ptr has reached the end of the buffer, loop back to
// "start_sampling" to reset it, otherwise just start the next block.
MOV     r2, BUFFER_END
QBLT    get_new_block, r2, r1
QBA     start_sampling




// "Subroutines"

// Set SCLK low and delay SCLK_LEN cycles
sclk_low:
MOV     r20, SCLK_COUNT
CLR     r30.t0
loop1:
SUB     r20, r20, 1
QBNE    loop1, r20, 0
RET

// Set SCLK high and delay SCLK_LEN cycles
sclk_high:
MOV     r20, SCLK_COUNT
SET     r30.t0
loop2:
SUB     r20, r20, 1
QBNE    loop2, r20, 0
RET
