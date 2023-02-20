#pragma once

// System includes
#include <queue>

namespace graphics_sandbox
{
	enum class FrameEvent
	{
		Paint,
		Close,
		Destroy
	};

	namespace event_collector
	{
		void push_event(FrameEvent event);
		bool peek_event(FrameEvent& event);
	}
}