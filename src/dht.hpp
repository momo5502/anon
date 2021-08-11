#pragma once

#include "network/socket.hpp"

class dht
{
public:
	using id = std::array<unsigned char, 20>;
	using results = std::function<void(const std::vector<network::address>&)>;
	
	dht(network::socket& socket, results results);
	~dht();

	dht(const dht&) = delete;
	dht& operator=(const dht&) = delete;

	dht(dht&&) = delete;
	dht& operator=(dht&&) = delete;

	void on_data(const std::string& data, const network::address& address);

	void ping(const network::address& address);
	void search(const std::string& keyword);

	std::chrono::milliseconds run_frame();

private:
	results results_;
	network::socket& socket_;

	static void callback_static(void* closure, int event, const unsigned char* info_hash, const void* data, size_t data_len);
	void callback(int event, const unsigned char* info_hash, const void* data, size_t data_len);
};
