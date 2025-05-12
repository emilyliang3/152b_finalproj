#include "xil_printf.h"
#include "xuartlite.h"
#include "xparameters.h"
#include "PmodKYPD.h"
#include "sleep.h"
#include "xil_cache.h"
#include "PmodBT2.h"
#include <stdio.h>
#include "PmodOLEDrgb.h"
#include "bitmap.h"

// Required definitions for sending & receiving data over host board's UART port
#ifdef __MICROBLAZE__
#include "xuartlite.h"
typedef XUartLite SysUart;
#define SysUart_Send            XUartLite_Send
#define SysUart_Recv            XUartLite_Recv
#define SYS_UART_DEVICE_ID      XPAR_AXI_UARTLITE_0_DEVICE_ID
#define BT2_UART_AXI_CLOCK_FREQ XPAR_CPU_M_AXI_DP_FREQ_HZ
#else
#include "xuartps.h"
typedef XUartPs SysUart;
#define SysUart_Send            XUartPs_Send
#define SysUart_Recv            XUartPs_Recv
#define SYS_UART_DEVICE_ID      XPAR_PS7_UART_1_DEVICE_ID
#define BT2_UART_AXI_CLOCK_FREQ 100000000
#endif

/* ------------------------------------------------------------ */
/*               Global Variables and Defines                   */
/* ------------------------------------------------------------ */
#define DEFAULT_KEYTABLE "0FED789C456B123A"
PmodKYPD myKypd;
PmodOLEDrgb oledrgb;
PmodBT2 myBT2;
SysUart myUart;

u8 rgbUserFont[] = {
   0x00, 0x04, 0x02, 0x1F, 0x02, 0x04, 0x00, 0x00, // 0x00
   0x0E, 0x1F, 0x15, 0x1F, 0x17, 0x10, 0x1F, 0x0E, // 0x01
   0x00, 0x1F, 0x11, 0x00, 0x00, 0x11, 0x1F, 0x00, // 0x02
   0x00, 0x0A, 0x15, 0x11, 0x0A, 0x04, 0x00, 0x00, // 0x03
   0x07, 0x0C, 0xFA, 0x2F, 0x2F, 0xFA, 0x0C, 0x07  // 0x04
}; // This table defines 5 user characters, although only one is used

/* ------------------------------------------------------------ */
/*                         Keypad PMOD                          */
/* ------------------------------------------------------------ */
void KYPDInitialize() {
    KYPD_begin(&myKypd, XPAR_PMODKYPD_0_AXI_LITE_GPIO_BASEADDR);
    KYPD_loadKeyTable(&myKypd, (u8*) DEFAULT_KEYTABLE);
 }
 
 char KYPDGetKey() {
   u16 keystate;
   XStatus status, last_status = KYPD_NO_KEY;
   u8 key, last_key = 'x';
   // Initial value of last_key cannot be contained in loaded KEYTABLE string

   Xil_Out32(myKypd.GPIO_addr, 0xF);

   while (1) {
      // Capture state of each key
      keystate = KYPD_getKeyStates(&myKypd);

      // Determine which single key is pressed, if any
      status = KYPD_getKeyPressed(&myKypd, keystate, &key);

      // Print key detect if a new key is pressed or if status has changed
      if (status == KYPD_SINGLE_KEY
            && (status != last_status || key != last_key)) {
         last_key = key;
         xil_printf("Key pressed: %c\r\n", key);
         return last_key;
      } else if (status == KYPD_MULTI_KEY && status != last_status)
         xil_printf("Error: Multiple keys pressed\r\n");

      last_status = status;

      usleep(1000);
   }
}

/* ------------------------------------------------------------ */
/*                            BLE PMOD                          */
/* ------------------------------------------------------------ */
void BleInitialize()
{
   SysUartInit();
   BT2_Begin (
      &myBT2,
      XPAR_PMODBT2_0_AXI_LITE_GPIO_BASEADDR,
      XPAR_PMODBT2_0_AXI_LITE_UART_BASEADDR,
      BT2_UART_AXI_CLOCK_FREQ,
      115200
   );
}

void BleRun()
{
   u8 buf[1];
   int n;

   print("Initialized PmodBT2 Demo\n\r");
   print("Received data will be echoed here, type to send data\r\n");

   while (1) {
      // Echo all characters received from both BT2 and terminal to terminal
      // Forward all characters received from terminal to BT2
      n = SysUart_Recv(&myUart, buf, 1);
      if (n != 0) {
         SysUart_Send(&myUart, buf, 1);
         BT2_SendData(&myBT2, buf, 1);
      }

      n = BT2_RecvData(&myBT2, buf, 1);
      if (n != 0) {
         SysUart_Send(&myUart, buf, 1);
      }
   }
}

/* ------------------------------------------------------------ */
/*                         OLED PMOD                            */
/* ------------------------------------------------------------ */
void OledInitialize() {
   OLEDrgb_begin(&oledrgb, XPAR_PMODOLEDRGB_0_AXI_LITE_GPIO_BASEADDR,
      XPAR_PMODOLEDRGB_0_AXI_LITE_SPI_BASEADDR);
}

void OledRun() {
   // char ch;

   // // Define the user definable characters
   // for (ch = 0; ch < 5; ch++) {
   //    OLEDrgb_DefUserChar(&oledrgb, ch, &rgbUserFont[ch * 8]);
   // }

   OLEDrgb_Clear(&oledrgb);
  
   // Set color (white)
   u16 color = OLEDrgb_BuildRGB(255, 255, 255);

   // Vertical lines (x = 32, x = 64)
   OLEDrgb_DrawLine(&oledrgb, 32, 0, 32, 63, color);
   OLEDrgb_DrawLine(&oledrgb, 64, 0, 64, 63, color);

   // Horizontal lines (y = 21, y = 42)
   OLEDrgb_DrawLine(&oledrgb, 0, 21, 95, 21, color);
   OLEDrgb_DrawLine(&oledrgb, 0, 42, 95, 42, color);

}
 
/* ------------------------------------------------------------ */
/*                     Set up / Clean up                        */
/* ------------------------------------------------------------ */
 void Cleanup() {
    DisableCaches();
    OLED_End(&myOled);
 }

// Initialize the system UART device
void SysUartInit() {
   #ifdef __MICROBLAZE__
      // AXI Uartlite for MicroBlaze
      XUartLite_Initialize(&myUart, SYS_UART_DEVICE_ID);
   #else
      // Uartps for Zynq
      XUartPs_Config *myUartCfgPtr;
      myUartCfgPtr = XUartPs_LookupConfig(SYS_UART_DEVICE_ID);
      XUartPs_CfgInitialize(&myUart, myUartCfgPtr, myUartCfgPtr->BaseAddress);
   #endif
   }
 
 void EnableCaches() {
 #ifdef __MICROBLAZE__
 #ifdef XPAR_MICROBLAZE_USE_ICACHE
    Xil_ICacheEnable();
 #endif
 #ifdef XPAR_MICROBLAZE_USE_DCACHE
    Xil_DCacheEnable();
 #endif
 #endif
 }
 
 void DisableCaches() {
 #ifdef __MICROBLAZE__
 #ifdef XPAR_MICROBLAZE_USE_DCACHE
    Xil_DCacheDisable();
 #endif
 #ifdef XPAR_MICROBLAZE_USE_ICACHE
    Xil_ICacheDisable();
 #endif
 #endif
 }

/* ------------------------------------------------------------ */
/*               Auxiliary functions & Main                     */
/* ------------------------------------------------------------ */

// Draw X
void DrawX(PmodOLEDrgb* oled, int row, int col, u16 color) {
   int x0 = col * 32 + 6;
   int y0 = row * 21 + 4;
   int x1 = col * 32 + 26;
   int y1 = row * 21 + 17;

   // Diagonals
   OLEDrgb_DrawLine(oled, x0, y0, x1, y1, color);
   OLEDrgb_DrawLine(oled, x0, y1, x1, y0, color);
}

// Draw O
void DrawO(PmodOLEDrgb* oled, int row, int col, u16 color) {
   int cx = col * 32 + 16;
   int cy = row * 21 + 10;
   int r = 8;

   // If you have a circle draw function:
   OLEDrgb_DrawCircle(oled, cx, cy, r, color);
}


 // Update board
 void updateBoard() {
    // Display new board
    // Check for win condition
 }

int main() { 
    // Initialize all peripherals
    EnableCaches(); // pulled it out of pmod initializations so only runs once
    KYPDInitialize();
    OledInitialize();
    BleInitialize();
    OledRun();
    u16 red = OLEDrgb_BuildRGB(255, 0, 0);
    u16 blue = OLEDrgb_BuildRGB(0, 0, 255);

    // Place X at (0,0), O at (0,1)
    DrawX(oled, 0, 0, red);
    DrawO(oled, 0, 1, blue);

    while(1) {
         // OledRun();
        // Get move
        // Update board
        // Wait for other move
        // Update board
    }
    Cleanup();
    return 0;
}
