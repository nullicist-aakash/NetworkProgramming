#pragma once
#include "Lexer.h"
#include <vector>
#include <bitset>
#include <map>
#include <string>

class Parser
{
public:
	int num_non_terminals;
	int num_terminals;
	int start_index;

	std::vector<std::vector<int>> productions;
	std::vector<std::string> symbolType2symbolStr;
	std::map<std::string, int> symbolStr2symbolType;
	std::bitset<128> nullable;
	std::vector<std::bitset<128>> firstSet;
	std::vector<std::bitset<128>> followSet;
	std::vector<std::vector<int>> parseTable;

	Parser() : num_non_terminals{ 0 }, num_terminals{ 0 }, start_index{ 0 }
	{

	}

	void computeNullables();

	void computeFirstSets();

	void computeFollowSets();

	void computeParseTable();
};

extern Parser parser;

struct ParseTreeNode
{
	int symbol_index = 0;
	int parent_child_index = 0;
	int productionNumber = 0;

	Token* token = nullptr;

	int isLeaf = 0;
	ParseTreeNode* parent = nullptr;
	std::vector<ParseTreeNode*> children;

	friend std::ostream& operator<<(std::ostream&, const ParseTreeNode&);
};

void loadParser();

ParseTreeNode* parseInputSourceCode(Buffer&, bool&);