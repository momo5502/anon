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
	hash.update(static_cast<const uint8_t*>(v1), len1);
	hash.update(static_cast<const uint8_t*>(v2), len2);
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

dht::dht()
{
	bool expected = false;
	if(!get_dht_barrier().compare_exchange_strong(expected, true))
	{
		throw std::runtime_error("Only one DHT instance supported at a time");
	}

	unsigned char id[20];
	dht_init(0, -1, id, reinterpret_cast<const unsigned char*>("JC\0\0"));
}

dht::~dht()
{
	get_dht_barrier().store(false);
}
