#pragma once

#include "network/socket.hpp"
#include <array>
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
	using id = std::array<unsigned char, 20>;
	using results = std::function<void(const std::vector<network::address>&)>;

	dht(network::socket& socket);
	~dht();

	dht(const dht&) = delete;
	dht& operator=(const dht&) = delete;

	dht(dht&&) = delete;
	dht& operator=(dht&&) = delete;

	void on_data(const std::string& data, const network::address& address);

	void ping(const network::address& address);
	void search(const std::string& keyword, results results, uint16_t port);
	void search(const id& hash, results results, uint16_t port);

	std::chrono::milliseconds run_frame();

private:
	struct search_entry
	{
		results callback{};
		uint16_t port{};
		std::chrono::system_clock::time_point last_query{};
	};

	network::socket& socket_;
	std::unordered_map<id, search_entry> searches_;

	void handle_result(const id& id, const std::string_view& data);

	static void callback_static(void* closure, int event, const unsigned char* info_hash, const void* data,
	                            size_t data_len);
	void callback(int event, const unsigned char* info_hash, const void* data, size_t data_len);
};
