#pragma once

namespace network
{
	void initialize_wsa();

	class address
	{
	public:
		address();
		address(const std::string& addr);
		address(const sockaddr_in& addr);
		address(const sockaddr_in6& addr);
		address(const sockaddr* addr, int length);

		void set_ipv4(uint32_t ip);
		void set_ipv4(const in_addr& addr);
		void set_ipv6(const in6_addr& addr);
		void set_address(const sockaddr* addr, int length);

		void set_port(unsigned short port);
		[[nodiscard]] unsigned short get_port() const;

		sockaddr& get_addr();
		sockaddr_in& get_in_addr();
		sockaddr_in6& get_in6_addr();

		const sockaddr& get_addr() const;
		const sockaddr_in& get_in_addr() const;
		const sockaddr_in6& get_in6_addr() const;

		int get_size() const;
		int get_max_size() const;

		bool is_ipv4() const;
		bool is_ipv6() const;
		bool is_supported() const;

		[[nodiscard]] bool is_local() const;
		[[nodiscard]] std::string to_string() const;

		bool operator==(const address& obj) const;

		bool operator!=(const address& obj) const
		{
			return !(*this == obj);
		}

		static std::vector<address> resolve_multiple(const std::string& hostname);

	private:
		union
		{
			sockaddr address_;
			sockaddr_in address4_;
			sockaddr_in6 address6_;
			sockaddr_storage storage_;
		};

		void parse(std::string addr);
		void resolve(const std::string& hostname);
	};
}

namespace std
{
	template <>
	struct hash<network::address>
	{
		std::size_t operator()(const network::address& a) const noexcept;
	};
}
