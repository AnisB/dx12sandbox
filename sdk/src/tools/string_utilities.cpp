#include "tools/string_utilities.h"

namespace graphics_sandbox
{
	std::wstring convert_to_wide(const std::string& str)
	{
		size_t stringSize = str.size();
		std::wstring wc(stringSize, L'#');
		mbstowcs(&wc[0], str.c_str(), stringSize);
		return wc;
	}

	std::wstring convert_to_wide(const char* str, uint32_t strLength)
	{
		size_t stringSize = strLength;
		std::wstring wc(stringSize, L'#');
		mbstowcs(&wc[0], str, stringSize);
		return wc;
	}
}
