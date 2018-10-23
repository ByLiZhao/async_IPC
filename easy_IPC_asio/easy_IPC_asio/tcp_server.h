#include "pch.h"
#include "tcp_connection.h"
#ifndef _EASY_IPC_TCP_SERVER
#define _EASY_IPC_TCP_SERVER

namespace easy_IPC {

	class tcp_server
	{//for single connection only
	public:
		tcp_server(boost::asio::io_context& io_context, unsigned short port_number)
			:port_number_(port_number),
			acceptor_(io_context, tcp::endpoint(tcp::v4(), port_number))
		{
			init(io_context);
		}

		tcp_server(const tcp_server&) = delete; //disable copy constructor
		tcp_server& operator= (const tcp_server&) = delete;//disable copy assignment constructor
		tcp_server(tcp_server&&) = default;
		tcp_server& operator= (tcp_server&&) = default;

		~tcp_server() {
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
			message = single_connection_->get_message();
			connected = single_connection_->is_connected();
		}

		void end_talk() {
			if (!(single_connection_->is_connected())) {
				single_connection_->send_message(std::string());
			}
			single_connection_->close_self();
			accept();
		}
		
		void shutdown() {
			//if shutdown is not called explicitly, the underlying connection will be destroyed 
			//when destructor of server instance is called.
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

			accept();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		void accept() {
			single_connection_->strand().post(
				[&, this]() {
				acceptor_.async_accept(
					single_connection_->socket(),
					[this](const boost::system::error_code& error) {
					handle_accept(single_connection_, error);
				}
				);
			}
			);
		}


		void handle_accept(tcp_connection::pointer connection_,
			const boost::system::error_code& error)
		{
			if (!error)
			{
				connection_->set_is_connected(true);
				//connection_->send_current_time();
				connection_->start_read();
			}
		}

		unsigned short port_number_;
		tcp::acceptor acceptor_;
		tcp_connection::pointer single_connection_;
		std::shared_ptr<boost::asio::io_context::work> work_;
		std::thread thread_;
	};

}

#endif