#include "client.h"

#include <sstream>
#include <iomanip>

int main() {
	RconClient client("127.0.0.1", 25575);
	if (client.connect()) {
		std::cout << "connected!" << std::endl;
	}
	else {
		return 0;
	}

	char password[] = "changeme";

	client.login(password);

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	char cmd[] = "list";

	client.sendCommand(cmd, 1, [](Packet p) {
		std::cout << p.payload << std::endl;
		});

	while (true) {
		std::string cmd;
		std::cin >> cmd;

		client.sendCommand(cmd.c_str(), 2, [](Packet p) {
			std::cout << p.payload << std::endl;
			});
	}

	client.close();
	
	return 0;
}