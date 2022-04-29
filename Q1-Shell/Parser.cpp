#include "Parser.h"
#include <vector>
#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <stack>
#include <cassert>

const char* GrammarLoc = "grammar.txt";
using namespace std;

void Parser::computeNullables()
{
	nullable.reset();
	nullable.set(0);

	// Get BIT representation of productions
	vector<bitset<128>> productionBitset(productions.size());

	for (int i = 0; i < productions.size(); ++i)
		for (int j = 1; j < productions[i].size(); ++j)
			productionBitset[i].set(productions[i][j]);

	// Base case
	for (int i = 0; i < productions.size(); ++i)
		if (productions[i].size() == 2 && productions[i][1] == 0)
			nullable.set(productions[i][0]);

	// iterate untill no update
	bool isUpdated = true;
	while (isUpdated)
	{
		isUpdated = false;

		for (int i = 0; i < productions.size(); ++i)
		{
			// rhs is nullable and is not already captured
			if ((productionBitset[i] & nullable) == productionBitset[i] &&
				!nullable.test(productions[i][0]))
			{
				nullable.set(productions[i][0]);
				isUpdated = true;
			}
		}
	}

	cerr << "Nullables: " << endl;
	for (int i = 0; i < 128; ++i)
		if (nullable.test(i))
			cerr << "\t" << symbolType2symbolStr[i] << endl;
}

void Parser::computeFirstSets()
{
	firstSet.clear();
	firstSet.resize(symbolType2symbolStr.size());

	// Base - Add eps to first set
	for (int i = 0; i < firstSet.size(); ++i)
		if (nullable.test(i))
			firstSet[i].set(0);

	// Base - First of terminals
	for (int i = 0; i < num_terminals; ++i)
		firstSet[i].set(i);

	// Iterate untill no update
	bool isUpdated = true;
	while (isUpdated)
	{
		isUpdated = false;

		for (const auto &production: productions)
		{
			bitset<128> bits = firstSet[production[0]];

			for (int j = 1; j < production.size(); ++j)
			{
				bits |= firstSet[production[j]];

				if (!nullable.test(production[j]))
					break;
			}

			if ((bits ^ firstSet[production[0]]).any())
			{
				firstSet[production[0]] |= bits;
				isUpdated = true;
			}
		}
	}

	cerr << "First sets: " << endl;
	for (int i = 0; i < firstSet.size(); ++i)
	{
		cerr << "FIRST(" << symbolType2symbolStr[i] << ")\t { ";

		if (!firstSet[i].any())
		{
			cerr << " }\n";
			continue;
		}

		for (int j = 0; j < 128; ++j)
			if (firstSet[i].test(j))
				cerr << symbolType2symbolStr[j] << ", ";

		cerr << "\b\b }" << endl;
	}
}

void Parser::computeFollowSets()
{
	followSet.clear();
	followSet.resize(symbolType2symbolStr.size());

	// Iterate untill no update
	bool isUpdated = true;
	while (isUpdated)
	{
		isUpdated = false;

		for (const auto& production : productions)
		{
			for (int j = 1; j < production.size(); ++j)
			{
				if (production[j] < num_terminals)
					continue;

				bitset<128> bits = followSet[production[j]];

				for (int k = j + 1; k < production.size(); ++k)
				{
					bits |= firstSet[production[k]];

					if (!nullable.test(production[k]))
						break;

					if (k == production.size() - 1)
						bits |= followSet[production[0]];
				}

				if (j == production.size() - 1)
					bits |= followSet[production[0]];

				if ((bits ^ followSet[production[j]]).any())
				{
					followSet[production[j]] |= bits;
					isUpdated = true;
				}
			}
		}
	}

	// remove eps from follow set
	for (auto& follow : followSet)
		follow.reset(0);

	cerr << "Follow sets: " << endl;
	for (int i = 0; i < followSet.size(); ++i)
	{
		cerr << "FOLLOW(" << symbolType2symbolStr[i] << ")\t { ";

		if (!followSet[i].any())
		{
			cerr << " }\n";
			continue;
		}

		for (int j = 0; j < 128; ++j)
			if (followSet[i].test(j))
				cerr << symbolType2symbolStr[j] << ", ";

		cerr << "\b\b }" << endl;
	}
}

void Parser::computeParseTable()
{
	parseTable.clear();
	parseTable.resize(symbolType2symbolStr.size(), vector<int>(num_terminals, -1));

	for (int i = 0; i < productions.size(); ++i)
	{
		bitset<128> select;
		auto& production = productions[i];

		for (int j = 1; j < production.size(); ++j)
		{
			select |= firstSet[production[j]];

			if (!nullable.test(production[j]))
				break;

			if (j == production.size() - 1)
				select |= followSet[production[0]];
		}

		for (int j = 0; j < num_terminals; ++j)
		{
			if (!select.test(j))
				continue;

			cerr << "Symbol " << parser.symbolType2symbolStr[productions[i][0]] << " on " << parser.symbolType2symbolStr[j] << " will give ";
			
			cerr << parser.symbolType2symbolStr[productions[i][0]] << " ---> ";
			for (int j = 1; j < productions[i].size(); ++j)
				cerr << parser.symbolType2symbolStr[productions[i][j]] << " ";
			cerr << endl;
			
			parseTable[productions[i][0]][j] = i;
		}
	}

	// fill for sync sets
	for (int i = 0; i < parseTable.size(); ++i)
	{
		for (int j = 0; j < num_terminals; ++j)
		{
			if (parseTable[i][j] > -1)
				continue;

			if (followSet[i].test(j))
				parseTable[i][j] = -2;
		}
	}

	for (auto& keyword : dfa.keywordTokens)
	{
		int col = symbolStr2symbolType[keyword];

		assert(col > 0);

		for (auto& row : parseTable)
			if (row[col] == -1)
				row[col] = -2;
	}

	cerr << productions.size() << endl;
	for (auto &prod: productions)
	{
		cerr << parser.symbolType2symbolStr[prod[0]] << " -> ";
		for (int j = 1; j < prod.size(); ++j)
			cerr << parser.symbolType2symbolStr[prod[j]] << " ";
		cerr << "." << endl;
	}
}

Parser parser;

std::ostream& operator<< (std::ostream& out, const ParseTreeNode& node)
{
	if (node.parent == nullptr)
	{
		out << setw(30) << "----" << setw(15) << -1 << setw(30) << "----" << setw(15) << "-nan" << setw(30) << "ROOT" << setw(10) << "no" << setw(30) << "program";
		return out;
	}

	const string &A = node.isLeaf ? node.token->lexeme : "----";
	
	const string &E = node.parent == NULL ? "root" : parser.symbolType2symbolStr[node.parent->symbol_index];
	
	const string& G = node.isLeaf ? "----" : parser.symbolType2symbolStr[node.symbol_index];

	out << setw(30) << A << setw(30) << E << setw(30) << G;

	return out;
}

void loadParser()
{
	ifstream grammarReader{ GrammarLoc };
	assert(grammarReader);

	int num_productions;
	grammarReader >> parser.num_terminals >> parser.num_non_terminals >> num_productions >> parser.start_index;
	parser.symbolType2symbolStr.clear();
	parser.symbolType2symbolStr.resize(parser.num_terminals + parser.num_non_terminals);

	for (int i = 0; i < parser.symbolType2symbolStr.size(); ++i)
	{
		string BUFF;
		grammarReader >> BUFF;
		parser.symbolType2symbolStr[i] = BUFF;
		parser.symbolStr2symbolType[BUFF] = i;
	}

	parser.productions.clear();
	parser.productions.resize(num_productions);

	for (int i = 0; i < parser.productions.size(); i++)
	{
		string BUFF;
		std::getline(grammarReader >> std::ws, BUFF);

		while (BUFF.back() == ' ' || BUFF.back() == '\r' || BUFF.back() == '\n')
			BUFF.pop_back();

		for (size_t pos = 0; (pos = BUFF.find(" ")) != std::string::npos; BUFF.erase(0, pos + 1))
			parser.productions[i].push_back(
				parser.symbolStr2symbolType[BUFF.substr(0, pos)]);

		parser.productions[i].push_back(
			parser.symbolStr2symbolType[BUFF]);
	}

	parser.computeNullables();
	parser.computeFirstSets();
	parser.computeFollowSets();
	parser.computeParseTable();
}

void _pop(ParseTreeNode** node, stack<int>& s)
{
	assert((*node)->symbol_index == s.top());

	s.pop();

	while ((*node)->parent_child_index == (*node)->parent->children.size() - 1)
	{
		(*node) = (*node)->parent;
		if ((*node)->parent == nullptr)
			return;
	}

	(*node) = (*node)->parent->children[(*node)->parent_child_index + 1];

	assert((*node)->symbol_index == s.top());
}

void printStack(stack<int> st)
{
	stack<string> s;
	while (st.size() > 1)
	{
		s.push(parser.symbolType2symbolStr[st.top()]);
		st.pop();
	}

	while (!s.empty())
	{
		cerr << s.top() << " ";
		s.pop();
	}
	cerr << endl;
}

ParseTreeNode* parseInputSourceCode(Buffer& buffer, bool &isError)
{
	isError = false;
	stack<int> st;
	st.push(-1);
	st.push(parser.start_index);

	ParseTreeNode* parseTree = new ParseTreeNode;
	parseTree->symbol_index = st.top();

	ParseTreeNode* node = parseTree;
	Token* lookahead = getNextToken(buffer);

	while (st.top() != -1)
	{
		if (lookahead->type == TokenType::TK_ERROR_LENGTH)
		{
			isError = true;
			cerr << *lookahead << endl;

			lookahead = getNextToken(buffer);
			continue;
		}
		if (lookahead->type == TokenType::TK_ERROR_PATTERN)
		{
			isError = true;
			cerr << *lookahead << endl;

			lookahead = getNextToken(buffer);
			continue;
		}
		if (lookahead->type == TokenType::TK_ERROR_SYMBOL)
		{
			isError = true;
			cerr << *lookahead << endl;

			lookahead = getNextToken(buffer);
			continue;
		}

		int stack_top = st.top();
		int input_terminal = parser.symbolStr2symbolType[dfa.tokenType2tokenStr[(int)lookahead->type]];

		if (lookahead->type == TokenType::TK_FG || lookahead->type == TokenType::TK_BG || lookahead->type == TokenType::TK_SS || lookahead->type == TokenType::TK_EXIT)
		{
			if (stack_top != input_terminal && stack_top != parser.start_index)
			{
				lookahead->type = TokenType::TK_TOKEN;
				input_terminal = parser.symbolStr2symbolType[dfa.tokenType2tokenStr[(int)lookahead->type]];
			}
		}

		if (stack_top == -1)
			break;

		cerr << endl << "Stack config: ";
		printStack(st);
		cerr << "Input symbol: " << parser.symbolType2symbolStr[input_terminal] << endl;

		// if top of stack matches with input terminal (terminal at top of stack)
		if (stack_top == input_terminal)
		{
			cerr << "Top matched!!" << endl;
			node->isLeaf = 1;
			node->token = lookahead;
			_pop(&node, st);
			lookahead = getNextToken(buffer);
			continue;
		}

		const string &la_token = parser.symbolType2symbolStr[input_terminal];
		const string &lexeme = lookahead->lexeme;
		const string &expected_token = parser.symbolType2symbolStr[stack_top];

		// if top of stack is terminal but it is not matching with input look-ahead
		if (stack_top < parser.num_terminals)
		{
			isError = true;
			cerr << "\t\terror: The token " << la_token << " for lexeme " << lexeme << " does not match with the expected token " << expected_token << endl;
			cerr << "\t\terror: The token " << la_token << " for lexeme " << lexeme << " does not match with the expected token " << expected_token << endl;
			_pop(&node, st);
			continue;
		}

		// Here, top of stack is always non-terminal

		int production_number = parser.parseTable[stack_top][input_terminal];

		// if it is a valid production
		if (production_number >= 0)
		{
			cerr << "Expanding along: " << parser.symbolType2symbolStr[parser.productions[production_number][0]] << " ---> ";
			for (int j = 1; j < parser.productions[production_number].size(); ++j)
				cerr << parser.symbolType2symbolStr[parser.productions[production_number][j]] << " ";
			cerr << endl;

			const vector<int> &production = parser.productions[production_number];
			int production_size = parser.productions[production_number].size();
			node->productionNumber = production_number;

			// empty production
			if (production_size == 2 && production[1] == 0)
			{
				_pop(&node, st);
				continue;
			}

			st.pop();

			node->children.assign(production_size - 1, nullptr);

			for (size_t i = production_size - 1; i > 0; --i)
			{
				st.push(production[i]);

				// -1 here because production[0] is the start symbol
				node->children[i - 1] = new ParseTreeNode;

				node->children[i - 1]->parent = node;
				node->children[i - 1]->symbol_index = production[i];
				node->children[i - 1]->parent_child_index = i - 1;
			}

			node = node->children[0];
			continue;
		}

		// if the production is not found and neither it is in sync set
		if (production_number == -1)
		{
			isError = true;
			cerr << "\t\terror: Invalid token " << la_token << " encountered with value " << lexeme << " stack top " << expected_token << endl;
			lookahead = getNextToken(buffer);
			continue;
		}

		// left case is for sync set
		assert(production_number == -2);

		isError = true;
		cerr << "\t\terror: Invalid token " << la_token << " encountered with value " << lexeme << " stack top " << expected_token << endl;
		_pop(&node, st);
	}

	if (st.top() != -1 || lookahead->type != TokenType::TK_END)
		isError = true;

	if (!isError)
		cerr << "Input source code is syntactically correct." << endl;
	else
		cerr << "Input source code is syntactically incorrect" << endl;

	cerr << endl;

	return parseTree;
}