#include "std_include.hpp"
#include "dht.hpp"
#include "utils/io.hpp"
#include <random>

#include "console.hpp"

namespace
{
	std::atomic_bool& get_dht_barrier()
	{
		static std::atomic_bool barrier{false};
		return barrier;
	}

	std::atomic<int>& get_current_fd()
	{
		static std::atomic<int> current_id{0};
		return current_id;
	}

	std::unordered_map<int, std::pair<dht::protocol, dht*>>& get_fd_mapping()
	{
		static std::unordered_map<int, std::pair<dht::protocol, dht*>> fd_mapping{};
		return fd_mapping;
	}

	void delete_fd_mappings(dht& dht)
	{
		auto& fd_mappings = get_fd_mapping();
		for (auto i = fd_mappings.begin(); i != fd_mappings.end();)
		{
			if (i->second.second != &dht)
			{
				++i;
				continue;
			}

			i = fd_mappings.erase(i);
		}
	}

	int store_fd_mapping(const dht::protocol protocol, dht& dht)
	{
		const auto fd = ++get_current_fd();
		get_fd_mapping()[fd] = {protocol, &dht};
		return fd;
	}

	const std::pair<dht::protocol, dht*>& get_fd_mapping_entry(const int fd)
	{
		return get_fd_mapping().at(fd);
	}

	dht::id get_id()
	{
		dht::id id{};
		std::string id_data{};
		if (utils::io::read_file("./dht.id", &id_data) && id_data.size() == id.size())
		{
			memcpy(id.data(), id_data.data(), id.size());
			return id;
		}

		dht::id random_data{};
		dht_random_bytes(random_data.data(), random_data.size());
		dht_hash(id.data(), static_cast<int>(id.size()), random_data.data(), static_cast<int>(random_data.size()), "",
		         0, "", 0);
		utils::io::write_file("./dht.id", id.data(), id.size(), false);

		return id;
	}
}

int dht_random_bytes(void* buf, const size_t size)
{
	std::random_device rd;
	std::default_random_engine engine{rd()};
	std::uniform_int_distribution<int> dist(0, 255);

	for (size_t i = 0; i < size; ++i)
	{
		static_cast<uint8_t*>(buf)[i] = static_cast<uint8_t>(dist(engine));
	}

	return static_cast<int>(size);
}

void dht_hash(void* hash_return, const int hash_size, const void* v1, const int len1, const void* v2, const int len2,
              const void* v3, const int len3)
{
	ZeroMemory(hash_return, hash_size);

	SHA256 hash{};
	hash.update(reinterpret_cast<const uint8_t*>(&hash_size), sizeof(hash_size));
	hash.update(reinterpret_cast<const uint8_t*>(&len1), sizeof(len1));
	hash.update(static_cast<const uint8_t*>(v1), len1);
	hash.update(reinterpret_cast<const uint8_t*>(&len2), sizeof(len2));
	hash.update(static_cast<const uint8_t*>(v2), len2);
	hash.update(reinterpret_cast<const uint8_t*>(&len3), sizeof(len3));
	hash.update(static_cast<const uint8_t*>(v3), len3);

	for (int i = 0; i < hash_size; ++i)
	{
		if (hash_size % 32 == 0)
		{
			hash.update(static_cast<const uint8_t*>(hash_return), hash_size);
		}

		static_cast<uint8_t*>(hash_return)[i] = hash.digest()[i % 32];
	}
}

int dht_blacklisted(const struct sockaddr* /*sa*/, int /*salen*/)
{
	return 0;
}

int dht_sendto(const int sockfd, const void* buf, const int len, const int flags,
               const struct sockaddr* to, const int tolen)
{
	const auto& mapping = get_fd_mapping_entry(sockfd);
	return mapping.second->on_send(mapping.first, buf, len, flags, to, tolen);
}

#ifdef WIN32
extern "C" int dht_gettimeofday(struct timeval* tp, struct timezone* /*tzp*/)
{
	static const uint64_t epoch = 116444736000000000ULL;

	SYSTEMTIME system_time;
	GetSystemTime(&system_time);

	FILETIME file_time;
	SystemTimeToFileTime(&system_time, &file_time);

	ULARGE_INTEGER ularge;
	ularge.LowPart = file_time.dwLowDateTime;
	ularge.HighPart = file_time.dwHighDateTime;

	tp->tv_sec = LONG((ularge.QuadPart - epoch) / 10000000L);
	tp->tv_usec = LONG(system_time.wMilliseconds * 1000);

	return 0;
}
#endif

dht::dht(data_transmitter transmitter)
	: transmitter_(std::move(transmitter))
{
	bool expected = false;
	if (!get_dht_barrier().compare_exchange_strong(expected, true))
	{
		throw std::runtime_error("Only one DHT instance supported at a time");
	}

	const auto id = get_id();

	const auto fd_v4 = store_fd_mapping(protocol::v4, *this);
	const auto fd_v6 = store_fd_mapping(protocol::v6, *this);

#ifndef NDEBUG
	dht_debug = stdout;
#endif

	dht_init(fd_v4, fd_v6, id.data(),
	         reinterpret_cast<const unsigned char*>("JC\0\0"));

	this->try_ping("router.bittorrent.com:6881");
	this->try_ping("router.utorrent.com:6881");
	this->try_ping("router.bitcomet.com:6881");
	this->try_ping("dht.transmissionbt.com:6881");
	this->try_ping("dht.aelitis.com:6881");
}

dht::~dht()
{
	dht_uninit();
	delete_fd_mappings(*this);
	get_dht_barrier().store(false);
}

void dht::on_data(protocol /*protocol*/, const network::address& address, const std::string& data)
{
	time_t tosleep = 0;
	dht_periodic(data.data(), data.size(), &address.get_addr(), sizeof(address.get_in_addr()), &tosleep,
	             &dht::callback_static, this);
}

int dht::on_send(protocol protocol, const void* buf, const int len, const int /*flags*/,
                 const struct sockaddr* to, const int /*tolen*/)
{
	const network::address target{*reinterpret_cast<const sockaddr_in*>(to)};
	const std::string string{reinterpret_cast<const char*>(buf), static_cast<size_t>(len)};
	this->transmitter_(protocol, target, string);
	return len;
}

bool dht::try_ping(const std::string& address)
{
	try
	{
		this->ping(network::address{address});
		return true;
	}
	catch (...)
	{
	}
	return false;
}

void dht::ping(const network::address& address)
{
	dht_ping_node(&address.get_addr(), sizeof(address.get_in_addr()));
}

void dht::search(const std::string& keyword, results results, const uint16_t port)
{
	id hash{};
	dht_hash(hash.data(), static_cast<int>(hash.size()), keyword.data(), static_cast<int>(keyword.size()), "", 0, "",
	         0);
	this->search(hash, std::move(results), port);
}

void dht::search(const id& hash, results results, const uint16_t port)
{
	search_entry entry{};
	entry.callback = std::move(results);
	entry.port = port;

	this->searches_[hash] = std::move(entry);
}

std::chrono::milliseconds dht::run_frame()
{
	const auto now = std::chrono::system_clock::now();

	for (auto& entry : this->searches_)
	{
		if ((now - entry.second.last_query) > 1min)
		{
			entry.second.last_query = now;
			dht_search(entry.first.data(), entry.second.port, AF_INET, &dht::callback_static, this);
		}
	}

	time_t tosleep = 0;
	dht_periodic(nullptr, 0, nullptr, 0, &tosleep, &dht::callback_static, this);
	return std::chrono::seconds{tosleep};
}

std::chrono::high_resolution_clock::time_point dht::run_frame_time_point()
{
	return this->run_frame() + std::chrono::high_resolution_clock::now();
}

void dht::handle_result(const id& id, const std::string_view& data)
{
	std::vector<network::address> addresses{};
	addresses.reserve((data.size() / 6) + 1);

	size_t offset = 0;
	while ((data.size() - offset) >= 6)
	{
		in_addr ip{};
		uint16_t port;
		memcpy(&ip.s_addr, data.data() + offset, 4);
		memcpy(&port, data.data() + offset + 4, 2);
		offset += 6;

		network::address address{};
		address.set_port(ntohs(port));
		address.set_ipv4(ip);

		addresses.emplace_back(address);
	}

	const auto entry = this->searches_.find(id);
	if (entry == this->searches_.end())
	{
		entry->second.callback(addresses);
	}
}

void dht::callback_static(void* closure, const int event, const unsigned char* info_hash, const void* data,
                          const size_t data_len)
{
	static_cast<dht*>(closure)->callback(event, info_hash, data, data_len);
}

void dht::callback(const int event, const unsigned char* info_hash, const void* data, const size_t data_len)
{
	console::log("Event: %d", event);

	if (event == DHT_EVENT_VALUES)
	{
		id hash{};
		memcpy(hash.data(), info_hash, hash.size());

		const std::string_view data_view{static_cast<const char*>(data), data_len};
		this->handle_result(hash, data_view);
	}
}
