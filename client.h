#pragma once

#ifdef _WIN32
	#include <WinSock2.h>
	#include <WS2tcpip.h>
	#pragma comment(lib, "Ws2_32.lib")
#else
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

#define PACKET_TYPE_LOGIN 3
#define PACKET_TYPE_COMMAND 2
#define PACKET_TYPE_COMMAND_RESPONSE 0

#define MAX_S_C_PACKET_LENGTH 4110
#define MAX_C_S_PACKET_LENGTH 1460

#include <iostream>
#include <cstring>

#include <vector>

#include <thread>
#include <chrono>

#include <functional>
#include <mutex>
#include <condition_variable>

struct Packet {
	int32_t length;
	int32_t id;
	int32_t type;
	char* payload;
	uint8_t pad = '\O';

	Packet() : length(0), id(0), type(0), payload(nullptr) {}

	Packet(int32_t packetId, int32_t packetType, const char* data) {
		id = packetId;
		type = packetType;

		payload = new char[strlen(data) + 1];

		strcpy_s(payload, strlen(data) + 1, data);

		length = sizeof(packetId) + sizeof(type) + strlen(payload) + 1 + sizeof(pad);
	}
};

class RconClient {
private:
	int sock;
	struct sockaddr_in server;

	void sendPacket(Packet& packet) {
		int totalLength = sizeof(packet.length) + sizeof(packet.id) + sizeof(packet.type) + strlen(packet.payload) + 1 + sizeof(packet.pad);

		uint8_t* buffer = new uint8_t[totalLength];
		int pos = 0;
		memcpy(buffer + pos, &packet.length, sizeof(packet.length));
		pos += sizeof(packet.length);
		memcpy(buffer + pos, &packet.id, sizeof(packet.id));
		pos += sizeof(packet.id);
		memcpy(buffer + pos, &packet.type, sizeof(packet.type));
		pos += sizeof(packet.type);
		memcpy(buffer + pos, packet.payload, strlen(packet.payload) + 1);
		pos += strlen(packet.payload) + 1;
		memcpy(buffer + pos, &packet.pad, sizeof(packet.pad));

		SSIZE_T bytesSent = send(sock, reinterpret_cast<const char*>(buffer), totalLength, 0);
		if (bytesSent == -1) {
			std::cerr << "Error sending packet to server" << std::endl;
		}
		else {
			std::cout << "Sent " << bytesSent << " bytes to server" << std::endl;
		}

		delete[] buffer;
	}

	void waitForResponse(uint32_t id, const std::function<void(Packet)>& callback) {
		bool responseReceived = false;

		Packet response;

		while (!responseReceived) {
			if (receivePacket_sync(response) && response.id == id) {
				responseReceived = true;
				break;
			}
		}

		callback(response);
	}

	bool receivePacket_sync(Packet& packet) {

		char lengthBuffer[sizeof(int32_t)];

		SSIZE_T bytesReceived = recv(sock, lengthBuffer, sizeof(packet.length), 0);
		if (bytesReceived <= 0) {
			std::cout << "Error receiving packet length" << std::endl;
			delete lengthBuffer;
			return false;
		}

		int32_t packetLength;
		memcpy(&packetLength, lengthBuffer, sizeof(int32_t));

		packet.length = packetLength;

		if (packet.length >= MAX_S_C_PACKET_LENGTH - sizeof(int32_t)) {
			std::cout << "Package could be multi packet response!";
		}

		std::cout << "After calc package length is: " << packet.length << std::endl;
		std::cout << "Before calc package length is: " << lengthBuffer << std::endl;

		SSIZE_T remainingBytes = packet.length;
		char* buffer = new char[remainingBytes];
		bytesReceived = recv(sock, buffer, remainingBytes, 0);
		if (bytesReceived <= 0) {
			std::cout << "Error receiving payload or no payload attached" << std::endl;
			delete[] buffer;
			return false;
		}

		int pos = 0;

		memcpy(&packet.id, buffer + pos, sizeof(packet.id));

		pos += sizeof(packet.id);

		memcpy(&packet.type, buffer + pos, sizeof(packet.type));

		pos += sizeof(packet.type);

		int32_t payloadSize = remainingBytes - pos;

		std::cout << "Remaining Bytes: " << remainingBytes << std::endl;
		std::cout << "Pos: " << pos << std::endl;
		std::cout << "Payload size: " << payloadSize << std::endl;

		if (payloadSize > 0) {
			packet.payload = new char[payloadSize];

			memcpy(packet.payload, buffer + pos, payloadSize);
		}
		else {
			packet.payload = nullptr;
		}

		delete[] buffer;
	}

public:
	RconClient(const char* addr, int port) {
#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			std::cout << "Windows error lol" << std::endl;
			return;
		}
#endif

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == -1) {
			return;
		}

		server.sin_family = AF_INET;
		server.sin_port = htons(port);
		
		if (inet_pton(AF_INET, addr, &server.sin_addr) <= 0) {
			return;
		}
	}

	/**
	* Connects to the server given in constructor
	* 
	* @return True if connection successful
	*/
	bool connect() {
		if (::connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
			std::cerr << "Connection failed" << std::endl;
			return false;
		}
		std::cout << "Connected to server" << std::endl;
		return true;
	}

	/**
	* Trys to login with the given password
	* 
	* @param password password for rcon server defined in server.properties
	*/
	void login(char* password) {
		Packet packet(12, PACKET_TYPE_LOGIN, password);

		std::cout << "Login packet contains " << packet.length << " bytes" << std::endl;

		sendPacket(packet);
	}

	//Wait a sec? New thread each time a command is sent?
	void sendCommand(const char* cmd, uint32_t id,const std::function<void(Packet)>& callback) {
		Packet packet(id, PACKET_TYPE_COMMAND, cmd);

		sendPacket(packet);

		std::thread responseThread(&RconClient::waitForResponse, this, id, callback);
		responseThread.detach();
	}

	//TODO: Handle response thread and perform general cleanup of data
	void close() {
		// Close the socket
		::closesocket(sock);

		// Cleanup Winsock on Windows
#ifdef _WIN32
		WSACleanup();
#endif
	}
};