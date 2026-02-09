#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <vector>

namespace noc
{
	class Dx12DeferredReleaseQueue
	{
	public:
		void Enqueue(uint64_t fenceValue, dx12::ComPtr<IUnknown>&& obj);
		void Collect(uint64_t completedFenceValue);
		void Clear(); // drop everything immediately (only call after GPU idle)

	private:
		struct Item
		{
			uint64_t fenceValue = 0;
			dx12::ComPtr<IUnknown> obj;
		};

		std::vector<Item> items_;
	};
}
