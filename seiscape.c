// SeisCape ADC driver host-side code.  This loads the AD7175-2
// bitbang assembly code into Beaglebone's PRU0.
//
// Inspired by Derek Molloy's PRU example code at
// http://exploringbeaglebone.com/chapter13/#High-Speed_Analog_to_Digital_Conversion_ADC_using_the_PRU-ICSS
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include "seiscape_common.h"


#define PRU0    0
//#define MMAP0_LOC   "/sys/class/uio/uio0/maps/map0/"
//#define MMAP1_LOC   "/sys/class/uio/uio0/maps/map1/"

// Register access sequences for the AD7175-2 are encoded as a series
// of variable-length blocks of the form
//
//    Nbytes   Length of SPI bus transaction, bytes (including reg addr)
//    Comms    Single byte to write first; contains read/write flag and reg addr
//    Data     Up to four bytes of data to write, or space to be filled by a read
//
// The sequence is null-terminated (Nbytes=0).

// Initialization/test sequence: read the ID register
// Zero-pad this up to a multiple of four bytes!
const uint8_t init_seq[] = { 3, 0x47, 0x00, 0x00,  // ID
                             3, 0x01, 0x00, 0x08,  // ADCMODE
                         //    3, 0x02, 0x00, 0x48,  // IFMODE
                             3, 0x10, 0x80, 0x01,  // CH0
                             3, 0x20, 0x13, 0x00,  // SETUPCON0
                             3, 0x28, 0x8e, 0xa6,  // FILTCON0
                             0, 0, 0, 0 };

 
// Short function to load a single unsigned int from a sysfs entry
//unsigned int readFileValue(char filename[])
//{
//   FILE* fp;
//   unsigned int value = 0;
//   fp = fopen(filename, "rt");
//   fscanf(fp, "%x", &value);
//   fclose(fp);
//   return value;
//}
 
 
int main()
{
   void *pru_dataram0;
   
   if (getuid() != 0)
   {
      printf("You must run this program as root. Exiting.\n");
      exit(EXIT_FAILURE);
   }

   // Initialize structure used by prussdrv_pruintc_intc
   // PRUSS_INTC_INITDATA is found in pruss_intc_mapping.h
   tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
 
   // Read in the location and address of the shared memory. This value changes
   // each time a new block of memory is allocated.
//   unsigned int PRU_data_addr = readFileValue(MMAP0_LOC "addr");
//   printf("-> the PRU memory is mapped at the base address: %x\n", (PRU_data_addr + 0x2000));
 
//   uint32_t ddr_addr = readFileValue(MMAP1_LOC "addr");
//   uint32_t ddr_size = readFileValue(MMAP1_LOC "size");
//   printf("The DDR External Memory pool has location: 0x%x and size: 0x%x bytes\n", ddr_addr, ddr_size);
 
   // Allocate and initialize memory
   prussdrv_init();
   prussdrv_open(PRU_EVTOUT_0);
 
   // Map the PRU0 data memory so we can read it later.
   if (prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pru_dataram0) == -1)
   {
       printf("PRUSS0_PRU0_DATARAM map failed\n");
       exit(EXIT_FAILURE);
   }

   // Write the AD7175-2 initialization sequence into PRU0 Data RAM0.
   memcpy(pru_dataram0, init_seq, sizeof(init_seq));
 
   // Map the PRU's interrupts
   prussdrv_pruintc_init(&pruss_intc_initdata);
 
   // Load and execute the PRU program on the PRU
   prussdrv_exec_program(PRU0, "./seiscape.bin");
 
   // Wait for event completion from PRU, returns the PRU_EVTOUT_0 number
   int n = prussdrv_pru_wait_event(PRU_EVTOUT_0);
   printf("PRU0 program completed, event number %d.\n", n);
   
   // Read the results of the AD7175-2 init seq
   printf("Register R/W results 0x%x 0x%x 0x%x 0x%x 0x%x\n", (unsigned int) ((uint32_t *)pru_dataram0)[0],
                                                             (unsigned int) ((uint32_t *)pru_dataram0)[1],
                                                             (unsigned int) ((uint32_t *)pru_dataram0)[2],
                                                             (unsigned int) ((uint32_t *)pru_dataram0)[3],
                                                             (unsigned int) ((uint32_t *)pru_dataram0)[4]);
    
   // Disable PRU and close memory mappings
   prussdrv_pru_disable(PRU0);
   prussdrv_exit();
   return EXIT_SUCCESS;
}
