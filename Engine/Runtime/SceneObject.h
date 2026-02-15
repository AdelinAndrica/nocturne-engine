#pragma once
#include <cstdint>

namespace noc
{
	struct SceneObjectHandle
	{
		uint32_t index = 0xFFFFFFFFu;
		uint32_t generation = 0;

		bool IsValid() const { return index != 0xFFFFFFFFu; }
	};

	inline bool operator==(const SceneObjectHandle& a, const SceneObjectHandle& b)
	{
		return a.index == b.index && a.generation == b.generation;
	}
}
