#include "Dx12DeferredReleaseQueue.h"

namespace noc
{
	void Dx12DeferredReleaseQueue::Enqueue(uint64_t fenceValue, dx12::ComPtr<IUnknown>&& obj)
	{
		if (!obj)
			return;

		Item it{};
		it.fenceValue = fenceValue;
		it.obj = std::move(obj);
		items_.push_back(std::move(it));
	}

	void Dx12DeferredReleaseQueue::Collect(uint64_t completedFenceValue)
	{
		// Compact in-place.
		size_t out = 0;
		for (size_t i = 0; i < items_.size(); ++i)
		{
			if (items_[i].fenceValue <= completedFenceValue)
			{
				// eligible: let ComPtr drop
				continue;
			}
			if (out != i)
				items_[out] = std::move(items_[i]);
			++out;
		}
		items_.resize(out);
	}

	void Dx12DeferredReleaseQueue::Clear()
	{
		items_.clear();
	}
}
