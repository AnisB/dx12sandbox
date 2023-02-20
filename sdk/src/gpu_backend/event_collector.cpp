// Internal includes
#include "gpu_backend/event_collector.h"

namespace graphics_sandbox
{
	namespace event_collector
	{
		// Queue that is used to keep track of the events
		static std::queue<FrameEvent> eventQueue;

		// If an event has been recorded process it
		bool peek_event(FrameEvent& event)
		{
			if (eventQueue.size() > 0)
			{
				event = eventQueue.front();
				eventQueue.pop();
				return true;
			}
			return false;
		}

		// Keep track of this event
		void push_event(FrameEvent event)
		{
			eventQueue.push(event);
		}
	}
}