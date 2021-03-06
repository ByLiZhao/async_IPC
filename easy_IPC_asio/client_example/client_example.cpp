// timer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "../easy_IPC_asio/tcp_client.h"

int main()
{
	using namespace::easy_IPC;
	boost::asio::io_context io_context;
	tcp_client tcp_client(io_context, "localhost", "8000");
	while (!tcp_client.is_connected()) {
		std::this_thread::sleep_for(std::chrono::nanoseconds(50));
	}
	std::cout << "connected" << std::endl;
	std::cout << "server will echo, press enter to show echoed message" << std::endl;
	for (;;) {

		bool is_connected;

		std::string str;
		bool loop = true;
		while (loop) {
			std::cout << "Client said: ";
			std::getline(std::cin, str);
			if (str.size() != 0) {
				tcp_client.send(str, is_connected);
				loop = false;
			}
			else {
				loop = true;
			}
		}
		std::string r_string;
		do {
			tcp_client.receive(r_string, is_connected);
			if (r_string.size() != 0)
				std::cout << "Server said: " << r_string << std::endl;
		} while (r_string.size() == 0);

		if (is_connected == false) {
			while (!tcp_client.is_connected()) {
				std::cout << "server is off line, will automatically try in 1 second" << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
				tcp_client.re_connect();
			}
		}

	}
	char a;
	std::cin >> a;
	return 0;
}
