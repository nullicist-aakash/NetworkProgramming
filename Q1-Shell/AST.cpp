#include "AST.h"
#include <iostream>
#include <cassert>
using namespace std;


std::ostream& operator<<(std::ostream& out, const ASTNode& node)
{
	out <<
		"{ symbol: '" <<
		parser.symbolType2symbolStr[node.sym_index] <<
		"', lexeme: '" <<
		(node.token ? node.token->lexeme : "") 

	<< "' }";
	return out;
}

Token* copy_token(Token* input)
{
	assert(input != nullptr);
	assert(input->length > 0);

	Token* out = new Token;
	out->type = input->type;
	out->lexeme = input->lexeme;
	out->length = input->length;
	return out;
}

ASTNode* createAST(const ParseTreeNode* input, const ParseTreeNode* parent, ASTNode* inherited)
{
	assert(input != nullptr);

	if (input->isLeaf)
	{
		assert(input != nullptr && input->isLeaf);

		ASTNode* node = new ASTNode;
		node->sym_index = input->symbol_index;
		node->token = copy_token(input->token);
		return node;
	}

	ASTNode* node = new ASTNode;
	node->sym_index = input->symbol_index;

	if(input->productionNumber == 0)
	{
		//input -> daemon command redirect isBackground TK_END
		ASTNode* dem = createAST(input->children[0], input);
		ASTNode* bg = createAST(input->children[3],input);
		node->isDaemon = dem == nullptr ? false : true; 
		node->isBackground = bg == nullptr ? false : true;
		node->children.resize(1);
		node->children[0] = createAST(input->children[1], input);
		//TODO:: handle redirect

	}
	else if (input->productionNumber <= 3)
	{
		//input -> TK_FG
		node->children.resize(1);
		node->children[0] = createAST(input->children[0], input);
	}
	else if(input->productionNumber == 4)
	{
		// daemon -> TK_DAEMON
		node->isDaemon = 1;
	}
	else if(input->productionNumber == 5)
	{
		// daemon -> eps
		delete node;
		return nullptr;
	}
	else if(input->productionNumber == 6)
	{
		// redirect -> eps
		delete node;
		return nullptr;
	}
	else if(input->productionNumber == 7)
	{
		// isBackground -> TK_AND
		node->isBackground = 1;
	}
	else if(input->productionNumber == 8)
	{
		// isBackground -> eps
		delete node;
		return nullptr;
	}
	else if(input->productionNumber == 9)
	{
		// command -> cmd remainCmd
		
		node->children.resize(1);
		node->children[0] = createAST(input->children[0], input);
		if (input->children[1] && input->children[1]->children.size()>0)
		{
			node->token = copy_token(input->children[1]->children[0]->token);
			node->sibling = createAST(input->children[1], input);
		}
		else
		{
			node->token = new Token;
			node->token->type = TokenType::TK_TOKEN;
		}
	
	}
	else if (input->productionNumber <= 11)
	{
		// cmd -> TK_TOKEN args
		// args -> TK_TOKEN args
		node->token = copy_token(input->children[0]->token);
		node->sibling = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 12)
	{
		// args eps
		delete node;
		return nullptr;
	}
	else if (input->productionNumber <= 15)
	{
		// remainCmd -> TK_PIPE pipeCmd
		// remainCmd -> TK_HASH hashCmd
		// remainCmd -> TK_SS ssCmd
		delete node;
		return createAST(input->children[1], input);
		// node->children.resize(1);
		// node->children[0] = createAST(input->children[1], input);
		// node->sibling = createAST(input->children[1]->children[0], input);
	}
	else if (input->productionNumber == 16)
	{
		delete node;
		return nullptr;
	}
	else if(input->productionNumber == 17)
	{
		// pipeCmd -> cmd pipeRemain
		node->children.resize(1);
		node->children[0] = createAST(input->children[0], input);
		node->sibling = createAST(input->children[1], input);
	}
	else if(input->productionNumber == 20 || input->productionNumber == 24)
	{
		// hashCmd -> cmd hashRemain
		// ssCmd -> cmd ssRemain

		node->children.resize(1);
		if(input->children[1] && input->children[1]->children[0]->token && input->children[1]->children[0]->token->type==TokenType::TK_COMMA)
		{
			node->token = copy_token(input->children[1]->children[0]->token);
			node->children[0] = new ASTNode;
			node->children[0]->children.resize(1);
			node->children[0]->children[0] = createAST(input->children[0], input);
			node->children[0]->sibling = createAST(input->children[1], input);
		}
		else
		{
			node->children[0] = createAST(input->children[0], input);
			node->sibling = createAST(input->children[1], input);
		}

		// node->children.resize(1);
		// node->children[0] = createAST(input->children[0], input);
		// node->sibling = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 19 || input->productionNumber == 23 || input->productionNumber == 27)
	{
		// pipeRemain -> eps
		// hashRemain -> eps
		// ssRemain -> eps
		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 18)
	{
		// pipeRemain -> TK_PIPE cmd pipeRemain
		node->children.resize(1);
		node->children[0] = createAST(input->children[1], input);
		node->sibling = createAST(input->children[2], input);
	}
	else if (input->productionNumber == 21 || input->productionNumber == 25)
	{
		// hashRemain -> TK_HASH cmd hashRemain
		// ssRemain -> TK_SS cmd ssRemain
		node->children.resize(1);
		if(input->children[2] && input->children[2]->children.size()>1 && input->children[2]->children[0]->token && input->children[2]->children[0]->token->type==TokenType::TK_COMMA)
		{
			node->token = copy_token(input->children[2]->children[0]->token);
			node->children[0] = new ASTNode;
			node->children[0]->children.resize(1);
			node->children[0]->children[0] = createAST(input->children[1], input);
			node->children[0]->sibling = createAST(input->children[2], input);
		}
		else
		{
			node->children[0] = createAST(input->children[1], input);
			node->sibling = createAST(input->children[2], input);
		}
	}
	else if (input->productionNumber == 22 || input->productionNumber == 26)
	{
		// hashRemain -> TK_COMMA commaCmd
		// ssRemain -> TK_COMMA commaCmd
		delete node;
		return createAST(input->children[1], input);
		// node->children.resize(1);
		// node->children[0] = createAST(input->children[1], input);
		// node->children[0]->sibling = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 28)
	{
		// commaCmd -> cmd commaRemainder
		node->children.resize(1);
		node->children[0] = createAST(input->children[0], input);
		node->sibling = createAST(input->children[1], input);
		// delete node;
		// ASTNode* left = createAST(input->children[1], input);
		// ASTNode* cmd = createAST(input->children[0], input);
		// cmd->sibling = left;
		// return cmd;
	}
	else if (input->productionNumber == 29)
	{
		// commaRemainder -> TK_COMMA cmd commaRemainder
		node->children.resize(1);
		node->children[0] = createAST(input->children[1], input);
		node->sibling = createAST(input->children[2], input);
		// delete node;
		// ASTNode* left = createAST(input->children[2], input);
		// ASTNode* cmd = createAST(input->children[1], input);
		// cmd->sibling = left;
		// return cmd;
	}
	else if(input->productionNumber == 30)
	{
		// commaRemainder -> eps
		delete node;
		return nullptr;
	}
	return node;
}