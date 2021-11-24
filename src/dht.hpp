#pragma once

#include "network/socket.hpp"
#include <array>
#include <atomic>
#include <string_view>
#include <unordered_map>

namespace std
{
	template <typename T, size_t N>
	struct hash<array<T, N>>
	{
		typedef array<T, N> argument_type;
		typedef size_t result_type;

		result_type operator()(const argument_type& a) const
		{
			hash<T> hasher;
			result_type h = 0;
			for (result_type i = 0; i < N; ++i)
			{
				h = h * 31 + hasher(a[i]);
			}
			return h;
		}
	};
}

class dht
{
public:
	enum class protocol
	{
		v4,
		v6,
	};

	using id = std::array<unsigned char, 20>;
	using results = std::function<void(const std::vector<network::address>&)>;
	using data_transmitter = std::function<void(protocol, const network::address& destination, const std::string& data)>
	;

	struct node
	{
		id id_{};
		network::address address{};
	};

	dht(data_transmitter transmitter);
	~dht();

	dht(const dht&) = delete;
	dht& operator=(const dht&) = delete;

	dht(dht&&) = delete;
	dht& operator=(dht&&) = delete;

	void insert_node(const node& node);

	bool try_ping(const std::string& address);
	void ping(const network::address& address);
	void search(const std::string& keyword, results results, uint16_t port);
	void search(const id& hash, results results, uint16_t port);

	std::chrono::milliseconds run_frame();
	std::chrono::high_resolution_clock::time_point run_frame_time_point();

	void on_data(protocol protocol, const network::address& address, const std::string& data);
	int on_send(protocol protocol, const void* buf, const int len, const int flags,
	            const struct sockaddr* to, const int tolen);

private:
	id id_;

	struct search_entry
	{
		results callback{};
		uint16_t port{};
		std::chrono::system_clock::time_point last_query{};
	};

	data_transmitter transmitter_;
	std::unordered_map<id, search_entry> searches_;

	void handle_result_v4(const id& id, const std::string_view& data);
	void handle_result_v6(const id& id, const std::string_view& data);

	static void callback_static(void* closure, int event, const unsigned char* info_hash, const void* data,
	                            size_t data_len);
	void callback(int event, const unsigned char* info_hash, const void* data, size_t data_len);

	void save_state() const;
};
