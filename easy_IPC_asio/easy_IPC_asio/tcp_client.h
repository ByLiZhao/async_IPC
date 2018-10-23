#include "pch.h"
#include "tcp_connection.h"
#ifndef _EASY_IPC_TCP_CLIENT
#define _EASY_IPC_TCP_CLIENT

namespace easy_IPC {

	class tcp_client {
	public:
		tcp_client(boost::asio::io_context& io_context, std::string host_name, std::string port_number)
			:host_name_(host_name),
			port_number_(port_number),
			resolver_(io_context)
		{
			init(io_context);
		}

		tcp_client(const tcp_client&) = delete; //disable copy constructor
		tcp_client& operator= (const tcp_client&) = delete;//disable copy assignment constructor
		tcp_client(tcp_client&&) = default;
		tcp_client& operator= (tcp_client&&) = default;

		~tcp_client() {
			shutdown();
		}

		bool is_connected() {
			return single_connection_->is_connected();
		}

		void send(const std::string& message, bool& connected) {//takes reference version
			single_connection_->send_message(message);
			connected = single_connection_->is_connected();
		}

		void receive(std::string& message, bool& connected) {
			//std::cout << "receive in client start" << std::endl;
			message = single_connection_->get_message();
			connected = single_connection_->is_connected();
		}

		void end_talk() {
			if (!(single_connection_->is_connected())) {
				single_connection_->send_message(std::string());
			}
			single_connection_->close_self();
		}

		void re_connect() {
			resolve(host_name_, port_number_);
		}

		void shutdown() {
			single_connection_->set_is_connected(false);
			if (single_connection_ != nullptr) {
				single_connection_->close_self();
			}
			single_connection_ = nullptr;
			work_ = nullptr;
			thread_.join();
		}

	private:
		void init(boost::asio::io_context& io_context) {
			work_ = std::make_shared<boost::asio::io_context::work>(io_context);
			single_connection_ = tcp_connection::create(io_context);

			thread_ = std::thread(
				[&]() {
				io_context.run();
			}
			);

			resolve(host_name_, port_number_);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		void resolve(std::string host_name, std::string port_number) {
			tcp::resolver::query resolver_query_(host_name, port_number);
			resolver_.async_resolve(resolver_query_,
				[this](const boost::system::error_code& error,
					tcp::resolver::results_type results) {
				this->resolve_handler(error, results);
			}
			);
		}

		void resolve_handler(
			const boost::system::error_code& error,
			tcp::resolver::results_type results)
		{
			if (!error)
			{
				boost::asio::async_connect(
					single_connection_->socket(),
					results,
					[this](const boost::system::error_code& error,
						const tcp::endpoint& endpoint) {
					this->connect_handler(error, endpoint);
				}
				);
			}
		}

		// ...

		void connect_handler(const boost::system::error_code& error,
			const tcp::endpoint& endpoint)
		{
			if (!error) {
				single_connection_->set_is_connected(true);
				single_connection_->start_read();
				//single_connection_->send_current_time();
			}
		}

		

		std::string host_name_;
		std::string port_number_;
		tcp::resolver resolver_;
		//tcp::resolver::query resolver_query_;
		tcp_connection::pointer single_connection_;
		std::shared_ptr<boost::asio::io_context::work> work_;
		std::thread thread_;
	};

}

#endif