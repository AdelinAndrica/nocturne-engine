#pragma once
#include "InputTypes.h"
#include <cstdint>

namespace noc
{
	class InputSystem;

	// Small, fixed-capacity action map (no STL in public header).
	class ActionMap
	{
	public:
		enum class SourceType : uint8_t
		{
			DigitalKey = 0,
			DigitalMouseButton,

			AnalogMouseDeltaX,
			AnalogMouseDeltaY,
			AnalogMouseWheel
		};

		struct Binding
		{
			ActionId    action = InvalidAction;
			SourceType  type = SourceType::DigitalKey;

			// For DigitalKey / DigitalMouseButton.
			Key         key = Key::Unknown;

			// Scale for analog sources.
			float       scale = 1.0f;

			// For digital, optional: treat as "negative" contribution (e.g., S on MoveForward).
			bool        negate = false;
		};

	public:
		void Clear();

		// Returns false if capacity exceeded.
		bool Bind(const Binding& b);

		// Rebind by action + source type (simple, first match). Returns true if replaced.
		bool Rebind(ActionId action, SourceType type, const Binding& replacement);

		// Call once per frame after InputSystem has updated its snapshot.
		void Update(const InputSystem& input);

		// Query
		float GetActionValue(ActionId action) const;
		bool  IsActionDown(ActionId action) const { return GetActionValue(action) != 0.0f; }

	private:
		static constexpr uint32_t kMaxBindings = 64;
		static constexpr uint32_t kMaxActionsCached = 64;

		Binding bindings_[kMaxBindings]{};
		uint32_t bindingCount_ = 0;

		// Cached computed values (per frame).
		struct ActionValue
		{
			ActionId action = InvalidAction;
			float    value = 0.0f;
		};
		ActionValue values_[kMaxActionsCached]{};
		uint32_t valueCount_ = 0;

	private:
		void SetValue_(ActionId action, float v);
		float GetValue_(ActionId action) const;
	};
}
