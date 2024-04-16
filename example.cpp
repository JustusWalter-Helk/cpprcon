#define E_DEBUG

#include "client.h"

#include <sstream>
#include <iomanip>

int main() {
	RconClient client("127.0.0.1", 25575);
	if (!client.connect()) {
		return 0;
	}

	char password[] = "changeme";

	client.login(password);

	client.sendCommand("test command", 1, [](Packet p) {
		std::cout << p.payload << std::endl;
		});

	while (true) {
		std::string cmd;
		std::getline(std::cin, cmd);

		client.sendCommand(cmd.c_str(), 2, [](Packet p) {
			std::cout << p.payload << std::endl;
			});
	}

	return 0;
}