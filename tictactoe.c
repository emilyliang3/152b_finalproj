// Link to report: https://docs.google.com/document/d/13yfF9RYUmNzhypZq_BQRnF1Hq2sdnPr280tNd-wY5Ew/edit?usp=sharing

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
#define X_TILE 1
#define O_TILE 2
PmodKYPD myKypd;
PmodOLEDrgb oledrgb;
PmodBT2 myBT2;
SysUart myUart;
int board[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

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
void BleInitializeSlave()
{
   BT2_Begin (
      &myBT2,
      XPAR_PMODBT2_0_AXI_LITE_GPIO_BASEADDR,
      XPAR_PMODBT2_0_AXI_LITE_UART_BASEADDR,
      BT2_UART_AXI_CLOCK_FREQ,
      115200
   );
   BT2_EnterATMode(&myBT2);
   BT2_SendCommand(&myBT2, "AT+ROLE=0");
   u8 rxBuf[64]; 
   int n;
   
   BT2_SendCommand(&myBT2, "AT+ADDR?");
   n = BT2_RecvData(&myBT2, rxBuf, 64); 
   
   if (n > 0) {
       rxBuf[n] = '\0'; 
       OLEDrgb_Clear(&oled);
       OLEDrgb_SetCursor(&oled, 0, 0);
       OLEDrgb_PutString(&oled, (char*)rxBuf);
   }
   BT2_ExitATMode(&myBT2);
}

void BleInitializeMaster() {
   BT2_Begin (
      &myBT2,
      XPAR_PMODBT2_0_AXI_LITE_GPIO_BASEADDR,
      XPAR_PMODBT2_0_AXI_LITE_UART_BASEADDR,
      BT2_UART_AXI_CLOCK_FREQ,
      115200
   );  
   BT2_EnterATMode(&myBT2);
   BT2_SendCommand(&myBT2, "AT+ROLE=1");
   BT2_SendCommand(&myBT2, "AT+CMODE=0"); 
   BT2_SendCommand(&myBT2, "AT+BIND=1234,56,789ABC"); 
   BT2_ExitATMode(&myBT2);
}

void BleRun()
{
   u8 buf[1] = 8;
   int n;

   while (1) {
      BT2_SendData(&myBT2, buf, 1);
      n = BT2_RecvData(&myBT2, buf, 1);
      if (n != 0) {
         OLEDrgb_PutString(oled, "Received bluetooth");
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
 
/* ------------------------------------------------------------ */
/*                     Set up / Clean up                        */
/* ------------------------------------------------------------ */
 void Cleanup() {
    DisableCaches();
    OLED_End(&oledrgb);
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
void ResetGame() {
   BoardInit();
   for (int i = 0; i < 9; i++) {
      board[i] = 0;
   }
}

// Empty board
void BoardInit() {
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

// Draw X
void DrawX(PmodOLEDrgb* oled, int row, int col, u16 color) {
   int x0 = col * 32 + 6;
   int y0 = row * 21 + 4;
   int x1 = col * 32 + 26;
   int y1 = row * 21 + 17;

   // Diagonals
   OLEDrgb_DrawLine(oled, x0, y0, x1, y1, color);
   OLEDrgb_DrawLine(oled, x0, y1, x1, y0, color);

   board[row*3 + col] = X_TILE;
}

// Draw circle
void OLEDrgb_DrawCircle(PmodOLEDrgb* oled, int cx, int cy, int r, u16 color) {
   int x = 0;
   int y = r;
   int d = 1 - r;

   while (x <= y) {
       // 8-way symmetry
       OLEDrgb_DrawPixel(oled, cx + x, cy + y, color);
       OLEDrgb_DrawPixel(oled, cx - x, cy + y, color);
       OLEDrgb_DrawPixel(oled, cx + x, cy - y, color);
       OLEDrgb_DrawPixel(oled, cx - x, cy - y, color);
       OLEDrgb_DrawPixel(oled, cx + y, cy + x, color);
       OLEDrgb_DrawPixel(oled, cx - y, cy + x, color);
       OLEDrgb_DrawPixel(oled, cx + y, cy - x, color);
       OLEDrgb_DrawPixel(oled, cx - y, cy - x, color);

       if (d < 0) {
           d += 2 * x + 3;
       } else {
           d += 2 * (x - y) + 5;
           y--;
       }
       x++;
   }
}

// Draw O
void DrawO(PmodOLEDrgb* oled, int row, int col, u16 color) {
   int cx = col * 32 + 16;
   int cy = row * 21 + 10;
   int r = 8;

   OLEDrgb_DrawCircle(oled, cx, cy, r, color);

   board[row*3 + col] = O_TILE;
}

int checkWin(int board[9], int player) {
   // All 8 possible winning combinations (by index)
   int wins[8][3] = {
       {0, 1, 2},  // Row 1
       {3, 4, 5},  // Row 2
       {6, 7, 8},  // Row 3
       {0, 3, 6},  // Col 1
       {1, 4, 7},  // Col 2
       {2, 5, 8},  // Col 3
       {0, 4, 8},  // Diagonal TL–BR
       {2, 4, 6}   // Diagonal TR–BL
   };

   for (int i = 0; i < 8; i++) {
       if (board[wins[i][0]] == player &&
           board[wins[i][1]] == player &&
           board[wins[i][2]] == player) {
           return player;
       }
   }

   for (int i = 0; i < 9; i++) {
      if (board[i] == 0)
         return -1;
   }

   return 0;
}

void gameOver(PmodOLEDrgb* oled, int tile) {
   OLEDrgb_Clear(oled);

   char* winnerLine;
   if (tile == X_TILE)
      winnerLine = "   X won!\n";
   else if (tile == O_TILE)
      winnerLine = "   O won!\n";
   else
      winnerLine = "It's a tie!\n";

   // Compute centered column
   int col1 = (16 - 10) / 2;
   int col2 = (16 - 12) / 2;
   int col3 = (16 - 26) / 2;

   // Choose vertical rows
   OLEDrgb_SetCursor(oled, 0, 1);
   OLEDrgb_PutString(oled, " Game Over!\n");
   OLEDrgb_SetCursor(oled, 0, 2);
   OLEDrgb_PutString(oled, winnerLine);
   OLEDrgb_SetCursor(oled, 0, 4);
   OLEDrgb_PutString(oled, "Press any   non-numeric key to      continue.");

   // Get key
   KYPDGetKey();
   ResetGame();
}

 // Update board, returns next tile
 int updateBoard(int tile, int row, int col) {
   u16 red = OLEDrgb_BuildRGB(255, 0, 0);
   u16 blue = OLEDrgb_BuildRGB(0, 0, 255);
    // Display new board
   if(board[3*row+col] != 0){
	   return tile;
   }
    switch (tile) {
      case X_TILE:
         DrawX(&oledrgb, row, col, red);
         break;
      case O_TILE:
         DrawO(&oledrgb, row, col, blue);
         break;
      default:
         break;
    }
    // Check for win condition
    int winner = checkWin(board, tile);
    if (winner >= 0)
      gameOver(&oledrgb, winner);
    return turnChange(tile);
 }

 int turnChange(int currentTile){
	 if(currentTile == 1){
		 return 2;
	 }
	 return 1;
 }

int main() {
    // Initialize all peripherals
    EnableCaches(); // pulled it out of pmod initializations so only runs once
    KYPDInitialize();
    OledInitialize();
    BleInitializeSlave();
    BoardInit();

    int curTile = X_TILE;

    while(1) {
         // OledRun();
        // Get move
        char key = KYPDGetKey();
        int pos = key - '0';
        switch (pos) {
         case 1:
            curTile = updateBoard(curTile, 0, 0);
            break;
         case 2:
        	 curTile = updateBoard(curTile, 0, 1);
            break;
         case 3:
        	 curTile = updateBoard(curTile, 0, 2);
            break;
         case 4:
        	 curTile = updateBoard(curTile, 1, 0);
            break;
         case 5:
        	 curTile = updateBoard(curTile, 1, 1);
            break;
         case 6:
        	 curTile = updateBoard(curTile, 1, 2);
            break;
         case 7:
        	 curTile = updateBoard(curTile, 2, 0);
            break;
         case 8:
        	 curTile = updateBoard(curTile, 2, 1);
            break;
         case 9:
        	 curTile = updateBoard(curTile, 2, 2);
            break;
         default:
            break;
        }
        // Update board
        // Wait for other move
        // Update board
    }
    Cleanup();
    return 0;
}