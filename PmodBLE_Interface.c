/*
 * PmodBLE_Interface.c
 *
 *  Created on: May 25, 2023
 *      Author: Eric
 */

#include "PmodBLE_Interface.h"

// *********** PmodBLE Variables *********** //
static PmodBLE bleDevice;

// *********** Static Functions (should be utility functions) *********** //
static void PmodBLE_Read(u8 *buf, int num_bytes);
static void PmodBLE_ReadUntilEOL(u8 *buf, char EOL);
static int PmodBLE_EnterCommandMode();
static int PmodBLE_ExitCommandMode();
static int PmodBLE_SendCommand(u8 *command);
static int PmodBLE_SendCommandRead(u8 *command, u8 *response, int response_bytes);
static int PmodBLE_SendCommandReadEOL(u8 *command, u8 *response, u8 EOL);

/*
 * Flushes the PmodBLE receiver buffers.
 */
void PmodBLE_Flush()
{
	// DEBUG
	xil_printf("PBLE_F: Flushed receive buffers\r\n");

	u8 flush_buf[128] = {0};
	BLE_RecvData(&bleDevice, flush_buf, 128);
}

/*
 * Reads a specific number of bytes from PmodBLE.
 * Input:
 *		buf - Buffer to store bytes in; must be initialized to 0.
 *		nBytes - Number of bytes to read.
 */
static void PmodBLE_Read(u8 *buf, int num_bytes)
{
	int n = 0;
	int bytes_read = 0;
	int idx = 0;
	u8 recv_byte = 0;

	// While bytes_read < num_bytes requested.
	while (bytes_read < num_bytes)
	{
		n = BLE_RecvData(&bleDevice, &recv_byte, 1);

		if (n != 0)
		{
			// DEBUG
			xil_printf("PBLE_R: Read %c (decimal %d)\r\n", recv_byte, recv_byte);

			buf[idx] = recv_byte;
			idx++;
			bytes_read++;
		}
	}
}

/*
 * Reads until the specified EOL character appears.
 *
 * NOTE: The returned string DOES NOT include the EOL.
 * WARNING: Could get stuck in an infinite loop if characters do not show up.
 *
 * Input:
 * 		buf - Buffer to store bytes in; must be initialized to 0; should be properly sized.
 */
static void PmodBLE_ReadUntilEOL(u8 *buf, char EOL)
{
	int n = 0;
	int idx = 0;	// Increment upon success.
	u8 recv_byte = 0;

	while (recv_byte != EOL)
	{
		n = BLE_RecvData(&bleDevice, &recv_byte, 1);

		if (n != 0 && recv_byte != EOL)
		{
			// DEBUG
			xil_printf("PBLE_RUE: Read %c (decimal %d)\r\n", recv_byte, recv_byte);
			buf[idx] = recv_byte;
			idx++;
		}
	}
}

/*
 * Enters Command Mode
 *
 * Output:
 * 		PMODBLE_STATUS_SUCCESS - Was able to get into command mode!
 * 		PMODBLE_STATUS_ERR - Was not able to get into command mode...
 */
static int PmodBLE_EnterCommandMode()
{
	// Response Buffer
	u8 response[ENTER_CMD_MODE_MAX_RESPONSE_BYTES + 1] = {0};

	// Put device to sleep for 100 ms (100,000 us)
	usleep(100000);

	// Flush receiver buffers
	PmodBLE_Flush();

	// Send the command mode characters
	BLE_SendData(&bleDevice, ENTER_CMD_MODE_CMD, ENTER_CMD_MODE_CMD_NUM_BYTES);

	// Receive the response
	PmodBLE_Read(response, 4);

	// Check output
	// ISSUE: Apparently when the device enters command mode, the
	//        characters it sends to state that it is in command mode
	//		  (i.e. "CMD>" or "CMD") does not end with a CR.
	// SOLUTION: Look for "CMD>" and "CMD" as a substring.
	if (strstr(response, ENTER_CMD_MODE_ENABLED_RESPONSE) != NULL)
	{
		xil_printf("PBLE_ECM: CMD Enabled\r\n");
		return PMODBLE_STATUS_SUCCESS;
	}
	else
	{
		xil_printf("PBLE_ECM: ERR\r\n");
		return PMODBLE_STATUS_ERR;
	}
}

/*
 * Exits Command Mode
 *
 * Output:
 * 		PMODBLE_STATUS_SUCCESS - Was able to exit command mode!
 * 		PMODBLE_STATUS_ERR - Was not able to exit command mode...
 */
static int PmodBLE_ExitCommandMode()
{
	u8 response[EXIT_CMD_MODE_MAX_RESPONSE_BYTES + 1] = {0};

	// Flush receiver buffers
	PmodBLE_Flush();

	// Send the characters to exit command mode
	BLE_SendData(&bleDevice, EXIT_CMD_MODE_CMD, EXIT_CMD_MODE_CMD_NUM_BYTES);

	// Receive the response
	PmodBLE_ReadUntilEOL(response, '\r');

	// Check output
	if (strcmp(response, EXIT_CMD_MODE_RESPONSE) == 0)
	{
		xil_printf("PBLE_ExCM: Exited command mode\r\n");
		return PMODBLE_STATUS_CMD_EXITED;
	}
	else
	{
		return PMODBLE_STATUS_ERR;
	}
}

/*
 * Sends a command to PmodBLE; must handle entering and exiting of command mode on your own; good for multiple commands.
 *
 * Input:
 * 		command - Buffer with command to send; make sure it ends with a '\r' in order for it to be executed; still end with '\0'
 * 				  That is, command is a C-String as follows: "<COMMAND>\r\0"
 */
static int PmodBLE_SendCommand(u8 *command)
{
	int status = 0;		// Keeps track of process status.

	// DEBUG
	xil_printf("PBLE_SC: Sending %s\r\n", command);

	// Send the command.
	int idx = 0;			// Increment upon success
	int n = 0;
	u8 byte_to_send = 0;
	while(command[idx] != '\0')
	{
		byte_to_send = command[idx];
		n = BLE_SendData(&bleDevice, &byte_to_send, 1);

		if (n == 1)
		{
			// DEBUG
			xil_printf("PMOD_SC: Sent %c (decimal %d)\r\n");

			idx++;
		}
	}

	// Return success.
	return PMODBLE_STATUS_SUCCESS;
}

/*
 * Sends a command to PmodBLE, retrieves a response, and exits command mode; good for one-time commands.
 *
 * Input:
 * 		command - Buffer with command to send; make sure it ends with a '\r' in order for it to be executed; still end with '\0'
 * 				  That is, command is a C-String as follows: "<COMMAND>\r\0"
 * 		response - Buffer to store response in; must be initialized to 0; should be sized accordingly.
 * 		response_bytes - Number of bytes to get from response.
 */
static int PmodBLE_SendCommandRead(u8 *command, u8 *response, int response_bytes)
{
	int status = 0;		// Keeps track of process status.

	// DEBUG
	xil_printf("PBLE_SCR: Sending %s\r\n", command);

	// 1. Enter command mode to send the command.
	status = PmodBLE_EnterCommandMode();
	if (status == PMODBLE_STATUS_ERR)
	{
		xil_printf("PBLE_SCR: Error sending command\r\n");
		return PMODBLE_STATUS_ERR;
	}

	// 2. Flush the receiver buffers.
	PmodBLE_Flush();

	// 3. Send the command.
	PmodBLE_SendCommand(command);

	// 4. Retrieve the response.
	PmodBLE_Read(response, response_bytes);

	// 5. Exit command mode.
	status = PmodBLE_ExitCommandMode();
	if (status == PMODBLE_STATUS_ERR)
	{
		xil_printf("PBLE_SCR: Error exiting command mode\r\n");
		return PMODBLE_STATUS_ERR;
	}

	// 6. Return success.
	return PMODBLE_STATUS_SUCCESS;

}

/*
 * Sends a command to PmodBLE, retrieves a response that ends with EOL, and exits command mode; good for one-time
 * commands.
 *
 * Input:
 * 		command - Buffer with command to send; make sure it ends with a '\r' in order for it to be executed; still end with '\0'
 * 				  That is, command is a C-String as follows: "<COMMAND>\r\0"
 * 		response - Buffer to store response in; must be initialized to 0; should be sized accordingly.
 * 		EOL - EOL character to look for in response.
 */
static int PmodBLE_SendCommandReadEOL(u8 *command, u8 *response, u8 EOL)
{
	int status = 0;		// Keeps track of process status.

	// DEBUG
	xil_printf("PBLE_SCRE: Sending %s\r\n", command);

	// 1. Enter command mode to send the command.
	status = PmodBLE_EnterCommandMode();
	if (status == PMODBLE_STATUS_ERR)
	{
		xil_printf("PBLE_SCRE: Error sending command\r\n");
		return PMODBLE_STATUS_ERR;
	}

	// 2. Flush the receiver buffers.
	PmodBLE_Flush();

	// 3. Send the command.
	PmodBLE_SendCommand(command);

	// 4. Retrieve the response.
	PmodBLE_ReadUntilEOL(response, EOL);

	// 5. Exit command mode.
	status = PmodBLE_ExitCommandMode();
	if (status == PMODBLE_STATUS_ERR)
	{
		xil_printf("PBLE_SCRE: Error exiting command mode\r\n");
		return PMODBLE_STATUS_ERR;
	}

	// 6. Return success.
	return PMODBLE_STATUS_SUCCESS;

}

/*
 * Initializes the PmodBLE device.
 */
void PmodBLE_Initialize()
{
	BLE_Begin(
	        &bleDevice,
	        XPAR_PMODBLE_0_S_AXI_GPIO_BASEADDR,
	        XPAR_PMODBLE_0_S_AXI_UART_BASEADDR,
			XPAR_CPU_M_AXI_DP_FREQ_HZ,
	        115200
	);

	// Set the device into advertisement mode.
	u8 response[3] = {0}; 	// Response is "AOK"
	u8 cmd[] = "A\r";
	PmodBLE_SendCommandRead(cmd, response, 3);
	xil_printf("PBLE_Init: Advertisement Mode -> %s\r\n", response);
}

/*
 *	Gets the device address.
 *
 *	Input:
 *		address - Buffer of 12-bytes to store the device address.
 */
int PmodBLE_GetDeviceAddress(u8 *address)
{
	u8 response[16] = {0};

	// 1. Send command to get device address.
	PmodBLE_SendCommandRead(GET_DEVICE_ADDRESS_CMD, response, 16);

	// DEBUG
	xil_printf("PBLE_GDA: %s\r\n", response);

	// 2. Copy address portion (i.e. char after "BTA=") into address.
	int idx = 4;
	while (idx < 16)
	{
		address[idx - 4] = response[idx];
		idx++;
	}

	// 3. Return success.
	return PMODBLE_STATUS_SUCCESS;
}

/*
 * Attempt connection to BLE device.
 */
int PmodBLE_ConnectTo(u8 *address)
{
	// NOTE: It appears that the STATUS messages from PmodBLE ends with a LF character; that is a '\n'.
	//       We can write a new read function that will continue reading until a certain EOL character
	//		 is received.

	// NOTE: For the ConnectTo function, we may need to be a bit more involved in the sending/recieving of commands.

	int status = 0;		// Use for status messages.
	u8 response[32] = {0};


	// 1. Concatenate address to connection command:
	//		Base CMD: 4 bytes
	//		Address: 12 bytes
	//		CR: 1 byte
	//		NULL: 1 byte
	//		Need 18 bytes for command.
	u8 cmd[18] = CONN_TO_DEVICE_CMD;
	strcat(cmd, address);
	strcat(cmd, "\r");

	// 2. Enter command mode.
	status = PmodBLE_EnterCommandMode();
	if (status != PMODBLE_STATUS_SUCCESS)
	{
		return PMODBLE_STATUS_ERR;
	}

	// 3. Send the command; make sure to flush buffers beforehand.
	PmodBLE_Flush();
	PmodBLE_SendCommand(cmd);

	// 4. Retrieve the response.
	PmodBLE_ReadUntilEOL(response, '\n');

	// 5. Loop until we get %CONNECT%, ERR, or %ERR_CONN%.
	if (strstr(response, CONN_TO_DEVICE_STARTING_RESPONSE) != NULL)
	{
		memset(response, '\0', sizeof(response));	// Clear response.
		PmodBLE_Read(response, CONN_TO_DEVICE_MAX_RESPONSE_BYTES);
	}
	else
	{
		return PMODBLE_STATUS_ERR;
	}

	// 6. Once we get a new status message, return status.
	//    It appears that upon successful connection, the device leaves
	//    command mode automatically. Thus, no need to do a exit command mode thing.
	if (strstr(response, CONN_TO_DEVICE_CONNECTED_RESPONSE) != NULL)			// %CONNECT%
	{
		return PMODBLE_STATUS_CONNECTED;
	}
	else if (strstr(response, CONN_TO_DEVICE_SYNTAX_ERROR_RESPONSE) != NULL)	// ERR
	{
		// Error occurred, so probably will have to do an exit command mode.
		xil_printf("PBLE_CT: Syntax Error\r\n");
		PmodBLE_ExitCommandMode();
		return PMODBLE_STATUS_ERR;
	}
	else if (strstr(response, CONN_TO_DEVICE_CONNECT_ERROR_RESPONSE) != NULL)	// %ERR_CONN%
	{
		// Error occurred, so probabily will have to do an exit command mode.
		xil_printf("PBLE_CT: Connection Error\r\n");
		PmodBLE_ExitCommandMode();
		return PMODBLE_STATUS_CONNECTION_ERR;
	}
	else
	{
		// Error occurred, so probabily will have to do an exit command mode.
		xil_printf("PBLE_CT: Other Error\r\n");
		PmodBLE_ExitCommandMode();
		return PMODBLE_STATUS_ERR;
	}
}

void PmodBLE_Disconnect()
{
	int status = 0;
	u8 response[4] = {0};

	xil_printf("PBLE_D: Executing Disconnect\r\n");

	// 1. Send the Disconnect Command.
	PmodBLE_SendCommandRead(DISCONNECT_CMD, response, 3);

	// 2. Check response type.
	if (strstr(response, DISCONNECT_SUCCESS_RESPONSE) != NULL)
	{
		xil_printf("PBLE_D: Disconnected\r\n");
		return PMODBLE_STATUS_SUCCESS;
	}
	else
	{
		xil_printf("PBLE_D: ERROR\r\n");
		return PMODBLE_STATUS_ERR;
	}
}

void PmodBLE_SendMessage(u8 *msg)
{
	int msg_size = strlen(msg);
	int bytes_sent = 0; // Increment upon success.
	int idx = 0;		// Increment upon success.
	int n = 0;			// Number of bytes sent.
	u8 byte_to_send = 0;

	while(bytes_sent != msg_size)
	{
		byte_to_send = msg[idx];
		n = BLE_SendData(&bleDevice, &byte_to_send, 1);
		if (n != 0)
		{
			bytes_sent++;
			idx++;
		}
	}

	xil_printf("PBLE_SM: Sent message\r\n");
}

int PmodBLE_ReceiveMessage(u8 *buf, int size)
{
	return BLE_RecvData(&bleDevice, buf, size);
}

int PmodBLE_IsConnected()
{
	return BLE_IsConnected(&bleDevice);
}
