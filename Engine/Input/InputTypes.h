#pragma once
#include <cstdint>

namespace noc
{
	// Keep public headers STL-free (project rule in architecture doc). :contentReference[oaicite:11]{index=11}

	// --- Keys (minimal set to start; extend as needed) ---
	enum class Key : uint16_t
	{
		Unknown = 0,

		Escape,

		W, A, S, D,
		Q, E,
		Space,
		LeftShift,
		LeftCtrl,

		MouseLeft,
		MouseRight,
		MouseMiddle,

		Count
	};

	struct MouseDelta
	{
		int dx = 0;
		int dy = 0;
	};

	// 32-bit stable ID for actions (hash of name).
	using ActionId = uint32_t;

	inline constexpr ActionId InvalidAction = 0;

	// Simple FNV-1a hash for action names (compile-time friendly if needed later).
	inline constexpr ActionId HashActionName(const char* s)
	{
		uint32_t h = 2166136261u;
		if (!s) return 0;
		while (*s)
		{
			h ^= static_cast<uint8_t>(*s++);
			h *= 16777619u;
		}
		return h;
	}
}
