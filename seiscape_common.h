// Common definitions for the seiscape host- and PRU-side code

#define SAMPLES_PER_BLOCK   600
#define BLOCKS_IN_BUFFER    3

// These are pointers into the 8-kB PRU data RAM:
#define BUFFER_PTR_SAVE 252     // Where to store dataram ptr for host
#define BUFFER_START    256
#define BUFFER_END      BUFFER_START + 4*SAMPLES_PER_BLOCK*BLOCKS_IN_BUFFER

