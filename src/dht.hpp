#pragma once

class dht
{
public:
	dht();
	~dht();

	dht(const dht&) = delete;
	dht& operator=(const dht&) = delete;

	dht(dht&&) = delete;
	dht& operator=(dht&&) = delete;
};
