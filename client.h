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

#ifdef E_DEBUG
	#define RCON_LOG(str) do {std::cout << "\033[34m[cpprcon] \033[39m"<< str << std::endl;} while(0);
#else
	#define RCON_LOG(str) do {} while(0);
#endif

#define RCON_ERROR(str) do {std::cout << "\033[31m[cpprcon] \033[39m"<< str << std::endl;} while(0);

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


/**
	 Basic class to manage the client->server connection and send/receive basic commands
	 Enable logs by defining E_DEBUG before including the header file
	**/
class RconClient {
private:
	int sock;
	struct sockaddr_in server;

	std::vector<std::thread*> threads = std::vector<std::thread*>();

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
		
		//No bytes to read on socket yet so we skip checking server availability when sending login packets
		if (packet.type == PACKET_TYPE_LOGIN) {
			SSIZE_T bytesSent = send(sock, reinterpret_cast<const char*>(buffer), totalLength, 0);
			if (bytesSent == -1) {
				RCON_ERROR("Error sending packet to server!");
				close();
			}
			else {
				RCON_LOG("Sent " << bytesSent << " bytes to server");
			}

			delete[] buffer;
			return;
		}

		SSIZE_T bytesSent = send(sock, reinterpret_cast<const char*>(buffer), totalLength, 0);
		if (bytesSent == -1) {
			RCON_ERROR("Error sending packet to server!");
			close();
		}
		else {
			RCON_LOG("Sent " << bytesSent << " bytes to server");
		}

		delete[] buffer;
	}

	void waitForResponse(uint32_t id, const std::function<void(Packet)>& callback) {
		bool responseReceived = false;

		Packet response;

		RCON_LOG("Waiting for response");

		while (!responseReceived) {
			if (receivePacket_sync(response) && response.id == id) {
				responseReceived = true;
				break;
			}
			
		}

		RCON_LOG("Response received, executing callback...");

		callback(response);
	}

	bool receivePacket_sync(Packet& packet) {

		char lengthBuffer[sizeof(int32_t)];

		SSIZE_T bytesReceived = recv(sock, lengthBuffer, sizeof(packet.length), 0);
		if (bytesReceived <= 0) {
			RCON_ERROR("Error receiving package length!");
			close();
			return false;
		}

		int32_t packetLength;
		memcpy(&packetLength, lengthBuffer, sizeof(int32_t));

		packet.length = packetLength;

		if (packet.length >= MAX_S_C_PACKET_LENGTH - sizeof(int32_t)) {
			RCON_LOG("Package could be multi packet response!");
		}

		SSIZE_T remainingBytes = packet.length;
		char* buffer = new char[remainingBytes];
		bytesReceived = recv(sock, buffer, remainingBytes, 0);
		if (bytesReceived <= 0) {
			RCON_ERROR("Error receiving payload or no payload attached");
			delete[] buffer;
			return false;
		}

		int pos = 0;

		memcpy(&packet.id, buffer + pos, sizeof(packet.id));

		pos += sizeof(packet.id);

		memcpy(&packet.type, buffer + pos, sizeof(packet.type));

		pos += sizeof(packet.type);

		int32_t payloadSize = remainingBytes - pos;

		if (payloadSize > 0) {
			packet.payload = new char[payloadSize];

			memcpy(packet.payload, buffer + pos, payloadSize);
		}
		else {
			packet.payload = nullptr;
		}

		delete[] buffer;

		return true;
	}

	void setup(const char* addr, int port) {
#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			RCON_ERROR("Windows error lol");
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

public:
	RconClient(const char* addr, int port) {
		setup(addr, port);
	}

	/**
	* Connects to the server given in constructor
	* 
	* @return True if connection successful
	*/
	bool connect() {
		if (::connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
			RCON_ERROR("Host could not be resolved!");
			return false;
		}
		RCON_LOG("Connected to Server");
		return true;
	}

	/**
	* Trys to login with the given password
	* 
	* @param password password for rcon server defined in server.properties
	*/
	void login(char* password) {
		Packet packet(12, PACKET_TYPE_LOGIN, password);

		RCON_LOG("Attempting login...");

		sendPacket(packet);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		Packet p;

		receivePacket_sync(p);

		if (p.id == 12) {
			RCON_LOG("Login successful!");
		}
		else {
			RCON_ERROR("Wrong password!");
		}
	}

	//Wait a sec? New thread each time a command is sent?
	/**
	* Sends a command type package to the server with the command as payload
	* 
	* @param cmd command to send as payload
	* @param id some kind of unique identifier for that excact command, should`t not change when sending the same command again
	* @param callback function to call after receiving a response from the server
	**/
	void sendCommand(const char* cmd, uint32_t id,const std::function<void(Packet)>& callback) {
		Packet packet(id, PACKET_TYPE_COMMAND, cmd);

		std::thread responseThread(&RconClient::waitForResponse, this, id, callback);
		threads.push_back(&responseThread);
		responseThread.detach();

		sendPacket(packet);
	}


	//TODO: Handle response thread and perform general cleanup of data
	void close() {
		for (std::thread* t : threads) {
			t->join();
		}

		// Close the socket
		::closesocket(sock);

		// Cleanup Winsock on Windows
#ifdef _WIN32
		WSACleanup();
#endif
	}
};