
//
// ===============================================================================
// clReflect
// -------------------------------------------------------------------------------
// Copyright (c) 2011-2012 Don Williamson & clReflect Authors (see AUTHORS file)
// Released under MIT License (see LICENSE file)
// ===============================================================================
//

#include "FileUtils.h"

#include <cstring>


char* ReadLine(FILE* fp)
{
	thread_local static char line[4096];

	// Loop reading characters until EOF or EOL
	int pos = 0;
	while (true)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return 0;
		}
		if (c == '\n')
		{
			break;
		}

		// Only add if the line is below the length of the buffer
		if (pos < sizeof(line) - 1)
		{
			line[pos++] = c;
		}
	}

	// Null terminate and return
	line[pos] = 0;
	return line;
}


const char* itoa(unsigned int value)
{
	static const int MAX_SZ = 20;
	thread_local static char text[MAX_SZ];
#ifdef _MSC_VER
	return _itoa(value, text, 10);
#else
    snprintf(text, 10, "%u", value);
    return text;
#endif  // _MSC_VER
}


const char* itohex(unsigned int value)
{
	static const int MAX_SZ = 9;
	thread_local static char text[MAX_SZ];

	// Null terminate and start at the end
	text[MAX_SZ - 1] = 0;
	char* tptr = text + MAX_SZ - 1;

	// Loop through the value with radix 16
	do 
	{
		int v = value & 15;
		*--tptr = "0123456789abcdef"[v];
		value /= 16;
	} while (value);

	// Zero-pad whats left
	while (tptr != text)
	{
		*--tptr = '0';
	}

	return tptr;
}


clcpp::uint32 hextoi(const char* text)
{
	// Sum each radix 16 element
	unsigned int val = 0;
	for (const char* tptr = text, *end = text + strlen(text); tptr != end; ++tptr)
	{
		val *= 16;
		char ch = tolower(*tptr);
		int v = ch >= 'a' ? ch - 'a' + 10 : ch - '0';
		val += v;
	}

	return val;
}


clcpp::uint64 hextoi64(const char* text)
{
	// Sum each radix 16 element
	clcpp::uint64 val = 0;
	for (const char* tptr = text, *end = text + strlen(text); tptr != end; ++tptr)
	{
		val *= 16;
		char ch = tolower(*tptr);
		int v = ch >= 'a' ? ch - 'a' + 10 : ch - '0';
		val += v;
	}

	return val;
}


bool startswith(const char* text, const char* cmp)
{
	return strstr(text, cmp) == text;
}


bool startswith(const std::string& text, const char* cmp)
{
	return startswith(text.c_str(), cmp);
}


const char* SkipWhitespace(const char* text)
{
	while (*text == ' ' || *text == '\t')
		text++;
	return text;
}


const char* ConsumeToken(const char* text, char delimiter, char* dest, int dest_size)
{
	char* end = dest + dest_size;
	while (*text && *text != delimiter && dest != end)
	{
		*dest++ = *text++;
	}
	*dest = 0;
	return text;
}


std::string StringReplace(const std::string& str, const std::string& find, const std::string& replace)
{
	std::string res = str;
	for (size_t i = res.find(find); i != res.npos; i = res.find(find, i))
	{
		res.replace(i, find.length(), replace);
		i += replace.length();
	}
	return res;
}


