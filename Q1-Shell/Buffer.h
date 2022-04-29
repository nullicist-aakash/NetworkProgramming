#pragma once
#include <fstream>
#include <cassert>

class Buffer
{
private:
	int charTaken = 0;
	const std::string input;

public:
	int line_number = 1;
	int start_index = 0;

	Buffer(const std::string& input) : input {input}
	{
	}

	const char& getChar(int index)
	{
		return input[index];
	}

	const char& getTopChar()
	{
		return getChar(start_index);
	}

	~Buffer()
	{
		
	}
};