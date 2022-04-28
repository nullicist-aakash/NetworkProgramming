#pragma once
#include "Parser.h"
#include <vector>

struct TypeLog;
std::ostream& operator<<(std::ostream&, const TypeLog&);

struct ASTNode
{
	int isLeaf = 0;
	int isGlobal = 0;
	int sym_index = 0;

	TypeLog* derived_type = nullptr;
	Token* token = nullptr;
	ASTNode* type = nullptr;
	std::vector<ASTNode*> children;
	ASTNode* sibling = nullptr;

	friend std::ostream& operator<<(std::ostream&, const ASTNode&);
};

ASTNode* createAST(const ParseTreeNode*, const ParseTreeNode* parent = nullptr, ASTNode* inherited = nullptr);