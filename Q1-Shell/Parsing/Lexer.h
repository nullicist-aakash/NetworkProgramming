#pragma once
#include <fstream>
#include <set>
#include <cstring>
#include <map>
#include <cassert>
#include <vector>

extern const char* LexerLoc;

enum class TokenType
{
	TK_TOKEN,
	TK_DAEMON,
	TK_PIPE,
	TK_HASH,
	TK_SS,
	TK_COMMA,
	TK_AND,
	TK_FG,
	TK_BG,
	TK_WHITESPACE,
	TK_ERROR_SYMBOL,
	TK_ERROR_PATTERN,
	TK_ERROR_LENGTH,
	TK_EXIT,
	TK_END,
	UNINITIALISED
};

struct Token
{
	TokenType type = TokenType::UNINITIALISED;
	std::string lexeme;
	int start_index = 0;
	int length = 0;

	friend std::ostream& operator<<(std::ostream&, const Token&);
};

struct DFA
{
	int num_tokens;
	int num_states;
	int num_transitions;
	int num_finalStates;
	int num_keywords;

	std::vector<std::vector<int>> productions;
	std::vector<TokenType> finalStates;
	std::vector<std::string> tokenType2tokenStr;
	std::map<std::string, TokenType> tokenStr2tokenType;
	std::map<std::string, TokenType> lookupTable;
	std::set<std::string> keywordTokens;

	DFA() : num_tokens{ 0 }, num_states{ 0 }, num_transitions{ 0 }, num_finalStates{ 0 }, num_keywords{ 0 }
	{

	}
};

class Buffer
{
private:
	const std::string input;

public:
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
};

extern DFA dfa;

void loadDFA();
Token* getNextToken(Buffer& b);