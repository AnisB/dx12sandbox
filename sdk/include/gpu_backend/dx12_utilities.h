#pragma once

// System includes
#include <string>

namespace graphics_sandbox
{
	// String manipulation
	std::wstring convert_to_wide(const std::string& str);
	std::wstring convert_to_wide(const char* str, uint32_t strLength);
}
