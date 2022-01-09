/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 2020 - 2020 WhatsappTray Sebastian Amann */

// Implementation for the WinSock server.

#include "stdafx.h"
#include "WinSockServer.h"

#include <string.h>
//#include <winsock2.h>
#include <atomic>
#include "Logger.h"

#pragma comment(lib, "ws2_32.lib")

#undef MODULE_NAME
#define MODULE_NAME "ServerSocket"

#define LogDebug(message, ...) //Logger::Info(MODULE_NAME "::" __FUNCTION__ " - " message "\n", __VA_ARGS__)

static bool SocketDoProcessing(char portString[]);
static bool SetupSocket(char portString[]);
static bool ProcessClientSocket(SOCKET remoteSocket, std::string& messageFromClient);
static int WaitForNewMesaageWithTimeout(SOCKET clientSocket, FD_SET& readSet, long sec, long usec);

static std::atomic<bool> isRunning = false;
static SOCKET listenSocket = INVALID_SOCKET;
static std::function<void(std::string)> socketMessageReceivedEvent = NULL;

/**
 * @brief Initialize socket
 *
 * @param portString The port the socket is running on
*/
bool SocketStart(char portString[])
{
	isRunning = true;

	// Initialize Winsock
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (NO_ERROR != nResult) {
		LogDebug("Error occurred while executing WSAStartup().");
		return false;
	}

	LogDebug("WSAStartup() successful.");

	SocketDoProcessing(portString);

	// Cleanup Winsock
	WSACleanup();
	return true;
}

static bool SocketDoProcessing(char portString[])
{
	bool success = true;

	if (SetupSocket(portString) == false) {
		return false;
	}

	if (SOCKET_ERROR == listen(listenSocket, SOMAXCONN)) {
		closesocket(listenSocket);

		LogDebug("Error occurred while listening.");
		return false;
	}

	LogDebug("Listen to port successful.");

	struct sockaddr_in clientAddress{};
	int clientSize = sizeof(clientAddress);
	while (true) {

		LogDebug("Wait for a client to connect.");
		SOCKET clientSocket = accept(listenSocket, (struct sockaddr*)&clientAddress, &clientSize);

		if (INVALID_SOCKET == clientSocket) {
			if (isRunning == false) {
				LogDebug("Listening on socket for new connection stopped.");
				break;
			}

			auto errorNo = WSAGetLastError();
			LogDebug("Error occurred while accepting socket: %ld.", errorNo);

			success = false;
			break;
		}
		else {
			LogDebug("Accept() successful.");
		}

		LogDebug("Client connected from: %s", inet_ntoa(clientAddress.sin_addr));

		std::string messageFromClient;
		auto clientProcessingSuccessful = ProcessClientSocket(clientSocket, messageFromClient);
	}

	if (INVALID_SOCKET != listenSocket) {
		closesocket(listenSocket);
	}
	return success;
}

void SocketNotifyOnNewMessage(const std::function<void(std::string)>& messageReceivedEvent)
{
	socketMessageReceivedEvent = messageReceivedEvent;
}

void SocketStopServer()
{
	isRunning = false;
	// NOTE: closesocket() can be calld on invalid sockets, so there is no handling for that necessary
	closesocket(listenSocket);
	listenSocket = INVALID_SOCKET;
}

/**
 * @brief Create a socket
*/
static bool SetupSocket(char portString[])
{
	SOCKET listenSocketTemp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == listenSocketTemp) {
		LogDebug("Error occurred while opening socket: %ld.", WSAGetLastError());
		return false;
	}

	LogDebug("socket() successful.");

	struct sockaddr_in ServerAddress{};
	// Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char*)&ServerAddress, sizeof(ServerAddress));

	// Port number will be supplied as a commandline argument
	int nPortNo = atoi(portString);

	// Fill up the address structure
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = INADDR_ANY; // WinSock will supply address
	ServerAddress.sin_port = htons(nPortNo);    //comes from commandline

	// Assign local address and port number
	if (SOCKET_ERROR == bind(listenSocketTemp, (struct sockaddr*)&ServerAddress, sizeof(ServerAddress))) {
		closesocket(listenSocketTemp);

		LogDebug("Error occurred while binding.");
		return false;
	}
	else {
		LogDebug("bind() successful.");
	}

	listenSocket = listenSocketTemp;

	return true;
}

static bool ProcessClientSocket(SOCKET clientSocket, std::string& messageFromClient)
{
	FD_SET readSet;

	auto selRet = WaitForNewMesaageWithTimeout(clientSocket, readSet, 10, 0);
	if (selRet == SOCKET_ERROR) {
		LogDebug("Error occurred while using select() on socket.");
		return false;
	}
	else if (selRet == 0) {
		LogDebug("Waiting for message timeout.");
		return false;
	}

	if (FD_ISSET(clientSocket, &readSet)) {
		// Init messageBuffer with 0
		const int messageBufferSize = 2000;
		char messageBuffer[messageBufferSize];
		ZeroMemory(messageBuffer, messageBufferSize);

		// Receive data from a connected or bound socket
		int nBytesRecv = recv(clientSocket, messageBuffer, messageBufferSize - 1, 0);

		if (SOCKET_ERROR == nBytesRecv) {
			LogDebug("Error occurred while receiving from socket.");

			closesocket(clientSocket);
			return false;
		}
		LogDebug("recv() successful.");

		messageFromClient = std::string(messageBuffer);

		if (socketMessageReceivedEvent) {
			socketMessageReceivedEvent(messageFromClient);
		}
	}

	// Send data to connected client
	std::string messageToClient = "Message received successfully";
	int nBytesSent = send(clientSocket, messageToClient.c_str(), (int)messageToClient.length(), 0);

	if (SOCKET_ERROR == nBytesSent) {
		LogDebug("Error occurred while writing message to Client to socket.");

		closesocket(clientSocket);
		return false;
	}
	LogDebug("Send message to Client successful.");

	closesocket(clientSocket);

	return true;
}

/**
 * @brief Wait with timeout for new message.
 *
 * @return The cout of sockets that have data or SOCKET_ERROR
*/
static int WaitForNewMesaageWithTimeout(SOCKET clientSocket, FD_SET& readSet, long sec, long usec)
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
