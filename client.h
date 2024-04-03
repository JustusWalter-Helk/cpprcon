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