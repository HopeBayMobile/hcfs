/*
  Enable Debug card from ITE IT8712F
  Programming by Tasuka Hsu
  Jun 28 2011
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/io.h>
#include <signal.h>

#define LOCK_FILE "/var/run/debugCard.pid"
#define ADDRESS_PORT 			0x2e
#define DATA_PORT 				0x2f

#if (!defined ADDRESS_PORT || !defined DATA_PORT)
  #define ADDRESS_PORT 			0x4e
  #define DATA_PORT 			0x4f
#endif

#define GPIO_PORT0 				0x2a8	// Right
#define GPIO_PORT1 				0x2a0	// Left
#define LED_PORT				GPIO_PORT0+1	// Green/Yellow LED in bit 0 and bit 1

#define LED_RED					0x01
#define LED_YELLOW				0x02

#define DATA_LENGTH 			1 		// in byte
#define ON 						1
#define OFF 					0
#define DELAY 					100 	// 100ms

/* it8712 global */
#define it87_chipID_Hi			0x20
#define it87_chipID_Lo			0x21
#define it87_configLDN			0x07
#define it87_CHIP_ID			0x8712

/* GPIO Port */
#define gpioLDN					0x07
#define gpioSet1Idx				0x25
#define gpioSet2Idx				0x26
#define gpioSet1SelReg 			0x2a
#define LogicalBlockLockReg 	0x2b

#define gpioSet1PinPolarityReg 	0xb0
#define gpioSet2PinPolarityReg	0xb1
#define gpioSet1PinPullUpReg 	0xb8
#define gpioSet2PinPullUpReg	0xb9
#define gpioSet1EnableReg 		0xc0
#define gpioSet2EnableReg		0xc1
#define gpioSet1OutputEnableReg	0xc8
#define gpioSet2OutputEnableReg	0xc9

#define gpioSimpleIOAddressMSB	0x62
#define gpioSimpleIOAddressLSB	0x63

/* Paralll port */
#define parallelLDN				0x03
#define parallelEnableReg		0x30
#define parallelInterruptReg	0x70
#define parallelDMAReg			0x74
#define parallelModeReg			0xf0

#define parallelAddressMSB		0x60
#define parallelAddressLSB		0x61

/* Game port */
#define gamePortLDN				0x09
#define gamePortEnableReg		0x30

unsigned int parallelPort=GPIO_PORT1;

// Enable IO Port
void enableIO(int port)
{
  if(ioperm((unsigned long)port,(unsigned long)DATA_LENGTH,(int)ON)){
    perror("ioperm open fail!");
    exit(1);
  }
}

// Disable IO Port
void disableIO(int port)
{
  if(ioperm((unsigned long)port,(unsigned long)DATA_LENGTH,(int)OFF)){
    perror("ioperm close fail!");
    exit(2);
  }
}

// Let IT87 to MB PnP Mode
void it87Config(void)
{
  // Change it87 to MB PnP Mode
  outb(0x87,ADDRESS_PORT);
  outb(0x01,ADDRESS_PORT);
  outb(0x55,ADDRESS_PORT);

  if(ADDRESS_PORT==0x2e && DATA_PORT==0x2f){
    outb(0x55,ADDRESS_PORT); // 0x2e 0x2f
  }else{
    outb(0xaa,ADDRESS_PORT); // 0x4e 0x4f
  }
}

// Exit it87 MB PnP mode
void it87ConfigEnd(void)
{
  // Exit MB PnP Mode
  outb(0x02,ADDRESS_PORT);
  outb(0x02,DATA_PORT);
}

// Get It87 Chip ID
int getChipID(void)
{
  int id=0;

  outb(it87_chipID_Hi,ADDRESS_PORT);
  id|=(inb(DATA_PORT)<<8);

  outb(it87_chipID_Lo,ADDRESS_PORT);
  id+=inb(DATA_PORT);

  return(id);
}

// Catch System Signal
void catchSignal(int signalNum)
{
  disableIO(ADDRESS_PORT);
  disableIO(DATA_PORT);
  
  disableIO(GPIO_PORT0);
  disableIO(parallelPort);
  disableIO(LED_PORT);

  exit(0);
}

int main(int argc, char * argv[])
{
  int i=0,j,z;
  int lock_file=0;
  int LED_status;
  unsigned int chipID=0;
  char pid[10];

// Mutual Exclusion and Running a Single Copy
  if((lock_file=open(LOCK_FILE,O_RDWR|O_CREAT,0640))<0){
    printf("Can not open %s.\n",LOCK_FILE);
    exit(1); // Can not open
  }

  if(lockf(lock_file,F_TLOCK,0)<0){
    printf("Already running!\n");
    exit(0); // Can not lock
  }

  sprintf(pid,"%d\n",getpid());
  write(lock_file,pid,strlen(pid)); // record pid to lockfile

  enableIO(ADDRESS_PORT);
  enableIO(DATA_PORT);

// Initialize Config for 0x2e 0x2f
  it87Config();

// check chip id "0x8712"
  chipID=getChipID();

#ifdef DEBUG  
  printf("Chip ID=0x%x\n",chipID);
#endif

  if(chipID!=it87_CHIP_ID){
    perror("Chip ID not match!");
    exit(3);
  }

// Check Game port, LDN 9
  outb(it87_configLDN,ADDRESS_PORT);
  outb(gamePortLDN,DATA_PORT);

  outb(gamePortEnableReg,ADDRESS_PORT);
  outb(0x00,DATA_PORT);			// Disable Game Port then as GPIO set 2
  
// Initialise GPIO
  outb(it87_configLDN,ADDRESS_PORT);	// Set config LDN 
  outb(gpioLDN,DATA_PORT);		// set LDN 7

// read GPIO set 1 mode
  outb(gpioSet1SelReg,ADDRESS_PORT);
  i=inb(DATA_PORT); 			// Save before change

#ifdef DEBUG
  printf("index 0x%x=0x%x\n",gpioSet1SelReg,i);
#endif

// Set GPIO Set 1 mode Select Register 
// to set pin 30/31/32/33/34/84 as GPIO 10-15
  outb(gpioSet1SelReg,ADDRESS_PORT);
  outb(i|0x3f,DATA_PORT); 		// bit 0 - bit 5

// Read Block Local Lock register for save
  outb(LogicalBlockLockReg,ADDRESS_PORT);
  i=inb(DATA_PORT);			// Save before change

#ifdef DEBUG
  printf("Index 0x%x=0x%x\n",LogicalBlockLockReg,i);
#endif

// Set Block Local Lock bit 6 for GPIO  
  outb(LogicalBlockLockReg,ADDRESS_PORT);
  outb(LogicalBlockLockReg&0x3f,DATA_PORT); 	// set bit 6 to 0 for GPIO enable

// Enable Set1 as GPIO
  outb(gpioSet1EnableReg,ADDRESS_PORT);
  outb(0xff,DATA_PORT);			// enable GPIO Set1 GP10 - GP17

  outb(gpioSet2EnableReg,ADDRESS_PORT);
  outb(0x0f,DATA_PORT);			// enable GPIO Set 2 GP20/GP21/GP22/GP23
  
  
// GPIO Set 1 Output Enable
  outb(gpioSet1OutputEnableReg,ADDRESS_PORT);
  outb(0xff,DATA_PORT);			// enable GPIO Set1 output

// GPIO Set 2 Output Enable
  outb(gpioSet2OutputEnableReg,ADDRESS_PORT);
  outb(0x03,DATA_PORT);			// enable GPIO Set1 output
  
// GPIO Set1 Pins Polarity
  outb(gpioSet1PinPolarityReg,ADDRESS_PORT);
  outb(0x0,DATA_PORT);			// GPIO Set1 All in NonInverting state

// GPIO Set2 Pins Polarity
  outb(gpioSet2PinPolarityReg,ADDRESS_PORT);
  outb(0x0,DATA_PORT);			// GPIO Set2 All in NonInverting state
  
// GPIO Set1 Internal Pull-up
  outb(gpioSet1PinPullUpReg,ADDRESS_PORT);
  outb(0x0,DATA_PORT);		// GPIO Set1 All in Disable PullUp state

// GPIO Set2 Internal Pull-up
  outb(gpioSet2PinPullUpReg,ADDRESS_PORT);
  outb(0x0,DATA_PORT);		// GPIO Set2 All in Disable PullUp state
  
// Choice GPIO Set 1		// Pin 28/29/30/31/32/33/34/84 as GPIO
  outb(gpioSet1Idx,ADDRESS_PORT);
  outb(0xff,DATA_PORT);

// Choice GPIO Set 2		// GPIO 20-24
  outb(gpioSet2Idx,ADDRESS_PORT);
  outb(0x0f,DATA_PORT);
  
//Set GPIO MSB address  
  outb(gpioSimpleIOAddressMSB,ADDRESS_PORT);
  outb((GPIO_PORT0>>8),DATA_PORT);

//Set GPIO LSB address
  outb(gpioSimpleIOAddressLSB,ADDRESS_PORT);
  outb(GPIO_PORT0&0x00ff,DATA_PORT);
  
/**************************/

// Check LPT port, LDN 3
  outb(it87_configLDN,ADDRESS_PORT);
  outb(parallelLDN,DATA_PORT);

  outb(LogicalBlockLockReg,ADDRESS_PORT);
  i=inb(DATA_PORT);		// Save before change

#ifdef DEBUG
  printf("Index 0x%x=0x%x\n",LogicalBlockLockReg,i);
#endif

  outb(LogicalBlockLockReg,ADDRESS_PORT);
  outb(i&0x77,DATA_PORT);	// Set Bit 3 to 0 for Parallel programable

  outb(parallelEnableReg,ADDRESS_PORT);

  if(inb(DATA_PORT)){ 				// Parallel Port already enable
    outb(parallelAddressMSB,ADDRESS_PORT); 
    parallelPort=inb(DATA_PORT)<<8;		// read High byte

    outb(parallelAddressLSB,ADDRESS_PORT);
    parallelPort|=(inb(DATA_PORT)&0x00ff);	// read Low Byte
  }else{
    outb(parallelEnableReg,ADDRESS_PORT);	// If Parallel Port not enable, then enable port
    outb(0x01,DATA_PORT);

    outb(parallelAddressMSB,ADDRESS_PORT);
    outb(((parallelPort&0x0f00)>>8),DATA_PORT); // High byte 0-3 bit only

    outb(parallelAddressLSB,ADDRESS_PORT);
    outb((parallelPort&0x00fc),DATA_PORT);	// Low byte 2-7 bit only
  }

#ifdef DEBUG
  printf("parallelPort=0x%x\n",parallelPort);
#endif

  outb(parallelInterruptReg,ADDRESS_PORT);	// IRQ7	
  outb(0x07,DATA_PORT);

  outb(parallelModeReg,ADDRESS_PORT);	// ECP Mode
  outb(0x02,DATA_PORT); 

/*****************************/

// End MB PnP Config
  it87ConfigEnd();
//

  disableIO(ADDRESS_PORT);
  disableIO(DATA_PORT);

// redirect signal
  signal(SIGINT,catchSignal);	// SIGINT for CTRL-C
  //signal(SIGTSTP,catchSignal);	// SIGTSTP for CTRL-Z
  signal(SIGABRT,catchSignal);  // SIGABRT for ABORT signal

  // Initialize ports
  enableIO(GPIO_PORT0);
  //enableIO(parallelPort);
  enableIO(LED_PORT);
  
  outb(0x0,GPIO_PORT0);
  //outb(0x0,parallelPort);
  //outb(0x0,LED_PORT);
  
  // here we control status of LED
  if (argv[1] == NULL)
     LED_status = 9;
  else
     LED_status = atoi(argv[1]); 
  //outb(LED_RED , LED_PORT); two off
  //outb(LED_RED | LED_YELLOW, LED_PORT); yellow on
  //outb(LED_YELLOW , LED_PORT);
  //outb(0 , LED_PORT); red on 
  switch(LED_status){
    case 0:
      //Booting
      outb(0 , LED_PORT); 
      break;
    case 1:
      //upgrading
      while(1){
        outb(LED_YELLOW , LED_PORT); 
        usleep(DELAY*1000);
        outb(0,LED_PORT);
        usleep(DELAY*1000);
      }
      break;
    case 2:
      //normal
      outb(LED_RED , LED_PORT);
      break;
    case 3:
      //error
      while(1){
        outb(LED_YELLOW , LED_PORT);
        usleep(DELAY*1000);
        outb(LED_RED | LED_YELLOW ,LED_PORT);
        usleep(DELAY*1000);
      }
      break;
    case 4:
      //shutdown
      outb(LED_RED | LED_YELLOW ,LED_PORT);
      break;
    default:
      break;
  }


// Never go here
  catchSignal(0);

  exit(0);
}



