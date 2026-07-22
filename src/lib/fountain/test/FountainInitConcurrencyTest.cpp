/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "FountainInit.h"

#include <atomic>
#include <thread>

bool fountain_init_from_peer_translation_unit();

int main()
{
	std::atomic<unsigned> ready{0U};
	std::atomic<bool> start{false};
	std::atomic<bool> local_result{false};
	std::atomic<bool> peer_result{false};

	auto await_start = [&ready, &start]
	{
		ready.fetch_add(1U, std::memory_order_release);
		while (!start.load(std::memory_order_acquire))
			std::this_thread::yield();
	};

	std::thread local([&]
	{
		await_start();
		local_result.store(FountainInit::init(), std::memory_order_release);
	});
	std::thread peer([&]
	{
		await_start();
		peer_result.store(fountain_init_from_peer_translation_unit(), std::memory_order_release);
	});

	while (ready.load(std::memory_order_acquire) != 2U)
		std::this_thread::yield();
	start.store(true, std::memory_order_release);
	local.join();
	peer.join();

	return local_result.load(std::memory_order_acquire) &&
	       peer_result.load(std::memory_order_acquire) ? 0 : 1;
}
