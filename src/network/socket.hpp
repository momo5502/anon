#pragma once

#include "network/address.hpp"

#ifdef _WIN32
using socklen_t = int;
#else
		using SOCKET = int;
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#endif

namespace network
{
	class socket
	{
	public:
		socket(int af = AF_INET);
		~socket();

		socket(const socket& obj) = delete;
		socket& operator=(const socket& obj) = delete;

		socket(socket&& obj) noexcept;
		socket& operator=(socket&& obj) noexcept;

		bool bind(const address& target);

		void send(const address& target, const std::string& data) const;
		bool receive(address& source, std::string& data) const;

		bool set_blocking(bool blocking);

		static const bool socket_is_ready = true;
		bool sleep(std::chrono::milliseconds timeout) const;
		bool sleep_until(std::chrono::high_resolution_clock::time_point time_point) const;

		SOCKET get_socket() const;
		uint16_t get_port() const;

	private:
		uint16_t port_ = 0;
		SOCKET socket_ = INVALID_SOCKET;
	};
}
