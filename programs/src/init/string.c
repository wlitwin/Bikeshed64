#include "string.h"

uint64_t strlen(const char* str)
{
	uint64_t length = 0;
	while (*str != 0)
	{
		++length;
		++str;
	}

	return length;
}

uint8_t streq(const char* str1, const char* str2)
{
	while (*str1 != 0 && *str2 != 0)
	{
		const char c1 = *str1;
		const char c2 = *str2;
		if (c1 != c2)
		{
			return 0;
		}

		++str1;
		++str2;
	}

	return *str1 == 0 && *str2 == 0;
}

