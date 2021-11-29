#include <std_include.hpp>

#include "console.hpp"
#include "dht.hpp"
#include "network/address.hpp"
#include "network/socket.hpp"

namespace
{
	void unsafe_main(const uint16_t port)
	{
		console::log("Creating socket on port %hu", port);

		network::address a{};
		a.set_ipv4(in_addr{INADDR_ANY});
		a.set_port(port);

		network::address a6{};
		a6.set_ipv6(in6addr_any);
		a6.set_port(port);

		network::socket s{AF_INET};
		s.set_blocking(false);
		if (!s.bind(a))
		{
			throw std::runtime_error("Failed to bind socket!");
		}

		network::socket s6{AF_INET6};
		s6.set_blocking(false);
		if (!s6.bind(a6))
		{
			throw std::runtime_error("Failed to bind socket!");
		}

		dht dht{
			[&s, &s6](const dht::protocol protocol, const network::address& destination, const std::string& data)
			{
				if (protocol == dht::protocol::v4)
				{
					s.send(destination, data);
				}
				else if (protocol == dht::protocol::v6)
				{
					s6.send(destination, data);
				}
			}
		};

		volatile bool kill = false;
		console::signal_handler handler([&]()
		{
			if (!kill)
			{
				console::new_line();
				console::log("Terminating server...");
			}

			kill = true;
		});

		dht.search("X-LABS", [&kill](const std::vector<network::address>& addresses)
		{
			for (const auto& address : addresses)
			{
				console::info("%s", address.to_string().data());
			}

			if (getenv("CI") != nullptr)
			{
				console::info("Terminating server, as running in a CI instance (CI environment variable is set");
				kill = true;
			}
		}, s.get_port());

		std::string data{};
		network::address address{};

		std::vector<const network::socket*> sockets{};
		sockets.push_back(&s);
		sockets.push_back(&s6);

		while (!kill)
		{
			const auto time = dht.run_frame();
			network::socket::sleep_sockets(sockets, time);

			while (s.receive(address, data))
			{
				dht.on_data(dht::protocol::v4, address, data);
				data.clear();
			}

			while (s6.receive(address, data))
			{
				dht.on_data(dht::protocol::v6, address, data);
				data.clear();
			}
		}
	}
}

uint16_t parse_port(const int argc, const char** argv)
{
	if (argc <= 1)
	{
		return 6881;
	}

	const auto port_string = argv[1];
	const auto port_num = atoi(port_string);
	return static_cast<uint16_t>(port_num);
}

int main(const int argc, const char** argv)
{
	console::set_title("ANoN Node");
	console::log("Starting ANoN Node");

	try
	{
		const auto port = parse_port(argc, argv);
		unsafe_main(port);
	}
	catch (std::exception& e)
	{
		console::error("Fatal error: %s\n", e.what());
		return -1;
	}

	return 0;
}
