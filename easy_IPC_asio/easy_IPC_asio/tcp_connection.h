#include "pch.h"
#ifndef _EASY_IPC_TCP_CONNECTION
#define _EASY_IPC_TCP_CONNECTION

namespace easy_IPC {

	using boost::asio::ip::tcp;

	std::string make_daytime_string()
	{
		using namespace std; // For time_t, time and ctime;
		time_t now = time(0);
		std::array<char, 128> buffer;
		ctime_s(buffer.data(), buffer.size(), &now);	//ctime_s is safer than ctime.
		return std::string(buffer.data());
	}

	std::string convert_to_char_array(uint16_t n) {
		//This is not a to_string method
		typedef uint8_t uint16_as_char_array[2];
		uint16_as_char_array char_array;
		//make it work for both little endian and big endian machines, 
		//so don't do this by memcpy
		char_array[0] = uint8_t((n & uint16_t(0xFF00)) >> 8);
		char_array[1] = uint8_t(n & uint16_t(0x00FF));
		return std::string((char *)&char_array, 2);
	}

	uint16_t convert_to_uint16_t(const std::string string_) {
		typedef  uint8_t uint16_as_char_array[2];
		uint16_as_char_array char_array;
		char_array[0] = string_[0];
		char_array[1] = string_[1];
		return uint16_t(((uint16_t)char_array[0] << 8) | ((uint16_t)char_array[1]));
	}

	class tcp_connection : public std::enable_shared_from_this<tcp_connection>
	{
	public:
		typedef std::shared_ptr<tcp_connection> pointer;

		static pointer create(boost::asio::io_context& io_context)
		{
			return pointer(new tcp_connection(io_context));
		}

		tcp::socket& socket() {//getter for socket_
			return socket_;
		}

		boost::asio::io_context::strand& strand() {//getter for strand
			return strand_;
		}

		void close_self() {
			auto self(shared_from_this());
			strand_.post(
				[this, self]() {
				boost::system::error_code error;
				socket_.shutdown(tcp::socket::shutdown_type::shutdown_both, error);
				if (socket().is_open() == true) {
					socket_.close(error);
				}
			}
			);
		}

		std::string get_message() {//get one message, run from the caller thread
			std::lock_guard<std::mutex> my_lock_guard(this->in_box_mutex_);
			if (!in_box_.empty()) {
				std::string msg = in_box_.front();
				in_box_.pop();
				return msg;
			}
			else {
				return std::string();//return an empty string
			}
		}

		void send_message(std::string str) {//let strand so the synchronization
			auto self(shared_from_this());
			std::string my_string;
			if (str.size() > limit_)//2^16-1
			{//truncate if the string is too large
				my_string = convert_to_char_array(limit_) + std::string(str.c_str(), limit_);
			}
			else {
				my_string = convert_to_char_array(static_cast<uint16_t>(str.size())) + str;
			}
			strand_.post(
				[this, self, my_string]() {
				strand_send(my_string);
			}
			);
		}

		void start_read() {
			auto self(shared_from_this());//make sure this object outlives handller
			strand_.post(
				[this, self]() {
				read_length();
			}
			);
		}

		void set_is_connected(bool yes_or_no) {
			is_connected_ = yes_or_no;
		}

		bool is_connected() {
			return is_connected_;
		}

		void send_current_time() {
			std::string current_time = make_daytime_string();
			send_message(current_time);
		}

	private:

		tcp_connection(boost::asio::io_context& io_context)	//private constructor
			: socket_(io_context),
			strand_(io_context),
			is_writing_(false),
			is_connected_(false)
		{

		}

		void strand_send(std::string str) {
			out_box_.push(str);
			if (is_writing_ == false) {//not writing now, start writing
				write();
			}
		}

		void write() {
			auto self(shared_from_this());
			is_writing_ = true;
			std::string str = out_box_.front();
			boost::asio::async_write(
				socket_,
				boost::asio::buffer(str.c_str(), str.size()),
				[this, self](const boost::system::error_code& error,
					size_t byte_transferred) {
				if (!error) {
					after_sent();
				}
				else {
					handle_error(error, byte_transferred);
				}
			}
			);
		}

		void after_sent() {
			out_box_.pop();
			if (out_box_.empty() == false) {//if out_box_ non-empty, keep writing
				write();
				return;
			}
			else
				is_writing_ = false;//nothing left, stop sent.
		}

		void read_length()
		{
			auto self(shared_from_this());//make sure this object outlives handller
			boost::asio::async_read(
				socket_,
				boost::asio::buffer(data_length_char_array_.data(), 2),
				[this, self](boost::system::error_code error, std::size_t byte_transferred) {
				if (!error) {
					data_length_ = convert_to_uint16_t(std::string((char *)data_length_char_array_.data(), 2));
					if (data_length_ == 0) {//close connection
						close_self();
					}
					else {
						strand_.post(
							[this, self]() {
							read_content();
						}
						);
					}
				}
				else {
					handle_error(error, byte_transferred);
				}
			}
			);
		}

		void read_content()
		{
			auto self(shared_from_this());
			backup_string_.reserve(data_length_);
			//notice here
			void * data_ptr = const_cast<char *>(backup_string_.data());
			boost::asio::async_read(
				socket_,
				boost::asio::buffer(data_ptr, data_length_),
				[this, self](boost::system::error_code error, std::size_t byte_transferred) {
				if (!error) {
					{
						std::lock_guard<std::mutex> my_lock_guard(this->in_box_mutex_);

						const char * data_ptr = backup_string_.data();
						in_box_.push(std::string(data_ptr, data_length_));//make string size work properly
					}
					strand_.post(
						[this, self]() {
						read_length();
					}
					);
				}
				else {
					handle_error(error, byte_transferred);
				}
			}
			);
		}

		void handle_error(const boost::system::error_code& error,
			size_t byte_transferred) {
			if (error) {
				close_self();
			}
		}


		tcp::socket socket_;
		boost::asio::io_context::strand strand_;
		std::queue<std::string> in_box_;
		std::queue<std::string> out_box_;
		bool is_writing_;
		std::array<uint8_t, 2> data_length_char_array_;
		uint16_t data_length_;
		std::string backup_string_;
		std::mutex in_box_mutex_;
		std::atomic_bool is_connected_;
		const uint16_t limit_ = 65535;	//2^16-1;
	};

}

#endif