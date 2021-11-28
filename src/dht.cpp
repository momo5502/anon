#include "std_include.hpp"
#include "dht.hpp"
#include "console.hpp"
#include "utils/io.hpp"

#include <atomic>
#include <string_view>
#include <random>

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

	struct dht_store
	{
		dht::id id{};
		std::vector<dht::node> nodes{};
	};

	std::string serialize_dht_store(const dht_store& store)
	{
		std::string data{};
		data.append(reinterpret_cast<const char*>(store.id.data()), store.id.size());

		std::vector<dht::node> ipv4_nodes{};
		std::vector<dht::node> ipv6_nodes{};

		for (const auto& node : store.nodes)
		{
			if (node.address.is_ipv4())
			{
				ipv4_nodes.emplace_back(node);
			}
			else if (node.address.is_ipv6())
			{
				ipv6_nodes.emplace_back(node);
			}
		}

		const auto ipv4_node_count = static_cast<uint32_t>(ipv4_nodes.size());
		const auto ipv6_node_count = static_cast<uint32_t>(ipv6_nodes.size());

		data.append(reinterpret_cast<const char*>(&ipv4_node_count), sizeof(ipv4_node_count));
		data.append(reinterpret_cast<const char*>(&ipv6_node_count), sizeof(ipv6_node_count));

		for (const auto& node : ipv4_nodes)
		{
			data.append(reinterpret_cast<const char*>(node.id_.data()), node.id_.size());

			const auto& in_addr = node.address.get_in_addr();
			const auto addr_size = sizeof(in_addr.sin_addr);
			static_assert(addr_size == 4);
			data.append(reinterpret_cast<const char*>(&in_addr.sin_addr), addr_size);

			const auto port = node.address.get_port();
			const auto port_size = sizeof(port);
			static_assert(port_size == 2);

			data.append(reinterpret_cast<const char*>(&port), port_size);
		}

		for (const auto& node : ipv6_nodes)
		{
			data.append(reinterpret_cast<const char*>(node.id_.data()), node.id_.size());

			const auto& in6_addr = node.address.get_in6_addr();
			const auto addr6_size = sizeof(in6_addr.sin6_addr);
			static_assert(addr6_size == 16);
			data.append(reinterpret_cast<const char*>(&in6_addr.sin6_addr), addr6_size);

			const auto port = node.address.get_port();
			const auto port_size = sizeof(port);
			static_assert(port_size == 2);

			data.append(reinterpret_cast<const char*>(&port), port_size);
		}

		return data;
	}

	void save_dht_store(const dht_store& store)
	{
		const auto data = serialize_dht_store(store);
		utils::io::write_file("./dht.store", data.data(), data.size(), false);
	}

	dht_store create_new_dht_store()
	{
		std::vector<uint8_t> random_data{};
		random_data.resize(0x100);
		dht_random_bytes(random_data.data(), random_data.size());

		dht_store store{};
		dht_hash(store.id.data(), static_cast<int>(store.id.size()), random_data.data(),
		         static_cast<int>(random_data.size()), "",
		         0, "", 0);

		save_dht_store(store);
		return store;
	}

	dht_store deserialize_dht_store(const std::string& data)
	{
		dht_store store{};
		uint32_t ipv4_node_count{};
		uint32_t ipv6_node_count{};

		size_t offset = 0;
		const auto read_data = [&data, &offset](void* destination, const size_t size)
		{
			if ((offset + size) > data.size())
			{
				throw std::runtime_error{"Serialized dht store is corrupted"};
			}

			memcpy(destination, data.data() + offset, size);
			offset += size;
		};

		read_data(store.id.data(), store.id.size());
		read_data(&ipv4_node_count, sizeof(ipv4_node_count));
		read_data(&ipv6_node_count, sizeof(ipv6_node_count));

		store.nodes.reserve(static_cast<size_t>(ipv4_node_count) + static_cast<size_t>(ipv6_node_count));

		for (uint32_t i = 0; i < ipv4_node_count; ++i)
		{
			dht::node node{};

			read_data(node.id_.data(), node.id_.size());

			in_addr address{};
			static_assert(sizeof(address) == 4);
			read_data(&address, sizeof(address));

			uint16_t port{};
			static_assert(sizeof(port) == 2);

			read_data(&port, sizeof(port));

			node.address.set_ipv4(address);
			node.address.set_port(port);

			store.nodes.emplace_back(std::move(node));
		}

		for (uint32_t i = 0; i < ipv4_node_count; ++i)
		{
			dht::node node{};

			read_data(node.id_.data(), node.id_.size());

			in6_addr address{};
			static_assert(sizeof(address) == 16);
			read_data(&address, sizeof(address));

			uint16_t port{};
			static_assert(sizeof(port) == 2);

			read_data(&port, sizeof(port));

			node.address.set_ipv6(address);
			node.address.set_port(port);

			store.nodes.emplace_back(std::move(node));
		}

		return store;
	}

	dht_store get_dht_store()
	{
		try
		{
			std::string data{};
			if (utils::io::read_file("./dht.store", &data))
			{
				return deserialize_dht_store(data);
			}
		}
		catch (...)
		{
		}

		return create_new_dht_store();
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

	const auto store = get_dht_store();
	this->id_ = store.id;

	const auto fd_v4 = store_fd_mapping(protocol::v4, *this);
	const auto fd_v6 = store_fd_mapping(protocol::v6, *this);

#ifndef NDEBUG
	dht_debug = stdout;
#endif

	dht_init(fd_v4, fd_v6, this->id_.data(),
	         reinterpret_cast<const unsigned char*>("JC\0\0"));

	this->try_ping("router.bittorrent.com", 6881);
	this->try_ping("router.utorrent.com", 6881);
	this->try_ping("dht.transmissionbt.com", 6881);
	this->try_ping("dht.aelitis.com", 6881);

	for (const auto& node : store.nodes)
	{
		this->insert_node(node);
	}
}

dht::~dht()
{
	try
	{
		this->save_state();
	}
	catch(std::exception& e)
	{
		console::error("Failed to save DHT state: %s", e.what());
	}

	dht_uninit();
	delete_fd_mappings(*this);
	get_dht_barrier().store(false);
}

void dht::on_data(protocol /*protocol*/, const network::address& address, const std::string& data)
{
	time_t tosleep = 0;
	dht_periodic(data.data(), data.size(), &address.get_addr(), address.get_size(), &tosleep,
	             &dht::callback_static, this);
}

int dht::on_send(const protocol protocol, const void* buf, const int len, const int /*flags*/,
                 const struct sockaddr* to, const int tolen)
{
	const network::address target{to, tolen};
	const std::string string{reinterpret_cast<const char*>(buf), static_cast<size_t>(len)};
	this->transmitter_(protocol, target, string);
	return len;
}

void dht::insert_node(const node& node)
{
	auto address = node.address;
	dht_insert_node(node.id_.data(), &address.get_addr(), address.get_size());
}

bool dht::try_ping(const std::string& hostname, const uint16_t port)
{
	bool pinged = false;
	auto addresses = network::address::resolve_multiple(hostname);
	
	for(auto& address : addresses)
	{
		if(address.is_supported())
		{
			address.set_port(port);
			this->ping(address);
			pinged = true;
		}
	}
	
	return pinged;
}

void dht::ping(const network::address& address)
{
	dht_ping_node(&address.get_addr(), address.get_size());
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
			dht_search(entry.first.data(), entry.second.port, AF_INET6, &dht::callback_static, this);
		}
	}

	time_t tosleep = 0;
	dht_periodic(nullptr, 0, nullptr, 0, &tosleep, &dht::callback_static, this);
	return std::min(std::chrono::seconds{tosleep}, 3s);
}

std::chrono::high_resolution_clock::time_point dht::run_frame_time_point()
{
	return this->run_frame() + std::chrono::high_resolution_clock::now();
}

void dht::handle_result_v4(const id& id, const std::string_view& data)
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
		address.set_ipv4(ip);
		address.set_port(ntohs(port));

		addresses.emplace_back(address);
	}

	console::info("Received %zu IPv4 addresses", addresses.size());

	const auto entry = this->searches_.find(id);
	if (entry != this->searches_.end())
	{
		entry->second.callback(addresses);
	}
}

void dht::handle_result_v6(const id& id, const std::string_view& data)
{
	std::vector<network::address> addresses{};
	addresses.reserve((data.size() / 18) + 1);

	size_t offset = 0;
	while ((data.size() - offset) >= 18)
	{
		in6_addr ip{};
		uint16_t port;
		memcpy(&ip.s6_addr, data.data() + offset, 16);
		memcpy(&port, data.data() + offset + 16, 2);
		offset += 18;

		network::address address{};
		address.set_ipv6(ip);
		address.set_port(ntohs(port));

		addresses.emplace_back(address);
	}

	console::info("Received %zu IPv6 addresses", addresses.size());

	const auto entry = this->searches_.find(id);
	if (entry != this->searches_.end())
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
	console::log("Event: %d (%zu)", event, data_len);

	if (event == DHT_EVENT_VALUES)
	{
		id hash{};
		memcpy(hash.data(), info_hash, hash.size());

		const std::string_view data_view{static_cast<const char*>(data), data_len};
		this->handle_result_v4(hash, data_view);
	}
	else if (event == DHT_EVENT_VALUES6)
	{
		id hash{};
		memcpy(hash.data(), info_hash, hash.size());

		const std::string_view data_view{static_cast<const char*>(data), data_len};
		this->handle_result_v6(hash, data_view);
	}
}

void dht::save_state() const
{
	dht_store store{};
	store.id = this->id_;

	int num = 0;
	int num6 = 0;

	int good_nodes{};
	int dubious_nodes{};
	int cached_nodes{};
	int incoming_nodes{};

	dht_nodes(AF_INET, &good_nodes, &dubious_nodes, &cached_nodes, &incoming_nodes);
	num = good_nodes + dubious_nodes + cached_nodes + incoming_nodes;

	dht_nodes(AF_INET6, &good_nodes, &dubious_nodes, &cached_nodes, &incoming_nodes);
	num6 = good_nodes + dubious_nodes + cached_nodes + incoming_nodes;

	constexpr auto id_size = id{}.size();

	std::vector<uint8_t> ids{};
	std::vector<sockaddr_in> addresses{};
	ids.resize(num * id_size);
	addresses.resize(num);

	std::vector<uint8_t> ids6{};
	std::vector<sockaddr_in6> addresses6{};
	ids6.resize(num6 * id_size);
	addresses6.resize(num6);

	dht_get_nodes(addresses.data(), ids.data(), &num, addresses6.data(), ids6.data(), &num6);

	const auto ipv4_nodes = std::min(addresses.size(), static_cast<size_t>(num));
	const auto ipv6_nodes = std::min(addresses6.size(), static_cast<size_t>(num6));
	store.nodes.reserve(ipv4_nodes + ipv6_nodes);

	for (size_t i = 0; i < ipv4_nodes; ++i)
	{
		node node{};
		memcpy(node.id_.data(), ids.data() + i * id_size, node.id_.size());
		memcpy(&node.address.get_in_addr(), &addresses.at(i), sizeof(sockaddr_in));
		store.nodes.emplace_back(std::move(node));
	}

	for (size_t i = 0; i < ipv6_nodes; ++i)
	{
		node node{};
		memcpy(node.id_.data(), ids6.data() + i * id_size, node.id_.size());
		memcpy(&node.address.get_in6_addr(), &addresses6.at(i), sizeof(sockaddr_in6));
		store.nodes.emplace_back(std::move(node));
	}

	save_dht_store(store);
}
