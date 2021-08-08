#include "std_include.hpp"
#include "dht.hpp"

#include <random>

int dht_random_bytes(void *buf, const size_t size)
{
	std::random_device rd;
	std::default_random_engine engine{rd()};
   std::uniform_int_distribution<int> dist(0, 255);

	 for (size_t i = 0; i < size; ++i) {
		 static_cast<uint8_t*>(buf)[i] = static_cast<uint8_t>(dist(engine));
	 }

	return static_cast<int>(size);
}

void dht_hash(void *hash_return, const int hash_size, const void *v1, const int len1, const void *v2, const int len2, const void *v3, const int len3)
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

int dht_sendto(const int sockfd, const void *buf, const int len, const int flags,
               const struct sockaddr *to, const int tolen)
{
	return sendto(sockfd, static_cast<const char*>(buf), len, flags, to, tolen);
}

#ifdef WIN32
extern "C" int dht_gettimeofday(struct timeval *tp, struct timezone* /*tzp*/)
{
	static const uint64_t epoch = 116444736000000000ULL;

	SYSTEMTIME systemTime;
	GetSystemTime(&systemTime);

	FILETIME fileTime;
	SystemTimeToFileTime(&systemTime, &fileTime);

	ULARGE_INTEGER ularge;
	ularge.LowPart = fileTime.dwLowDateTime;
	ularge.HighPart = fileTime.dwHighDateTime;

	tp->tv_sec = LONG((ularge.QuadPart - epoch) / 10000000L);
	tp->tv_usec = LONG(systemTime.wMilliseconds * 1000);

	return 0;
}
#endif

namespace
{

std::atomic_bool& get_dht_barrier()
{
	static std::atomic_bool barrier{false};
	return barrier;
}

}

dht::dht(network::socket& socket)
	: socket_(socket)
{
	bool expected = false;
	if (!get_dht_barrier().compare_exchange_strong(expected, true))
	{
		throw std::runtime_error("Only one DHT instance supported at a time");
	}

	unsigned char id[20];
	dht_hash(id, sizeof(id), "h", 1, "", 0, "", 0);

	dht_debug = stdout;
	
	dht_init(static_cast<int>(this->socket_.get_socket()), -1, id, reinterpret_cast<const unsigned char*>("JC\0\0"));

	this->ping(network::address{"router.bittorrent.com:6881"});
	this->ping(network::address{"router.utorrent.com:6881"});
	this->ping(network::address{"router.bitcomet.com:6881"});
	this->ping(network::address{"dht.transmissionbt.com:6881"});
	this->ping(network::address{"dht.aelitis.com:6881"});
	
	dht_search(reinterpret_cast<const unsigned char*>(
		           "\xE3\x81\x1B\x95\x39\xCA\xCF\xF6\x80\xE4\x18\x12\x42\x72\x17\x7C\x47\x47\x71\x57"), 20810, AF_INET, &dht::callback_static, this);
}

dht::~dht()
{
	dht_uninit();
	get_dht_barrier().store(false);
}

void dht::on_data(const std::string& data, const network::address& address)
{
	time_t tosleep = 0;
	dht_periodic(data.data(), data.size(), &address.get_addr(), sizeof(address.get_in_addr()), &tosleep,
	             &dht::callback_static, this);
}

void dht::ping(const network::address& address)
{
	dht_ping_node(&address.get_addr(), sizeof(address.get_in_addr()));
}

std::chrono::milliseconds dht::run_frame()
{
	time_t tosleep = 0;
	dht_periodic(nullptr, 0, nullptr, 0, &tosleep, &dht::callback_static, this);
	return std::chrono::seconds{tosleep};
}

void dht::callback_static(void* closure, const int event, const unsigned char* info_hash, const void* data, const size_t data_len)
{
	static_cast<dht*>(closure)->callback(event, info_hash, data, data_len);
}

void dht::callback(int event, const unsigned char* /*info_hash*/, const void* /*data*/, size_t /*data_len*/)
{
	printf("Event: %d\n", event);
}
