// WinSockClient implementation
//

#include "WinSockClient.h"

#include <string.h>
#include <winsock2.h>
#include "../libs/readerwriterqueue/readerwriterqueue.h"
#include "../libs/readerwriterqueue/atomicops.h"

#pragma comment(lib, "ws2_32.lib")

#undef MODULE_NAME
#define MODULE_NAME "ClientSocket"

// NOTE: For debugging OutputDebugStringA could be used...
#define LogDebug(message, ...) //printf(MODULE_NAME "::" __FUNCTION__ " - " message "\n", __VA_ARGS__)

static void ProcessMessageQueue();
static bool SocketInit();
static void SocketCleanup();
static bool SocketSendMessage(const char ipString[], const char portString[], const char message[]);
static bool SetupSocket();
static bool CreateServerAddress(SOCKET clientSocket, const char ipString[], const char portString[], sockaddr_in& ServerAddress);
static int WaitForNewMesaageWithTimeout(FD_SET& readSet, long sec, long usec);

static bool _isRunning = false;
static bool _waitForEmptyBuffer;
static std::string _ipString;
static std::string _portString;
static std::thread _processMessagesThread;
static moodycamel::BlockingReaderWriterQueue<std::string> _messageBuffer;
static SOCKET clientSocket = INVALID_SOCKET;

constexpr int timeoutReceiveSec = 10;

/**
 * @brief Sends message to server
*/
void SocketSendMessage(const char message[])
{
	if (_isRunning) {
		_messageBuffer.enqueue(std::string(message));
	}
}

/**
 * @brief Starts the client
*/
void SocketStart(const char ipString[], const char portString[])
{
	_ipString = ipString;
	_portString = portString;

	LogDebug("Start message-processing thread");

	_isRunning = true;

	_processMessagesThread = std::thread(ProcessMessageQueue);
}

void ProcessMessageQueue()
{
	SocketInit();

	std::string item;
	while (true) {
		_messageBuffer.wait_dequeue(item);

		if (_isRunning == false) {
			break;
		}

		SocketSendMessage(_ipString.c_str(), _portString.c_str(), item.c_str());
	}

	if (_waitForEmptyBuffer) {
		// Empty the buffer before stopping.

		// One item should be still in item that was not processed yet.
		// If it was the dummy, do nothing.

		while (true) {

			if (_messageBuffer.size_approx() == 0) {
				break;
			}

			SocketSendMessage(_ipString.c_str(), _portString.c_str(), item.c_str());

			_messageBuffer.try_dequeue(item);
		}

	}

	SocketCleanup();
}

bool SocketInit()
{
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (NO_ERROR != nResult) {
		LogDebug("Error occurred while executing WSAStartup()");
		return false;
	}

	LogDebug("WSAStartup() successful");

	return true;
}

void SocketCleanup()
{
	if (clientSocket != INVALID_SOCKET) {
		closesocket(clientSocket);
	}

	// Cleanup Winsock
	WSACleanup();
}

bool SocketSendMessage(const char ipString[], const char portString[], const char message[])
{
	if (SetupSocket() == false) {
		closesocket(clientSocket);
	}

	sockaddr_in ServerAddress;
	if (CreateServerAddress(clientSocket, ipString, portString, ServerAddress) == false) {
		return false;
	}

	// Establish connection with the server
	if (SOCKET_ERROR == connect(clientSocket, reinterpret_cast<const struct sockaddr*>(&ServerAddress), sizeof(ServerAddress))) {
		auto error = WSAGetLastError();
		LogDebug("Error occurred while connecting: %ld.", error);
	}
	LogDebug("connect() successful.");

	// Send the message to the server
	int nBytesSent = send(clientSocket, message, (int)strlen(message), 0);

	if (SOCKET_ERROR == nBytesSent) {
		LogDebug("Error occurred while writing to socket.");
		return false;
	}
	LogDebug("send() successful.");

	FD_SET readSet;
	// NOTE: selRet has the cout of sockets that are ready
	auto selRet = WaitForNewMesaageWithTimeout(readSet, timeoutReceiveSec, 0);
	if (selRet == SOCKET_ERROR) {
		LogDebug("Error occurred while using select() on socket.");
		return false;
	} else if (selRet == 0) {
		LogDebug("Waiting for message timeout.");
		return false;
	}

	if (FD_ISSET(clientSocket, &readSet)) {
		// Server sent message to client

		// Init szBuffer with 0
		const int bufferSize = 256;
		char szBuffer[bufferSize];
		ZeroMemory(szBuffer, bufferSize);

		// Get the message from the server
		// WARNING: MSG_WAITALL only because in this case the server will close the connection, wich will also stop the waiting.
		int bytesRecv = recv(clientSocket, szBuffer, bufferSize - 1, MSG_WAITALL);

		if (SOCKET_ERROR == bytesRecv) {
			LogDebug("Error occurred while reading from socket.");
			return false;
		}
		LogDebug("recv() successful.");

		LogDebug("Server message: '%s'", szBuffer);
	}

	closesocket(clientSocket);

	return true;
}

/**
 * @brief Create a socket
 *
 * @return True if socket was sucessfully created.
*/
static bool SetupSocket()
{
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == clientSocket) {
		LogDebug("Error occurred while opening socket: %ld.", WSAGetLastError());
		return false;
	}
	LogDebug("socket() successful.");

	return true;
}

static bool CreateServerAddress(SOCKET clientSocket, const char ipString[], const char portString[], sockaddr_in& serverAddress)
{
	// Server name will be supplied as a commandline argument
	// Get the server details
	struct hostent* server = gethostbyname(ipString);

	if (server == NULL) {
		LogDebug("Error occurred no such host.");
		return false;
	}

	LogDebug("gethostbyname() successful.");

	// Port number will be supplied as a commandline argument
	int portNumber = atoi(portString);

	// Cleanup and Init the serverAddress with 0
	ZeroMemory((char*)&serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;

	// Assign the information received from gethostbyname()
	CopyMemory((char*)&serverAddress.sin_addr.s_addr, (char*)server->h_addr, server->h_length);

	serverAddress.sin_port = htons(portNumber); //comes from commandline

	return true;
}

// Wait with timeout for new message.
static int WaitForNewMesaageWithTimeout(FD_SET& readSet, long sec, long usec)
{
	FD_ZERO(&readSet);
	FD_SET(clientSocket, &readSet);
	timeval timeoutTime;
	timeoutTime.tv_sec = sec;
	timeoutTime.tv_usec = usec;

	// NOTE: selRet has the cout of sockets that are ready
	auto selRet = select(NULL, &readSet, NULL, NULL, &timeoutTime);

	return selRet;
}

/**
 * @brief Stops the client
*/
void SocketStop(bool waitForEmptyBuffer, bool waitForShutdown)
{
	_waitForEmptyBuffer = waitForEmptyBuffer;
	_isRunning = false;

	// Send dummy-message to get out of wait_dequeue()
	_messageBuffer.enqueue(std::string("end processing dummy-message. This should not be sent!"));

	if (waitForShutdown && _processMessagesThread.joinable()) {
		_processMessagesThread.join();
	}
}
