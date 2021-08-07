#include <std_include.hpp>

#include "console.hpp"
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
		if (!s.bind(a))
		{
			throw std::runtime_error("Failed to bind socket!");
		}

		console::signal_handler handler([&s]()
		{
			
		});

		// TODO: Run

		console::log("Terminating server...");
	}
}


int main(const int argc, const char** argv)
{
	console::set_title("ANoN Node");
	console::log("Starting ANoN Node");

	try
	{
		unsafe_main(argc > 1 ? static_cast<uint16_t>(atoi(argv[1])) : 20810);
	}
	catch (std::exception& e)
	{
		console::error("Fatal error: %s\n", e.what());
		return -1;
	}

	return 0;
}
