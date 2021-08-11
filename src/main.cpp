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

		network::address a{"0.0.0.0"};
		a.set_port(port);

		network::socket s{};
		s.set_blocking(false);
		if (!s.bind(a))
		{
			throw std::runtime_error("Failed to bind socket!");
		}

		dht dht{s, [](const std::vector<network::address>& addresses)
		{
			for(const auto& address : addresses)
			{
				console::info("%s", address.to_string().data());
			}
		}};
		
		volatile bool kill = false;
		console::signal_handler handler([&]()
		{
			kill = true;
		});

		std::chrono::system_clock::time_point last_search{};
		
		std::string data{};
		network::address address{};

		while(!kill)
		{
			const auto time = dht.run_frame();
			(void)s.sleep(time);

			if(s.receive(address, data))
			{
				dht.on_data(data, address);
				data.clear();
			}

			const auto now = std::chrono::system_clock::now();
			if((now - last_search) > 1min)
			{
				console::log("Searching...");
				last_search = now;
				dht.search("X-LABS");
			}
		}
		
		console::log("Terminating server...");
	}
}


int main(const int argc, const char** argv)
{
	console::set_title("ANoN Node");
	console::log("Starting ANoN Node");

	try
	{
		unsafe_main(argc > 1 ? static_cast<uint16_t>(atoi(argv[1])) : 20811);
	}
	catch (std::exception& e)
	{
		console::error("Fatal error: %s\n", e.what());
		return -1;
	}

	return 0;
}
