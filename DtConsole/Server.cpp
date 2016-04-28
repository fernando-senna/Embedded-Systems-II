/*
 *	Server.cpp
 *	Authors: 	Fernando Do Nascimento
 *			 	Isaac Ramirez
 *	Class: 	 	CSE 4342 - Embedded Systems II
 *	Lab:	 	Design of a Real-Data Acquisition System with Network Communications
 *	Due Date:	May 5, 2016	
 */
 
 #include "Libraries.h"
 #include "Server.h"
 #include "Variables.h"
 #include "DT9816.h"
 
 //Function Prototype
 //Function Prototypes
int set_server_socket(void);
void set_dt9816(void);
double minimum(double values[]);
double maximum(double values[]);
double average(double values[], int len);
double variance(double values[]);
void activate_led(int number);
void convolution(double arrayA[], double arrayB[], int lenA, int lenB, int arrayC[]);

//Error Handling Macros
#define SHOW_ERROR(ecode) MessageBox(HWND_DESKTOP,olDaGetErrorString(ecode,\
                  str,STRLEN),"Error", MB_ICONEXCLAMATION | MB_OK);

#define CHECKERROR(ecode) if ((board.status = (ecode)) != OLNOERROR)\
                  {\
                  SHOW_ERROR(board.status);\
                  olDaReleaseDASS(board.hdass);\
                  olDaTerminate(board.hdrvr);\
                  return ((UINT)NULL);}

#define CHECKERROR( ecode )                           \
do                                                    \
{                                                     \
   ECODE olStatus;                                    \
   if( OLSUCCESS != ( olStatus = ( ecode ) ) )        \
   {                                                  \
      printf("OpenLayers Error %d\n", olStatus );  \
      exit(1);                                        \
   }                                                  \
}                                                     \
while(0)  

BOOL CALLBACK
GetDriver(LPSTR lpszName, LPSTR lpszEntry, LPARAM lParam)
/*
*	This is a callback function of olDaEnumBoards, it gets the strings of the
*	Open Layers board and attemps to initialize the board. If successful,
*	enumeration is halted.
*/
{
	LPBOARD lpboard = (LPBOARD)(LPVOID)lParam;

	//Fill in Board Strings
	lstrcpyn(lpboard->name, lpszName, MAX_BOARD_NAME_LENGTH - 1);
	lstrcpyn(lpboard->entry, lpszEntry, MAX_BOARD_NAME_LENGTH - 1);

	//Try to Open Board
	lpboard->status = olDaInitialize(lpszName, &lpboard->hdrvr);

	if (lpboard->hdrvr != NULL)
		return FALSE;	//False to Stop Enumerating

	else
		return TRUE;	//True to Continue Enumerating
}

/*
 *	Data Acquisition
 */
LRESULT WINAPI
WndProc(HWND hWnd, UINT msg, WPARAM hAD, LPARAM lParam)
{
	int i, j;

	switch (msg)
	{
		case OLDA_WM_BUFFER_DONE:
			HBUF hBuf;
			printf("Buffer Done\n");
			CHECKERROR(olDaGetBuffer((HDASS)hAD, &hBuf));

			if(hBuf && setUpDt9816 == true && startSampling == true)
			{ 
				printf("Buffer Done 2\n");
				olDaGetRange((HDASS)hAD, &max, &min);
				olDaGetEncoding((HDASS)hAD, &encoding);
				olDaGetResolution((HDASS)hAD, &resolution);
				olDmGetValidSamples(hBuf, &sample);

				if (resolution > 16)
				{
					printf("Buffer Done 3\n");
					olDmGetBufferPtr(hBuf, (LPVOID *)&pBuffer32);
					value = pBuffer32[sample - 1];
				}

				else
				{
					printf("Buffer Done 4\n");
					olDmGetBufferPtr(hBuf, (LPVOID *)&pBuffer);
					value = pBuffer[sample - 1];

					cout << valSW << " " << value << endl;
					if (value >= 49000)
					{
						activate_led(1);
						sprintf(inputChar, "Real Time Data Acquisition Started!");
				//		send(comm->datasock, inputChar, sizeof(inputChar), 0);

						cout << "Sample = " << sample << endl;
						i = j = 0;
						while (i < sample)
						{
							value = pBuffer[i];

							volt = ((float)max - (float)min) / (1L << resolution) * value + float(min);

							cout << "Volt = " << volt << endl;

							buffer[j] = volt;

							i += 2;
							j++;
						}

						//convolution();
					}
				}
			}
			/*
			printf("Buffer Done Count : %ld \r", counter);
			HBUF hBuf;
			counter++;
			olDaGetBuffer((HDASS)hAD, &hBuf);
			olDaPutBuffer((HDASS)hAD, hBuf);*/
			break;

		case OLDA_WM_QUEUE_DONE:
			printf("\nReal Time Data Acquisition Stopped!!");
			activate_led(0);
			PostQuitMessage(0);
			break;

		case OLDA_WM_TRIGGER_ERROR:
			printf("\nTrigger Error: Acquisition Stopped.");
			PostQuitMessage(0);
			break;

		case OLDA_WM_OVERRUN_ERROR:
			printf("\nInput Overrun Error: Acquisition Stopped.");
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, msg, hAD, lParam);
	}

	return 0;
}

/*
 *	Initialize DT9816 Board
 */
BOOL CALLBACK
EnumBrdProc(LPSTR lpszBrdName, LPSTR lpszDriverName, LPARAM lParam)
{

	// Make sure we can Init Board
	if (OLSUCCESS != (olDaInitialize(lpszBrdName, (LPHDEV)lParam)))
	{
		return TRUE;  // try again
	}

	// Make sure Board has an A/D Subsystem 
	UINT uiCap = 0;
	olDaGetDevCaps(*((LPHDEV)lParam), OLDC_ADELEMENTS, &uiCap);
	if (uiCap < 1)
	{
		return TRUE;  // try again
	}

	printf("%s succesfully initialized.\n", lpszBrdName);
	return FALSE;    // all set , board handle in lParam
}

int main(void)
{
	int serverSocket = 0;
	
	serverSocket = set_server_socket;
	
	hClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_iface_thread, (LPVOID)&profiler, 0, &dwClientThreadID);
	SetThreadPriority(hClientThread, THREAD_PRIORITY_LOWEST);
	
	set_dt9816();
	
	while(setUpDt9816 == false && startSampling == false)
		;
		
	//Set the DT9816 with the specified sample rate
	// create a window for messages
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = "DtConsoleClass";
	RegisterClass(&wc);

	HWND hWnd = CreateWindow(wc.lpszClassName,
		NULL,
		NULL,
		0, 0, 0, 0,
		NULL,
		NULL,
		NULL,
		NULL);

	if (!hWnd)
		exit(1);

	HDEV hDev = NULL;
	CHECKERROR(olDaEnumBoards(EnumBrdProc, (LPARAM)&hDev));

	HDASS hAD = NULL;
	CHECKERROR(olDaGetDASS(hDev, OLSS_AD, 0, &hAD));

	CHECKERROR(olDaSetWndHandle(hAD, hWnd, 0));

	CHECKERROR(olDaSetDataFlow(hAD, OL_DF_CONTINUOUS));	//Continuos Data
	CHECKERROR(olDaSetChannelListSize(hAD, 2));				//2 Analog Channels Used
	CHECKERROR(olDaSetChannelListEntry(hAD, 0, 0));		//Channel 0 Entry
	CHECKERROR(olDaSetChannelListEntry(hAD, 1, 1));			//Channel 1 Entry
	CHECKERROR(olDaSetGainListEntry(hAD, 0, 1));		//Channel 0 Gain
	CHECKERROR(olDaSetGainListEntry(hAD, 1, 1));			//Channel 1 Gain
	CHECKERROR(olDaSetTrigger(hAD, OL_TRG_SOFT));
	CHECKERROR(olDaSetClockSource(hAD, OL_CLK_INTERNAL));
	CHECKERROR(olDaSetClockFrequency(hAD, sampleRate));	//Sample Rate Set by Client
	CHECKERROR(olDaSetWrapMode(hAD, OL_WRP_NONE));
	CHECKERROR(olDaConfig(hAD));

	HBUF hBufs[NUM_OL_BUFFERS];
	for (int i = 0; i < NUM_OL_BUFFERS; i++)
	{
		if (OLSUCCESS != olDmAllocBuffer(GHND, sampleRate * 2, &hBufs[i]))
		{
			for (i--; i >= 0; i--)
			{
				olDmFreeBuffer(hBufs[i]);
			}
			exit(1);
		}
		olDaPutBuffer(hAD, hBufs[i]);
	}

	if (OLSUCCESS != (olDaStart(hAD)))
	{
		printf("A/D Operation Start Failed...hit any key to terminate.\n");
	}

	else
	{
		printf("A/D Operation Started...hit any key to terminate.\n\n");
		printf("Buffer Done Count : %ld \r", counter);
	}

	MSG msg;
	SetMessageQueue(50);                      // Increase the our message queue size so
											  // we don't lose any data acq messages

											  // Acquire and dispatch messages until a key is hit...since we are a console app 
											  // we are using a mix of Windows messages for data acquistion and console approaches
											  // for keyboard input.
											  //
	while (GetMessage(&msg,        // message structure              
		hWnd,        // handle of window receiving the message 
		0,           // lowest message to examine          
		0))        // highest message to examine         
	{
		TranslateMessage(&msg);    // Translates virtual key codes       
		DispatchMessage(&msg);     // Dispatches message to window 
		if (_kbhit())
		{
			_getch();
			PostQuitMessage(0);
		}
	}

	while (1);
	
	return 0;
}

/************************************************************************************
* VOID client_iface_thread(LPVOID)
*
* Description: Thread communicating commands from client and the status of their
*				completion back to client.
*
*
************************************************************************************/
VOID client_iface_thread(LPVOID parameters) //LPVOID parameters)
{
	sp_struct_t profiler = (sp_struct_t)parameters;
	sp_comm_t comm = &profiler->comm;
	INT retval;
	struct sockaddr_in saddr;
	int saddr_len;
	char ParamBuffer[110];

	//Variables
	double temp;
	char start[6];

	printf("Server Program\n");
	printf("--------------\n");

	while (ParamBuffer[0] != '!')
	{
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len = sizeof(saddr);

		//Receives the Sample Rate & Filter Coefficients
		if (serverCounter == 0)
		{
			recv(comm->cmdrecvsock, (char *)&sampleRate, sizeof(sampleRate), 0);

			for (int index = 0; index < 101; index++)
			{
				recv(comm->cmdrecvsock, (char *)&temp, sizeof(temp), 0);
				filterCoefficient[index] = temp;
			}

			setUpDt9816 = true;

			serverCounter = 1;
		}

		//Waiting to receive the start signal
		if (serverCounter == 1)
		{
			recv(comm->cmdrecvsock, (char *)&start, sizeof(start), 0);

			if (strcmp(start, "Start") == 0)
			{
				sprintf(inputChar, "Sampling Process Started! Waiting for the user to press the switch!");
				
				send(comm->datasock, inputChar, sizeof(inputChar), 0);

				startSampling = true;
			}
		}

		//Send Data Back to the Client
		if (clientCounter == 0)
		{
			while (done == true)
			{
				if (_kbhit())
				{
					//Send Buffer When Data Acquisition Stops
					if (stopSampling == true)
					{

					}

					else
					{
						scanf("%s", inputChar);
						send(comm->datasock, inputChar, sizeof(inputChar), 0);

					}
				}
			}

		}
	}

	done = false;
}

/*
 *	Function: set_server_socket
 *	Parameter(s): void
 *	Returns: an integer value if there was an error while setting up the server socket.
 *	Description: This function sets up the server socket, so the program can send & receive data from & to the client.
 */
int set_server_socket(void)
{
	int res = 0;

	//Server
	memset(&profiler, 0, sizeof(profiler));
	sp_comm_t comm = &profiler.comm;

	if ((res = WSAStartup(0x202, &wsaData)) != 0)
	{
		fprintf(stderr, "WSAStartup failed with error %d\n", res);
		WSACleanup();

		return ERR_CODE_SWI;
	}

	/*
	*	Setup Data Transmition Socket to Broadcast Data
	*/
	hp = (struct hostent*)malloc(sizeof(struct hostent));
	hp->h_name = (char*)malloc(sizeof(char) * 17);
	hp->h_addr_list = (char**)malloc(sizeof(char*) * 2);
	hp->h_addr_list[0] = (char*)malloc(sizeof(char) * 5);

	strcpy(hp->h_name, "lab_example\0");

	hp->h_addrtype = 2;
	hp->h_length = 4;

	//Broadcast in 255.255.255.255 Network
	hp->h_addr_list[0][0] = (signed char)255;	//192;129
	hp->h_addr_list[0][1] = (signed char)255;	//168;107
	hp->h_addr_list[0][2] = (signed char)255;	//0;255  
	hp->h_addr_list[0][3] = (signed char)255;	//140;255
	hp->h_addr_list[0][4] = 0;

	/*
	*	Setup a Socket for Broadcasting Data
	*/
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = hp->h_addrtype;

	memcpy(&(saddr.sin_addr), hp->h_addr, hp->h_length);
	saddr.sin_port = htons(1500);

	if ((comm->datasock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		fprintf(stderr, "socket(datasock) failed: %d\n", WSAGetLastError());
		WSACleanup();

		return ERR_CODE_NONE;
	}

	if (connect(comm->datasock, (struct sockaddr*)&saddr, sizeof(saddr)) == SOCKET_ERROR)
	{
		fprintf(stderr, "connect(datasock) failed: %d\n", WSAGetLastError());
		WSACleanup();

		return ERR_CODE_SWI;
	}

	/*
	*	Setup and Bind a Socket to Listen for Commands from Client
	*/
	memset(&saddr, 0, sizeof(struct sockaddr_in));

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(1024);

	if ((comm->cmdrecvsock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		fprintf(stderr, "socket(cmdrecvsock) failed: %d\n", WSAGetLastError());
		WSACleanup();

		return ERR_CODE_NONE;
	}

	if (bind(comm->cmdrecvsock, (struct sockaddr*)&saddr, sizeof(saddr)) == SOCKET_ERROR)
	{
		fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
		WSACleanup();

		return ERR_CODE_NONE;
	}
}

/*
 *	Function: set_dt9816
 *	Parameter(s): void
 *	Returns: void
 *	Description: After the server receives from the client the sampleFrequency and the filter coeffcient,
 *	this functions sets up the DT9816 in order for the program to be able to begin the data acquisition.
 */
void set_dt9816(void)
{
	//Setup DT9816
	board.hdrvr = NULL;

	CHECKERROR(olDaEnumBoards(GetDriver, (LPARAM)(LPBOARD)&board));
	CHECKERROR(board.status);

	if (board.hdrvr == NULL)
	{
		MessageBox(HWND_DESKTOP, "No Open Layer Boards!!!", "Error",
			MB_ICONEXCLAMATION | MB_OK);
	}

	CHECKERROR(olDaGetDASS(board.hdrvr, OLSS_DOUT, 0, &board.hdass));
	CHECKERROR(olDaSetDataFlow(board.hdass, OL_DF_SINGLEVALUE));
	CHECKERROR(olDaConfig(board.hdass));
	CHECKERROR(olDaGetResolution(board.hdass, &resolution));

	value = 0;

	CHECKERROR(olDaPutSingleValue(board.hdass, value, channel, gain));
	CHECKERROR(olDaReleaseDASS(board.hdass));
	CHECKERROR(olDaTerminate(board.hdrvr));

	setUpDt9816 = true;
}

/*
 *	Function: average
 *	Parameter(s): values - an array of type double that stores the results collected from the DT9816
 *	Returns: The average result of the data collected.
 *	Description: The function takes the double array with the data collected, then sums all of the
 *	values, and finally calculates and returns the average.
 */
double average(double values[], int len)
{
	int index;
	double sum;

	sum = 0.0;

	//Adds all of the values of the array
	for (index = 100; index < len - 100; index++)
		sum = sum + values[index];

	//Calculates & Returns the average
	return sum / len;
}

/*
 *	Function: minimum
 *	Parameter(s): values - an array of type double that stores the results collected from the DT9816 
 *	Returns: The minimum value of the data collected.
 *	Description: The function takes the the first value of the array and sets it to be its minimum,
 *	then the function loops thru the entire array and comparing the value with the arbitrary minimum value.
 *	If the value compared is less than the arbitrary value, then that number replaces the arbitrary value.
 *	Once the loop finishes, the function returns the minimum value of the array.
 */
double minimum(double values[])
{
	int len, index;
	double min;		

	len = 1099;
	index = 200;
	min = values[200];	//Sets arbitrary minimum value

	//Loops thru the array
	while (index < (len - 200))
	{
		//Checks if the value in the array is less than the arbitrary value
		if (values[index] < min)
			min = values[index];

		index++;
	}

	return min;
}

/*
 *	Function: maximum
 *	Parameter(s): values - an array of type double that stores the results collected from the DT9816
 *	Returns: The maximum value from the array
 *	Description: The function sets an arbitrary maximum value, loops thru the entire array comparing values
 *	looking for the largest number of the array and updates the max variable.
 */
double maximum(double values[])
{
	int len, index;
	double max;

	len = 1099;
	max = values[200];	//Sets arbitrary maximum value

	//Loops thru the entire array looking for the max value & updating the max variable
	//if the value compared is grater than the arbitrary max
	for (index = 200; index < len - 200; index++)
		if (values[index] > max)
			max = values[index];

	return max;
}

/*
 *	Function: variance
 *	Parameter(s): values - an array of type double that stores the results collected from the DT9816
 *	Returns: The variance of the data collected from the DT9816
 *	Description: The function first calculates the average of the data collected, then for
 *	each number we subtract the average and square the result, and finally we calculate the
 *	average again to calculate the variance.
 *	Source: http://www.mathsisfun.com/data/standard-deviation.html
 */
double variance(double values[])
{
	int len, index;
	double firstAvg, sum;

	len = 1099;
	firstAvg = sum = 0.0;

	firstAvg = average(values, len);

	for (index = 200; index < len - 200; index++)
		sum = sum + pow((values[index] - firstAvg), 2);

	//Calculates & Returns the Variance (Average)
	return sum / len;
}

/*
 *	Function:
 *	Parameter(s):
 *	Returns:
 *	Description:
 */
void activate_led(int number)
{
	//Get first available Open Layer Board
	board.hdrvr = NULL;
	CHECKERROR(olDaEnumBoards(GetDriver, (LPARAM)(LPBOARD)&board));

	//Check for error within callback function
	CHECKERROR(board.status);

	if (board.hdrvr == NULL)
	{
		MessageBox(HWND_DESKTOP, "No Open Layer Boards!!!",
			"Error", MB_ICONEXCLAMATION | MB_OK);

		exit(0);
	}

	//Get hanle to DOUT subsystem
	CHECKERROR(olDaGetDASS(board.hdrvr, OLSS_DOUT, 0, &board.hdass));

	//Set subsystem for single value operations
	CHECKERROR(olDaSetDataFlow(board.hdass,OL_DF_SINGLEVALUE));
	CHECKERROR(olDaConfig(board.hdass));

	//Put all 0's single value
	CHECKERROR(olDaGetResolution(board.hdass, &resolution));

	if(number == 1)
		number = (1L << resolution) - 1;
	
	CHECKERROR(olDaPutSingleValue(board.hdass, number, channel, gain));

	CHECKERROR(olDaReleaseDASS(board.hdass));
	CHECKERROR(olDaTerminate(board.hdrvr));
}

/*
 *	Function:
 *	Parameter(s):
 *	Returns:
 *	Description:
 *	Source: http://toto-share.com/2011/11/cc-convolution-source-code/
 */
void convolution(double arrayA[], double arrayB[], int lenA, int lenB, double arrayC[])
{
	int i, j, k, newConv;
	double sum, *result;

	//Allocates space for the convolution array
	newConv = lenA + lenB - 1;
	result = (double *)calloc(newConv, sizeof(double));
	
	//Convolution process
	for (i = 0; i < newConv; i++)
	{
		k = i;
		sum = 0;

		for (j = 0; j < lenB; j++)
		{
			if (k >= 0 && k < lenA)
				sum = sum + (arrayA[k] * arrayB[j]);

			k = k - 1;
			arrayC[i] = sum;
		}
	}

//	*lenC = newConv;
}