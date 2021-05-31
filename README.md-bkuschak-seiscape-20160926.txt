 bkuschak / seiscape / README.md

 Latest commit d738deb on Sep 26, 2016



History for seiscape/README.md
Commits on Sep 26, 2016

    Update README.md
    @bkuschak
    bkuschak committed on Sep 26, 2016

Update README.md
@bkuschak
bkuschak committed on Sep 26, 2016

Commits on Feb 14, 2016

    Initial commit
    @haunma
    haunma committed on Feb 14, 2016 



 Update README.md

    master 

@bkuschak
bkuschak committed on Sep 26, 2016
1 parent ca56fb4 commit 3072a05d62bedef1bbaf46b89fb898a9cff2e725
Showing
with 30 additions and 1 deletion.
31 README.md
@@ -1,2 +1,31 @@
# seiscape
Beaglebone cape with 24-bit sigma-delta A/D, TCXO, and split power supplies for a Yuma force-balance vertical seismometer
Beaglebone cape with 24-bit sigma-delta A/D, VCXO, and split power supplies for a Yuma force-balance vertical seismometer

# Board mods
1. Substitute LT1761IS5-SD for U8 ADP7118, due to availability. Lift pin 4 and add 10.5K and 10K feedback divider for 2.5V output.   
2. Change R21 from 115K to 162K. Change R24 from 7.15K to 10K.   
3. Change R31 from 255K to 170K (162K). R26 from 15K to 10K.  
4. Change R23 from 100K to 17.7K to set +15V correctly.  
5. Change C6,C9,C12 to film caps.   
6. FIXME - add divider resistors to R2 and R4 to set +/-10V input span.   
7. Jumper JP16 to select P915 GPIO48 as CAPE_EN  
8. Add UBlox Neo-6M GPS receiver. 47 nH + 15 ohm bias-T, 1K+LED on PPS. Connected to UART4.  
9. Added LEDs on 3.3V power and LED[1:0]
10. FIXME - change ADR445ARMZ to ADR441ARMZ 2.5V ref, or ADR444ARMZ 4.096V ref. 5V is too close to AVDD span.  
11. Remove R9 and connect AD7175 CS# to PRU9_28/CS0

# Pin usage
    CS       <--> P9_28  PRU0_3 / CS0 (CS was tied low through resistor R9)  
    SCLK     <--> P9_31  PRU0_0 / SCK
    DIN      <--> P9_30  PRU0_2 / MOSI
    DOUT/RDY <--> P9_29  PRU0_1 / MISO  
    SYNC     <--> P9_27  PRU0_5 (not used by code yet)  
    CAPE_EN  <--> P9_15  GPIO_48
    PPS      <--> P8_7   TIMER4 and (not jumpered yet)
                  P9_16  PRU0_16 and
                  P8_16  EQEP2_INDEX
    VCXO     <--> P9_41  TCLKIN and (not jumpered yet)
                  P8_12  EQEP2A_IN
    LED0     <--> P8_12  PRU0_14
    LED1     <--> P8_11  PRU0_15

0 comments on commit 3072a05
Please sign in to comment. 




 Update README.md

    master 

@bkuschak
bkuschak committed on Sep 26, 2016
1 parent 3072a05 commit d738deb990c218a6bb2763541adbad1b91ff9b76
Showing
with 1 addition and 0 deletions.
1 README.md
@@ -13,6 +13,7 @@ Beaglebone cape with 24-bit sigma-delta A/D, VCXO, and split power supplies for
9. Added LEDs on 3.3V power and LED[1:0]
10. FIXME - change ADR445ARMZ to ADR441ARMZ 2.5V ref, or ADR444ARMZ 4.096V ref. 5V is too close to AVDD span.  
11. Remove R9 and connect AD7175 CS# to PRU9_28/CS0
12. Increase soft start time: add 1 uF in parallel to both C43 and C44.  Change C23 to 200 nF.

# Pin usage
    CS       <--> P9_28  PRU0_3 / CS0 (CS was tied low through resistor R9)  
0 comments on commit d738deb
Please sign in to comment. 










# seiscape
Beaglebone cape with 24-bit sigma-delta A/D, VCXO, and split power supplies for a Yuma force-balance vertical seismometer

# Board mods
1. Substitute LT1761IS5-SD for U8 ADP7118, due to availability. Lift pin 4 and add 10.5K and 10K feedback divider for 2.5V output.   
2. Change R21 from 115K to 162K. Change R24 from 7.15K to 10K.   
3. Change R31 from 255K to 170K (162K). R26 from 15K to 10K.  
4. Change R23 from 100K to 17.7K to set +15V correctly.  
5. Change C6,C9,C12 to film caps.   
6. FIXME - add divider resistors to R2 and R4 to set +/-10V input span.   
7. Jumper JP16 to select P915 GPIO48 as CAPE_EN  
8. Add UBlox Neo-6M GPS receiver. 47 nH + 15 ohm bias-T, 1K+LED on PPS. Connected to UART4.  
9. Added LEDs on 3.3V power and LED[1:0]
10. FIXME - change ADR445ARMZ to ADR441ARMZ 2.5V ref, or ADR444ARMZ 4.096V ref. 5V is too close to AVDD span.  
11. Remove R9 and connect AD7175 CS# to PRU9_28/CS0
12. Increase soft start time: add 1 uF in parallel to both C43 and C44.  Change C23 to 200 nF.

# Pin usage
    CS       <--> P9_28  PRU0_3 / CS0 (CS was tied low through resistor R9)  
    SCLK     <--> P9_31  PRU0_0 / SCK
    DIN      <--> P9_30  PRU0_2 / MOSI
    DOUT/RDY <--> P9_29  PRU0_1 / MISO  
    SYNC     <--> P9_27  PRU0_5 (not used by code yet)  
    CAPE_EN  <--> P9_15  GPIO_48
    PPS      <--> P8_7   TIMER4 and (not jumpered yet)
                  P9_16  PRU0_16 and
                  P8_16  EQEP2_INDEX
    VCXO     <--> P9_41  TCLKIN and (not jumpered yet)
                  P8_12  EQEP2A_IN
    LED0     <--> P8_12  PRU0_14
    LED1     <--> P8_11  PRU0_15
    

