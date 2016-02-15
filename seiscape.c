// SeisCape ADC driver host-side code.  This loads the AD7175-2
// bitbang assembly code into Beaglebone's PRU0.

#include <arpa/inet.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include "seiscape_common.h"
#include "seiscape.h"
#include "kiss_fft130/kiss_fft.h"
#include "kiss_fft130/kiss_fftr.h"
#include "libmseed/libmseed.h"

#define PRU0    0

#define NFFT    1024

// Register access sequences for the AD7175-2 are encoded as a series
// of variable-length blocks of the form
//
//    Nbytes   Length of SPI bus transaction, bytes (including reg addr)
//    Comms    Single byte to write first; contains read/write flag and reg addr
//    Data     Up to four bytes of data to write, or space to be filled by a read
//
// The sequence is null-terminated (Nbytes=0).
//
// Initialization/test sequence: read the ID register
// Zero-pad this up to a multiple of four bytes!
const uint8_t init_seq[] = { 3, 0x47, 0x00, 0x00,  // ID
                             3, 0x01, 0x00, 0x08,  // ADCMODE
                         //    3, 0x02, 0x00, 0x48,  // IFMODE
                             3, 0x10, 0x80, 0x01,  // CH0
                             3, 0x20, 0x13, 0x00,  // SETUPCON0
                             3, 0x28, 0x8e, 0xa6,  // FILTCON0
                             0, 0, 0, 0 };

// Buffers for FFT-based filtering
static kiss_fft_scalar d3fir[NFFT];
static kiss_fft_scalar inputBlock[NFFT];
static kiss_fft_scalar outputBlock[NFFT];
static kiss_fft_cpx    D3fir[NFFT/2 + 1];
static kiss_fft_cpx    InputBlock[NFFT/2 + 1];
static kiss_fft_cpx    OutputBlock[NFFT/2 + 1];
static int32_t dataBuf[OUTBUF_LEN];
static int64_t timeBuf[OUTBUF_LEN];
static int bufPtr = 0;



// Handler for finished miniseed records, called as often as necessary
// by libmseed's msr_pack().  We use it to send a datalink packet
// to the ringserver instance.
void HandleMSRecord(char *record, int recLen, void *handlerData)
{
    char dlPkt[600];
  
    int dlSocket = ((HandlerData *) handlerData)->dlSocket;
    int64_t *timeBuf = ((HandlerData *) handlerData)->timeBuf;
    // There ought to be a better way to obtain this information, but
    // I don't see any other way.  We need the number of data samples
    // represented by this miniseed record, in order to set the
    // datalink start/end times correctly.
    int nSamples = record[30]*256 + record[31];
   
    // Generate a datalink header.
    int dlHeaderLen = sprintf(dlPkt, "DLxWRITE nn_sssss_ll_ccc/MSEED %" PRId64 " %" PRId64 " N 512", timeBuf[0], timeBuf[nSamples-1]);
    // Fill in the blanks above.
    dlPkt[2] = dlHeaderLen-3;    // datalink header-length byte
    memcpy(&dlPkt[9],  &record[18], 2);     // network code
    memcpy(&dlPkt[12], &record[8],  5);     // station identifier
    memcpy(&dlPkt[18], &record[13], 2);     // location identifier
    memcpy(&dlPkt[21], &record[15], 3);     // channel identifier
        
    // Copy the 512-byte miniseed packet into place and send to ringserver.
    memcpy(&dlPkt[dlHeaderLen], record, recLen);
    if (send(dlSocket, dlPkt, dlHeaderLen+recLen, 0) < 0)
    {
        printf("ringserver data send failed @ %" PRId64 "\n", timeBuf[0]);
    }
}


// Initialize a socket and open a TCP/IP connection to a datalink
// server, typically an instance of the "ringserver" program.
//
// Returns the datalink socket on success, -1 on failure.
int InitDatalink()
{
    int len, ringserver_socket;
    struct sockaddr_in ringserver;
    char rsPkt[100];
    
    // Create sockets
    ringserver_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ringserver_socket == -1)
    {
        printf("Could not create socket(s)\n");
        return(-1);
    }

    // Ringserver datalink address/port
    ringserver.sin_addr.s_addr = inet_addr("127.0.0.1");
    ringserver.sin_family = AF_INET;
    ringserver.sin_port = htons(16000);

    // Connect to ringserver datalink socket
    if (connect(ringserver_socket, (struct sockaddr *)&ringserver, sizeof(ringserver)) < 0)
    {
        printf("Could not connect to ringserver\n");
        return(-1);
    }

    // Say hello to ringserver
    if (send(ringserver_socket, "DL\x18ID ws2rs:mark:1234:Linux", 27, 0) < 0)
    {
        printf("ringserver client-ID send failed.\n");
        return(-1);
    }
    
    // Parse ringserver's reply
    if ((len = recv(ringserver_socket, rsPkt, 100, 0)) < 0)
    {
        puts("ringserver recv failed");
        return 1;
    }
    rsPkt[len] = 0;
    printf("Connected to ringserver: %s\n", rsPkt);
    
    return ringserver_socket;
}


// Initialize the miniseed template record with our station-specific
// information.  It will be used by msr_pack() when generating
// miniseed data packets.
void InitMSHeaders(MSRecord *msr)
{
    // Populate MSRecord values
    strcpy(msr->network, "AM");
    strcpy(msr->station, "BRIER");
    strcpy(msr->location, "01");
    strcpy(msr->channel, "BHZ");

    msr->dataquality = 'D';
    msr->samprate = 20.0;
    msr->reclen = 512;          // Seedlink-compliant 512-byte packets
    msr->encoding = DE_STEIM2;  // Steim 2 compression
    msr->byteorder = 1;         // big-endian byte order
    msr->sampletype = 'i';      // declare type to be 32-bit integers
}


// Initialize the PRU-ICSS #0 and start our program running.
// On success, returns a void pointer to the PRU's data RAM mapped
// into our address space.  On failure, returns NULL.
void *InitPRU()
{
    void *pru_dataram0;
   
    if (getuid() != 0)
    {
        printf("You must run this program as root.\n");
        return NULL;
    }

    // Initialize structure used by prussdrv_pruintc_intc
    // PRUSS_INTC_INITDATA is found in pruss_intc_mapping.h
    tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
 
    // Allocate and initialize memory
    prussdrv_init();
    prussdrv_open(PRU_EVTOUT_0);
 
    // Map the PRU0 data memory so we can read it later.
    if (prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pru_dataram0) == -1)
    {
        printf("PRUSS0_PRU0_DATARAM map failed\n");
        return NULL;
    }
    else
    {
        printf("PRUSS0_PRU0_DATARAM @ %p\n", pru_dataram0);
    }
    

    // Write the AD7175-2 initialization sequence into PRU0 Data RAM0.
    memcpy(pru_dataram0, init_seq, sizeof(init_seq));
 
    // Map the PRU's interrupts
    prussdrv_pruintc_init(&pruss_intc_initdata);
 
    // Load and execute the PRU program on the PRU
    prussdrv_exec_program(PRU0, "./seiscape.bin");
    
    return pru_dataram0;
}
    


int main()
{
    int firstBlock = 1;
    int k, numRec;
    int64_t lastSampleTime, consumedSamples;
    HandlerData hData;
    MSRecord *msr;
    void *pru;
    struct timespec tp;

//int n = 0;
    
    // Allocate the template miniseed record and populate static header fields.
    msr = msr_init(NULL);
    InitMSHeaders(msr);
    
    // Initialize communication with the datalink server.
    hData.dlSocket = InitDatalink();
    // Give the record handler a reference to the time buffer.
    hData.timeBuf = timeBuf;
    
    // Prepare 3x decimating filter and buffers FFT-based calculation.
    memset(d3fir, 0, NFFT*sizeof(kiss_fft_scalar));  // zero padding
    memcpy(d3fir, DECIMATION_FILTER, DECIMATION_FILTER_LEN*sizeof(int32_t));
    
    // Set up 1024-pt forward transform.
    kiss_fftr_cfg cfgFwd = kiss_fftr_alloc(NFFT, 0, NULL, NULL);
    // Set up 1024-pt inverse transform.
    kiss_fftr_cfg cfgInv = kiss_fftr_alloc(NFFT, 1, NULL, NULL);
    
    // put kth sample in real_in[k]
    kiss_fftr(cfgFwd, d3fir, D3fir);
    // transformed. DC is in cmplx_out[0].r and cmplx_out[0].i.
    // size of cmplx_out is Nfft/2+1.

//    for (k = 0 ; k < NFFT/2 ; k+=5)
//    {
//        printf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
//            D3fir[k].r,   D3fir[k].i, D3fir[k+1].r, D3fir[k+1].i, D3fir[k+2].r,
//            D3fir[k+2].i, D3fir[k+3].r, D3fir[k+3].i, D3fir[k+4].r, D3fir[k+4].i);
//    }
            
    // Start the PRU and get a pointer to its data RAM.
    if ((pru = InitPRU()) == NULL)
    {
        printf("PRU init failed.  Exiting...\n");
        exit(EXIT_FAILURE);
    }
    
    while (1)
    {
        // Wait for EVTOUT_0, which signals that a block of A/D samples is ready.
        k = prussdrv_pru_wait_event(PRU_EVTOUT_0);

        // Record the time (eventually this should use hardware timing information)
        clock_gettime(CLOCK_REALTIME, &tp);
        // Assume last sample was acquired "now" with added corrections
        // for the group delay of the decimation filter, and a "fudge
        // factor".
        lastSampleTime = (int64_t)tp.tv_sec * 1000000L + ((int64_t)tp.tv_nsec / 1000L)
            - (int64_t) floor((double)(DECIMATION_FILTER_LEN-1)*0.5e6/60.0 + 0.5)
            - 1000UL;  // fudge factor

        // Extract the PRU data pointer from its storage location.
        int32_t *blockStart = (int32_t *) (pru + *((uint32_t *)(pru + BUFFER_PTR_SAVE)) - 4*SAMPLES_PER_BLOCK);

        printf("Got EVTOUT_0 #%d -> %p\n", k, (void *)blockStart);
        for (k = 0 ; k < SAMPLES_PER_BLOCK ; k += 10)
        {
            printf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                blockStart[k],   blockStart[k+1], blockStart[k+2], blockStart[k+3], blockStart[k+4],
                blockStart[k+5], blockStart[k+6], blockStart[k+7], blockStart[k+8], blockStart[k+9]);
        }

        // Fill the FFT input buffer with a new block of A/D samples.
        // Per the overlap-save method, the first DECIMATION_FILTER_LEN-1 samples 
        // are overlapping from the previous block, or, on the very first block,
        // zero.
        if (firstBlock)
        {
            memset(inputBlock, 0, (DECIMATION_FILTER_LEN-1)*sizeof(kiss_fft_scalar));
            firstBlock = 0;
        }
        else
        {
            memcpy(inputBlock, &inputBlock[NFFT-DECIMATION_FILTER_LEN+1], (DECIMATION_FILTER_LEN-1)*sizeof(kiss_fft_scalar));
        }

        for (k = 0 ; k < SAMPLES_PER_BLOCK ; k++)
        {   // Copy in the new samlpes, converting the A/D's offset binary to int32
            inputBlock[k+DECIMATION_FILTER_LEN-1] = (blockStart[k] - (1 << 23)) << 8;
        }
        
        // Fourier-transform the input block.
        kiss_fftr(cfgFwd, inputBlock, InputBlock);
        // Multiply complex vectors (input block * filter) in frequency domain.
        for (k = 0 ; k < NFFT/2 + 1 ; k++)
        {
            OutputBlock[k].r = (((int64_t) InputBlock[k].r * (int64_t) D3fir[k].r) >> 20)
                             - (((int64_t) InputBlock[k].i * (int64_t) D3fir[k].i) >> 20);
            OutputBlock[k].i = (((int64_t) InputBlock[k].r * (int64_t) D3fir[k].i) >> 20)
                             + (((int64_t) InputBlock[k].i * (int64_t) D3fir[k].r) >> 20);
        }
        // Inverse transform to get the output samples.
        kiss_fftri(cfgInv, OutputBlock, outputBlock);
        
        // diagnostic dump
        printf("Output:\n");
//        for (k = DECIMATION_FILTER_LEN-1 ; k < NFFT ; k += 30)
//        {
//            printf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
//                outputBlock[k],    outputBlock[k+3],  outputBlock[k+6],  outputBlock[k+9],  outputBlock[k+12],
//                outputBlock[k+15], outputBlock[k+18], outputBlock[k+21], outputBlock[k+24], outputBlock[k+27]);
//        }
//        for (k = 0 ; k < NFFT/2 ; k+=5)
//        {
//            printf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
//                OutputBlock[k].r,   OutputBlock[k].i, OutputBlock[k+1].r, OutputBlock[k+1].i, OutputBlock[k+2].r,
//                OutputBlock[k+2].i, OutputBlock[k+3].r, OutputBlock[k+3].i, OutputBlock[k+4].r, OutputBlock[k+4].i);
//        }

        // Keep last SAMPLES_PER_BLOCK samples (overlap-save method),
        // and decimate by 3.
        for (k = DECIMATION_FILTER_LEN-1 ; k < NFFT ; k += 3)
        {
            dataBuf[bufPtr] = outputBlock[k];
//dataBuf[bufPtr] = (int32_t) floor(8388608.0 * sin((3.14/2000000.0)*(double)(n*n)) + 0.5);
//if (++n >= 20000) n = 0;
            timeBuf[bufPtr] = lastSampleTime
                - (int64_t) floor((double)(NFFT-1-k)*1.0e6/60.0 + 0.5);

            if (++bufPtr == OUTBUF_LEN)
            {   // This should never happen...
                printf("Output buffer overrun @ %" PRId64 ".\n", timeBuf[bufPtr-1]);
                bufPtr = 0;
            }
        }

        // Pack into 512-byte miniseed records.  This does nothing
        // unless there is enough data to fill at least one record.
        msr->starttime = timeBuf[0];
        msr->datasamples = dataBuf;
        msr->numsamples = bufPtr;

printf("lastSampleTime = %" PRId64 ", timeBuf[0] = %" PRId64 ", msr->numsamples = %d\n", lastSampleTime, timeBuf[0], bufPtr);
        if ((numRec = msr_pack(msr, &HandleMSRecord, &hData, &consumedSamples, 0, 1)) < 0)
        {
            printf("msr_pack() failed @ %" PRId64 ".\n", timeBuf[0]);
        }
        else
        {
            printf("msr_pack() used %" PRIu64 " samples in %d records\n", consumedSamples, numRec);
            
            if (consumedSamples > 0)
            {   // Shift remaining dataBuf/timeBuf samples down to start of array.
                memcpy(dataBuf, &dataBuf[consumedSamples], (bufPtr-consumedSamples)*sizeof(dataBuf[0]));
                memcpy(timeBuf, &timeBuf[consumedSamples], (bufPtr-consumedSamples)*sizeof(timeBuf[0]));
                bufPtr -= consumedSamples;
            }
        }
        
        // Clear the PRU EVTOUT, otherwise it can't fire again.
        if (prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT))
        {
            printf("prussdrv_pru_clear_event() failed\n");
        }
    }

// NOT REACHED
    // Free miniseed record template.
    msr_free(&msr);

    // Free FFT tables.
    free(cfgFwd);
    free(cfgInv);

    // Disable PRU and close memory mappings.
    prussdrv_pru_disable(PRU0);
    prussdrv_exit();
    return EXIT_SUCCESS;
}
