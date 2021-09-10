#include "std_include.hpp"

#include "network/address.hpp"
#include <optional>
#include <string_view>

namespace network
{
	namespace
	{
		class wsa_initializer
		{
		public:
			wsa_initializer()
			{
#ifdef _WIN32
				WSADATA wsa_data;
				if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
				{
					throw std::runtime_error("Unable to initialize WSA");
				}
#endif
			}

			~wsa_initializer()
			{
#ifdef _WIN32
				WSACleanup();
#endif
			}
		};
	}

	void initialize_wsa()
	{
		static wsa_initializer wsa;
	}

	address::address()
	{
		initialize_wsa();
		this->address_.sa_family = AF_UNSPEC;
	}

	address::address(const std::string& addr)
		: address()
	{
		this->parse(addr);
	}

	address::address(const sockaddr_in6& addr)
		: address()
	{
		this->address6_ = addr;
	}

	address::address(const sockaddr_in& addr)
		: address()
	{
		this->address4_ = addr;
	}

	address::address(const sockaddr* addr, const int length)
		: address()
	{
		this->set_address(addr, length);
	}
	

	bool address::operator==(const address& obj) const
	{
		if(this->address_.sa_family != obj.address_.sa_family)
		{
			return false;
		}

		if(this->get_port() != obj.get_port())
		{
			return false;
		}

		if (this->address_.sa_family == AF_INET)
		{
			return this->address4_.sin_addr.s_addr == obj.address4_.sin_addr.s_addr;
		}
		else if (this->address_.sa_family == AF_INET6)
		{
			return !memcmp(this->address6_.sin6_addr.s6_addr, obj.address6_.sin6_addr.s6_addr, sizeof(obj.address6_.sin6_addr.s6_addr));
		}

		return false;
	}

	void address::set_ipv4(const in_addr addr)
	{
		ZeroMemory(&this->address4_, sizeof(this->address4_));
		this->address4_.sin_family = AF_INET;
		this->address4_.sin_addr = addr;
	}

	void address::set_ipv6(const in6_addr addr)
	{
		ZeroMemory(&this->address6_, sizeof(this->address6_));
		this->address6_.sin6_family = AF_INET6;
		this->address6_.sin6_addr = addr;
	}

	void address::set_address(const sockaddr* addr, const int length)
	{
		if(static_cast<size_t>(length) >= sizeof(sockaddr_in) && addr->sa_family == AF_INET)
		{
			this->address4_ = *reinterpret_cast<const sockaddr_in*>(addr);
		}
		else if(static_cast<size_t>(length) == sizeof(sockaddr_in6) && addr->sa_family == AF_INET6)
		{
			this->address6_ = *reinterpret_cast<const sockaddr_in6*>(addr);
		}
		else
		{
			throw std::runtime_error("Invalid network address");
		}
	}

	void address::set_port(const unsigned short port)
	{
		switch(this->address_.sa_family)
		{
		case AF_INET:
			this->address4_.sin_port = htons(port);
			break;
		case AF_INET6:
			this->address6_.sin6_port = htons(port);
			break;
		default:
			throw std::runtime_error("Invalid address family");
		}
	}

	unsigned short address::get_port() const
	{
		switch(this->address_.sa_family)
		{
		case AF_INET:
			return ntohs(this->address4_.sin_port);
		case AF_INET6:
			return ntohs(this->address6_.sin6_port);
		default:
			return 0;
		}
	}

	std::string address::to_string() const
	{
		char buffer[1000] = {0};
		
		switch(this->address_.sa_family)
		{
		case AF_INET:
			inet_ntop(this->address_.sa_family, &this->address4_.sin_addr, buffer, sizeof(buffer));
			break;
		case AF_INET6:
			inet_ntop(this->address_.sa_family, &this->address6_.sin6_addr, buffer, sizeof(buffer));
			break;
		default:
			buffer[0] = '?';
			buffer[1] = 0;
			break;
		}

		return std::string(buffer) + ":"s + std::to_string(this->get_port());
	}

	bool address::is_local() const
	{
		if(this->address_.sa_family != AF_INET)
		{
			return false;
		}

		// According to: https://en.wikipedia.org/wiki/Private_network

		uint8_t bytes[4];
		*reinterpret_cast<uint32_t*>(&bytes) = this->address4_.sin_addr.s_addr;

		// 10.X.X.X
		if (bytes[0] == 10)
		{
			return true;
		}

		// 192.168.X.X
		if (bytes[0] == 192
			&& bytes[1] == 168)
		{
			return true;
		}

		// 172.16.X.X - 172.31.X.X
		if (bytes[0] == 172
			&& bytes[1] >= 16
			&& bytes[1] < 32)
		{
			return true;
		}

		// 127.0.0.1
		if (this->address4_.sin_addr.s_addr == 0x0100007F)
		{
			return true;
		}

		return false;
	}

	sockaddr& address::get_addr()
	{
		return this->address_;
	}

	const sockaddr& address::get_addr() const
	{
		return this->address_;
	}

	sockaddr_in& address::get_in_addr()
	{
		return this->address4_;
	}

	sockaddr_in6& address::get_in6_addr()
	{
		return this->address6_;
	}

	const sockaddr_in& address::get_in_addr() const
	{
		return this->address4_;
	}

	const sockaddr_in6& address::get_in6_addr() const
	{
		return this->address6_;
	}

	int address::get_size() const
	{
		switch(this->address_.sa_family)
		{
		case AF_INET:
			return static_cast<int>(sizeof(this->address4_));
		case AF_INET6:
			return static_cast<int>(sizeof(this->address6_));
		default:
			return static_cast<int>(sizeof(this->address_));
		}
	}

	void address::parse(std::string addr)
	{
		std::optional<uint16_t> port_value{};

		const auto pos = addr.find_last_of(':');
		if (pos != std::string::npos)
		{
			auto port = addr.substr(pos + 1);
			port_value = uint16_t(atoi(port.data()));
			addr = addr.substr(0, pos);
		}

		this->resolve(addr);

		if(port_value)
		{
			this->set_port(*port_value);
		}
	}

	void address::resolve(const std::string& hostname)
	{
		const auto port = this->get_port();

		addrinfo* result = nullptr;
		if (!getaddrinfo(hostname.data(), nullptr, nullptr, &result))
		{
			for (auto* i = result; i; i = i->ai_next)
			{
				if (!(i->ai_addr->sa_family != AF_INET && i->ai_addr->sa_family != AF_INET6))
				{
					this->set_address(i->ai_addr, static_cast<int>(i->ai_addrlen));
					this->set_port(port);
					break;
				}
			}

			freeaddrinfo(result);
		}
	}
}

std::size_t std::hash<network::address>::operator()(const network::address& a) const noexcept
{
	const uint32_t family = a.get_addr().sa_family;
	const uint32_t port = a.get_port();

	std::size_t hash = std::hash<uint32_t>{}(family);
	hash ^= std::hash<uint32_t>{}(port);
	switch(a.get_addr().sa_family)
	{
	case AF_INET:
		hash ^= std::hash<decltype(a.get_in_addr().sin_addr.s_addr)>{}(a.get_in_addr().sin_addr.s_addr);
		break;
	case AF_INET6:
		hash ^= std::hash<std::string_view>{}(std::string_view{reinterpret_cast<const char*>(a.get_in6_addr().sin6_addr.s6_addr), sizeof(a.get_in6_addr().sin6_addr.s6_addr)});
		break;
	}

	return hash;
}
