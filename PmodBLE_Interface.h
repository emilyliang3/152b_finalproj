/*
 * PmodBLE_Interface.h
 *
 *  Created on: May 25, 2023
 *      Author: Eric
 */

#ifndef SRC_PMODBLE_INTERFACE_H_
#define SRC_PMODBLE_INTERFACE_H_


#include "PmodBLE.h"
#include "xparameters.h"
#include "sleep.h"
#include <string.h>
#include "xil_printf.h"

// Status Codes
#define PMODBLE_STATUS_ERR -1				// General Error
#define PMODBLE_STATUS_SUCCESS 6			// General Success
#define PMODBLE_STATUS_CMD_ENABLED 0		// CMD Mode is Enabled
#define PMODBLE_STATUS_CMD_DISABLED 1		// CMD Mode is Disabled (i.e. in command mode, but cannot execute commands)
#define PMODBLE_STATUS_CMD_EXITED 2			// Exited CMD Mode
#define PMODBLE_STATUS_CONNECTING 3			// Connecting to a BLE device
#define PMODBLE_STATUS_CONNECTED 4			// Connected to a BLE device
#define PMODBLE_STATUS_CONNECTION_ERR 5		// Connection error occurred
#define PMODBLE_STATUS_DISCONNECTED 7		// Disconnected from BLE device

// Enter Command Mode
#define ENTER_CMD_MODE_CMD "$$$"
#define ENTER_CMD_MODE_CMD_NUM_BYTES 3
#define ENTER_CMD_MODE_MAX_RESPONSE_BYTES 4
#define ENTER_CMD_MODE_ENABLED_RESPONSE "CMD>"	// What we want to see
#define ENTER_CMD_MODE_DISABLED_RESPONSE "CMD"

// Exit Command Mode
#define EXIT_CMD_MODE_CMD "---\r"
#define EXIT_CMD_MODE_CMD_NUM_BYTES 4
#define EXIT_CMD_MODE_MAX_RESPONSE_BYTES 3
#define EXIT_CMD_MODE_RESPONSE "END"

// Get Device Address
#define GET_DEVICE_ADDRESS_CMD "D\r"
#define GET_DEVICE_ADDRESS_CMD_NUM_BYTES 2
#define GET_DEVICE_ADDRESS_PREFIX "BTA="	// Line prefix to look for to get the device address.

// Connect to Device
#define CONN_TO_DEVICE_CMD "C,0,"
#define CONN_TO_DEVICE_CMD_NUM_BYTES 4
#define CONN_TO_DEVICE_MAX_RESPONSE_BYTES 10
#define CONN_TO_DEVICE_STARTING_RESPONSE "Trying"
#define CONN_TO_DEVICE_CONNECTED_RESPONSE "CONNECT"
#define CONN_TO_DEVICE_SYNTAX_ERROR_RESPONSE "ERR"
#define CONN_TO_DEVICE_CONNECT_ERROR_RESPONSE "ERR_CONN"

// Disconnect from Device
#define DISCONNECT_CMD "K,1\r"
#define DISCONNECT_CMD_NUM_BYTES 4
#define DISCONNECT_CMD_MAX_RESPONSE_BYTES 12
#define DISCONNECT_SUCCESS_RESPONSE "AOK"
#define DISCONNECT_STATUS_RESPONSE "%DISCONNECT%"
#define DISCONNECT_ERR_RESPONSE "ERR"

// Initializes the PmodBLE
void PmodBLE_Initialize();

// Get device address of PmodBLE device
int PmodBLE_GetDeviceAddress(u8 *address);

// Connect PmodBLE to another Bluetooth Device
int PmodBLE_ConnectTo(u8 *address);

// Disconnect PmodBLE from connected device.
void PmodBLE_Disconnect();

// Send a message to the other PmodBLE device.
void PmodBLE_SendMessage(u8 *msg);

// Receive a message from PmodBLE device.
int PmodBLE_ReceiveMessage(u8 *buf, int size);

// Checks if the PmodBLE device is connected to another device.
// Return:
//		1 if connected
//		0 if not connected
int PmodBLE_IsConnected();

// Flushes recieve buffers
void PmodBLE_Flush();


#endif /* SRC_PMODBLE_INTERFACE_H_ */
