#pragma once

#include "network/socket.hpp"

class dht
{
public:
	struct Id {
		char data[20];
	};
	
	dht(network::socket& socket);
	~dht();

	dht(const dht&) = delete;
	dht& operator=(const dht&) = delete;

	dht(dht&&) = delete;
	dht& operator=(dht&&) = delete;

	void on_data(const std::string& data, const network::address& address);

	void ping(const network::address& address);

	std::chrono::milliseconds run_frame();

private:
	network::socket& socket_;

	static void callback_static(void* closure, int event, const unsigned char* info_hash, const void* data, size_t data_len);
	void callback(int event, const unsigned char* info_hash, const void* data, size_t data_len);
};
