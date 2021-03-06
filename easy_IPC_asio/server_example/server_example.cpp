// timer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "../easy_IPC_asio/tcp_server.h"

int main()
{
	using namespace::easy_IPC;
	boost::asio::io_context io_context;
	tcp_server tcp_server(io_context, 8000);
	while (!tcp_server.is_connected()) {
		std::this_thread::sleep_for(std::chrono::nanoseconds(5000));
	}
	std::cout << "connected" << std::endl;
	std::cout << "start echo" << std::endl;
	for (;;) {

		bool is_connected;

		std::string r_string;

		tcp_server.receive(r_string, is_connected);
		if (r_string.size() != 0) {
			std::cout << "Client said: " << r_string << std::endl;

			tcp_server.send(r_string, is_connected);
		}
		if (is_connected == false) {
			std::cout << "client is off line, wait for re-connection" << std::endl;
			//tcp_server.restart();
			while (!tcp_server.is_connected()) {
				tcp_server.end_talk();
				std::this_thread::sleep_for(std::chrono::nanoseconds(5000));
			}
		}

	}
	char a;
	std::cin >> a;
	return 0;
}
