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
		(node.token ? node.token->lexeme : "") <<
		", type: ";

	if (node.derived_type)
		out << *(node.derived_type);
	else
		out << "(null)";

	out << " }";
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
		//input TK_FG
		node->children.resize(1);
		node->children[0] = createAST(input->children[0], input);
	}
	else if(input->productionNumber == 4)
	{
		// daemon TK_DAEMON
		node->isDaemon = 1;
	}
	else if(input->productionNumber == 5)
	{
		// daemon eps
		delete node;
		return nullptr;
	}
	else if(input->productionNumber == 6)
	{
		// redirect eps
		delete node;
		return nullptr;
	}
	else if(input->productionNumber == 7)
	{
		// isBackground TK_AND
		node->isBackground = 1;
	}
	else if(input->productionNumber == 8)
	{
		// isBackground eps
		delete node;
		return nullptr;
	}
	else if(input->productionNumber == 9)
	{
		// command cmd remainCmd
		if (input->children[1])
			node->token = copy_token(input->children[1]->children[0]->token);
		else
		{
			node->token = new Token;
			node->token->type = TokenType::TK_TOKEN;
		}
		
	}
	if (input->productionNumber == 0)
	{
		//<program> ===> <otherFunctions> <mainFunction>
		//<program>.treenode = createTreeNode(<otherFunctions>.treenode, <mainFunction>.treenode);

		node->children.resize(2);
		node->children[0] = createAST(input->children[0], input);
		node->children[1] = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 1)
	{
		//<mainFunction> ===> TK_MAIN <stmts> TK_END
		//<mainFunction>.treenode = createTreeNode("main",<stmts>.treenode);

		node->token = copy_token(input->children[0]->token);
		node->children.resize(3, nullptr);

		node->children[2] = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 2)
	{
		//<otherFunctions> ===> <function> <otherFunctions[1]>
		//<otherFunctions>.treenode = createTreeNodeList(head = <function>.treenode, tail = <otherFunctions>.treenode);

		delete node;
		ASTNode* func = createAST(input->children[0], input);
		func->sibling = createAST(input->children[1], input);
		return func;
	}
	else if (input->productionNumber == 3)
	{
		//<otherFunctions> ===> eps
		//<otherFunctions>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 4)
	{
		//<function> ===> TK_FUNID <input_par> <output_par> TK_SEM <stmts> TK_END
		//<function>.treenode = createTreeNode(TK_FUNID.value, <input_par>.treenode, <output_par>.treenode, <stmts>.treenode);

		node->children.resize(3);
		node->token = copy_token(input->children[0]->token);
		node->children[0] = createAST(input->children[1], input);
		node->children[1] = createAST(input->children[2], input);
		node->children[2] = createAST(input->children[4], input);
	}
	else if (input->productionNumber == 5)
	{
		//<input_par> ===> TK_INPUT TK_PARAMETER TK_LIST TK_SQL <parameter_list> TK_SQR
		//<input_par>.treenode = <parameter_list>.treenode;

		delete node;
		return createAST(input->children[4], input);
	}
	else if (input->productionNumber == 6)
	{
		//<output_par> ===> TK_OUTPUT TK_PARAMETER TK_LIST TK_SQL <parameter_list> TK_SQR
		//<output_par>.treenode = <parameter_list>.treenode;

		delete node;
		return createAST(input->children[4], input);
	}
	else if (input->productionNumber == 7)
	{
		//<output_par> ===> eps
		//<output_par>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 8)
	{
		//<parameter_list> ===> <dataType> TK_ID <remaining_list>
		//<parameter_list>.treenode = createTreeNodeList(head = createTreeNode(<dataType>.data, TK_ID.value), tail = <remaining_list>.treenode);

		node->token = copy_token(input->children[1]->token);	// Stores name of id
		node->type = createAST(input->children[0], input);
		node->sibling = createAST(input->children[2], input);
	}
	else if (input->productionNumber == 9)
	{
		//<dataType> ===> <primitiveDatatype>
		//<dataType>.treenode = <primitiveDatatype>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 10)
	{
		//<dataType> ===> <constructedDatatype>
		//<dataType>.treenode = <constructedDatatype>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 11)
	{
		//<primitiveDatatype> ===> TK_INT
		//<primitiveDatatype>.data = "int";

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 12)
	{
		//<primitiveDatatype> ===> TK_REAL
		//<primitiveDatatype>.data = "real"

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 13)
	{
		//<constructedDatatype> ===> TK_RECORD TK_RUID
		//<constructedDatatype>.data = TK_RUID.data;

		delete node;
		ASTNode* temp = createAST(input->children[0], input);
		temp->sibling = createAST(input->children[1], input);
		return temp;
	}
	else if (input->productionNumber == 14)
	{
		//<constructedDatatype> ===> TK_UNION TK_RUID
		//<constructedDatatype>.data = TK_RUID.data;

		delete node;
		ASTNode* temp = createAST(input->children[0], input);
		temp->sibling = createAST(input->children[1], input);
		return temp;
	}
	else if (input->productionNumber == 15)
	{
		//<constructedDatatype> == = > TK_RUID
		//<constructedDatatype>.data = TK_RUID.data;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 16)
	{
		//<remaining_list> ===> TK_COMMA <parameter_list>
		//<remaining_list>.treenode = <parameter_list>.treenode;

		delete node;
		return createAST(input->children[1], input);
	}
	else if (input->productionNumber == 17)
	{
		//<remaining_list> ===> eps
		//<remaining_list>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 18)
	{
		//<stmts> ===> <typeDefinitions> <declarations> <otherStmts> <returnStmt>
		//<stmts>.treenode = createTreeNode(<typeDefinitions>.treenode, <declarations>.treenode,<otherStmts>.treenode, <returnStmt>.treenode);

		node->children.resize(4);
		node->children[0] = createAST(input->children[0], input);
		node->children[1] = createAST(input->children[1], input);
		node->children[2] = createAST(input->children[2], input);
		node->children[3] = createAST(input->children[3], input);
	}
	else if (input->productionNumber == 19)
	{
		//<typeDefinitions> ===> <actualOrRedefined> <typeDefinitions>1
		//<typeDefinitions>.treenode = 	createTreeNodeList(head = <actualOrRedefined>.treenode, tail = <typeDefinitions>1.treenode);

		delete node;
		ASTNode* temp = createAST(input->children[0], input);
		temp->sibling = createAST(input->children[1], input);
		return temp;
	}
	else if (input->productionNumber == 20)
	{
		//<typeDefinitions> ===> eps
		//<typeDefinitions>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 21)
	{
		//<actualOrRedefined> ===> <typeDefinition>
		//<actualOrRedefined>.treenode = <typeDefinition>.treenode

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 22)
	{
		//<actualOrRedefined> ===> <definetypestmt>
		//<actualOrRedefined>.treenode = <definetypestmt>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 23)
	{
		//<typeDefinition> ===> TK_RECORD TK_RUID <fieldDefinitions> TK_ENDRECORD
		//<typeDefinition>.treenode = createTreeNode(TK_RUID, <fieldDefinitions>.treenode);
		//<typeDefinition>.data = "record"

		node->token = copy_token(input->children[0]->token);
		node->children.resize(2);
		node->children[0] = createAST(input->children[1], input);
		node->children[1] = createAST(input->children[2], input);
	}
	else if (input->productionNumber == 24)
	{
		//<typeDefinition> ===> TK_UNION TK_RUID <fieldDefinitions> TK_ENDUNION
		//<typeDefinition>.treenode = createTreeNode(TK_RUID, <fieldDefinitions>.treenode);
		//<typeDefinition>.data = "union"

		node->token = copy_token(input->children[0]->token);
		node->children.resize(2);
		node->children[0] = createAST(input->children[1], input);
		node->children[1] = createAST(input->children[2], input);
	}
	else if (input->productionNumber == 25)
	{
		//<fieldDefinitions> ===> <fieldDefinition>1 <fieldDefinition>2 <moreFields>
		//<fieldDefinitions>.treenode = createTreeNodeList( head = createTreeNode(<fieldDefinition>1.treenode, <fieldDefinition>2.treenode), tail = <moreFieds>.treenode);
	
		delete node;
		ASTNode* first = createAST(input->children[0], input);
		first->sibling = createAST(input->children[1], input);
		first->sibling->sibling = createAST(input->children[2], input);
		return first;
	}
	else if (input->productionNumber == 26)
	{
		//<fieldDefinition> ===> TK_TYPE <fieldType> TK_COLON TK_FIELDID TK_SEM
		//<fieldDefinition>.treenode = createTreeNode(<fieldType>.data, TK_FIELDID.value);
		node->token = copy_token(input->children[3]->token);
		node->type = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 27)
	{
		//<fieldType> ===> <primitiveDatatype>
		//<fieldType>.data = <primitiveDatatype>.data;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 28)
	{
		//<fieldType> ===> TK_RUID
		//<fieldType>.data = TK_RUID.data;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 29)
	{
		//<moreFields> ===> <fieldDefinition> <moreFields>
		//<moreFields>.treenode = createTreeNodeList(head = <fieldDefinition>.treenode, tail = moreFields.treenode);

		delete node;
		ASTNode* first = createAST(input->children[0], input);
		first->sibling = createAST(input->children[1], input);
		return first;
	}
	else if (input->productionNumber == 30)
	{
		//<moreFields> ===> eps
		//<moreFields>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 31)
	{
		//<declarations> ===> <declaration> <declarations>1
		//<declarations>.treenode = createTreeNodeList(head = <declaration>.treenode, tail = <declarations>1.treenode);

		delete node;
		ASTNode* first = createAST(input->children[0], input);
		first->sibling = createAST(input->children[1], input);
		return first;
	}
	else if (input->productionNumber == 32)
	{
		//<declarations> ===> eps
		//<declarations>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 33)
	{
		//<declaration> ===> TK_TYPE <dataType> TK_COLON TK_ID <global_or_not> TK_SEM
		//<declaration>.treenode = createTreeNode(<dataType>.data, TK_ID.value, <global_or_not>.isGlobal);

		node->type = createAST(input->children[1], input);
		node->token = copy_token(input->children[3]->token);
		ASTNode* isGlobal = createAST(input->children[4], input);
		node->isGlobal = isGlobal == nullptr ? 0 : 1;

		if (isGlobal)
			delete isGlobal;
	}
	else if (input->productionNumber == 34)
	{
		//<global_or_not> ===> TK_COLON TK_GLOBAL
		//<global_or_not>.isGlobal = true;

		node->isGlobal = 1;
	}
	else if (input->productionNumber == 35)
	{
		//<global_or_not> ===> eps
		//<global_or_not>.isGlobal = false;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 36)
	{
		//<otherStmts> ===> <stmt> <otherStmts>1
		//<otherStmts>.treenode = createTreeNodeList(head = <stmt>.treenode, tail = <otherStmts>1.treenode);

		delete node;
		ASTNode* first = createAST(input->children[0], input);
		first->sibling = createAST(input->children[1], input);
		return first;
	}
	else if (input->productionNumber == 37)
	{
		//<otherStmts> ===> eps
		//<otherStmts>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 38)
	{
		//<stmt> ===> <assignmentStmt>
		//<stmt>.treenode = <assignmentStmt>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 39)
	{
		//<stmt> ===> <iterativeStmt>
		//<stmt>.treenode = <iterativeStmt>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 40)
	{
		//<stmt> ===> <conditionalStmt>
		//<stmt>.treenode = <conditionalStmt>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 41)
	{
		//<stmt> ===> <ioStmt>
		//<stmt>.treenode = <ioStmt>.treenode

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 42)
	{
		//<stmt> ===> <funCallStmt>
		//<stmt>.treenode = <funCallStmt>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 43)
	{
		//<assignmentStmt> ===> <singleOrRecId> TK_ASSIGNOP <arithmeticExpression> TK_SEM
		//<assignmentStmt>.treenode = createTreeNode(<singleOrRecId>.treenode, <arithmeticExpression>.treenode;

		node->token = copy_token(input->children[1]->token);
		node->children.resize(2);
		node->children[0] = createAST(input->children[0], input);
		node->children[1] = createAST(input->children[2], input);
	}
	else if (input->productionNumber == 44)
	{
		//<oneExpansion> ===> TK_DOT TK_FIELDID
		//<oneExpansion>.treenode = createTreeNode(TK_FIELDID.value);

		node->children.resize(2);
		node->children[0] = createAST(input->children[0], input);
		node->children[1] = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 45)
	{
		//<moreExpansions> ===> <oneExpansion> <moreExpansions>1
		//<moreExpansions>.treenode = createTreeNodeList(head = <oneExpansion>.treenode, tail = <moreExpansions>1.treenode);

		delete node;
		ASTNode* oneExp = createAST(input->children[0], input);
		assert(oneExp->children.size() == 2);
		ASTNode* dot = oneExp->children[0];
		ASTNode* id = oneExp->children[1];
		delete oneExp;

		dot->children.resize(2);
		dot->children[0] = inherited;
		dot->children[1] = id;

		return createAST(input->children[1], input, dot);
	}
	else if (input->productionNumber == 46)
	{
		//<moreExpansions> ===> eps
		//<moreExpansions>.treeNode = nullptr;

		delete node;
		return inherited;
	}
	else if (input->productionNumber == 47)
	{
		//<singleOrRecId> ===> TK_ID <option_single_constructed>
		//<singleOrRecId>.treenode = createTreeNode(TK_ID.value, <option_single_constructed>.treenode);

		delete node;
		ASTNode* idNode = createAST(input->children[0], input);
		ASTNode* constructed = createAST(input->children[1], input, idNode);
		return constructed;
	}
	else if (input->productionNumber == 48)
	{
		//<option_single_constructed> ===> eps
		//<option_single_constructed>.treeNode = nullptr;

		delete node;
		return inherited;
	}
	else if (input->productionNumber == 49)
	{
		//<option_single_constructed> ===> <oneExpansion> <moreExpansions>
		//<option_single_constructed>.treenode = createTreeNodeList(head = <oneExpansion>.treenode, tail = <moreExpansions>1.treenode);

		delete node;
		ASTNode* oneExp = createAST(input->children[0], input);
		assert(oneExp->children.size() == 2);
		ASTNode* dot = oneExp->children[0];
		ASTNode* id = oneExp->children[1];
		delete oneExp;

		dot->children.resize(2);
		dot->children[0] = inherited;
		dot->children[1] = id;

		return createAST(input->children[1], input, dot);
	}
	else if (input->productionNumber == 50)
	{
		// <funCallStmt> ===> <outputParameters> TK_CALL TK_FUNID TK_WITH TK_PARAMETERS <inputParameters> TK_SEM
		// <funCallStmt>.treenode = createTreeNode(TK_FUNID.val, <outputParameters>.treenode, <inputParameters>.treenode)

		node->token = copy_token(input->children[2]->token);
		node->children.resize(2);
		node->children[0] = createAST(input->children[0], input);
		node->children[1] = createAST(input->children[5], input);
	}
	else if (input->productionNumber == 51)
	{
		// <outputParameters> ===> TK_SQL <idList> TK_SQR TK_ASSIGNOP
		// <outputParameters>.treenode = createTreeNode("<--", <idList>.treenode)

		delete node;
		return createAST(input->children[1], input);
	}
	else if (input->productionNumber == 52)
	{
		// <outputParameters> ===> eps
		// <outputParameters>.treenode = nullptr

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 53)
	{
		// <inputParameters> ===> TK_SQL <idList> TK_SQR
		// <inputParameters>.treenode = <idList>.treenode

		delete node;
		return createAST(input->children[1], input);
	}
	else if (input->productionNumber == 54)
	{
		// <iterativeStmt> ===> TK_WHILE TK_OP <booleanExpression> TK_CL <stmt> <otherStmts> TK_ENDWHILE
		// <iterativeStmt>.treenode = createTreeNode("while", <booleanExpressions>.treenode, createTreeNodeList(head = <stmt>.treenode, tail = <otherStmts>.treenode));

		node->token = copy_token(input->children[0]->token);
		node->children.resize(2);
		node->children[0] = createAST(input->children[2], input);
		node->children[1] = createAST(input->children[4], input);
		node->children[1]->sibling = createAST(input->children[5], input);
	}
	else if (input->productionNumber == 55)
	{
		//<conditionalStmt> == = > TK_IF TK_OP<booleanExpression> TK_CL TK_THEN<stmt><otherStmts><elsePart>
		//<conditionalStmt>.treenode = createTreeNode("if", <booleanExpression>.treenode, createTreeNodeList(head = <stmt>.treenode, tail = <otherStmts>.treenode), <elsePart>.treenode)
		
		node->token = copy_token(input->children[0]->token);
		node->children.resize(3);
		node->children[0] = createAST(input->children[2], input);
		node->children[1] = createAST(input->children[5], input);
		node->children[2] = createAST(input->children[7], input);
		node->children[1]->sibling = createAST(input->children[6], input);
	}
	else if (input->productionNumber == 56)
	{
		//<elsePart> ===> TK_ELSE <stmt> <otherStmts> TK_ENDIF
		//<elsePart>.treenode = createTreeNode("else", createTreeNodeList(head=<stmt>.treenode, tail=<otherStmts>.treenode)

		delete node;
		ASTNode* first = createAST(input->children[1], input);
		first->sibling = createAST(input->children[2], input);
		return first;
	}
	else if (input->productionNumber == 57)
	{
		//<elsePart> ===> TK_ENDIF
		//<elsePart>.treenode = nullptr

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 58)
	{
		//<ioStmt> == = > TK_READ TK_OP<var> TK_CL TK_SEM
		//<ioStmt>.treenode = createTreeNode("read", <var>.treenode)

		node->token = copy_token(input->children[0]->token);
		node->children.resize(1);
		node->children[0] = createAST(input->children[2], input);
	}
	else if (input->productionNumber == 59)
	{
		//<ioStmt> == = > TK_WRITE TK_OP<var> TK_CL TK_SEM
		//<ioStmt>.treenode = createTreeNode("write", <var>.treenode)

		node->token = copy_token(input->children[0]->token);
		node->children.resize(1);
		node->children[0] = createAST(input->children[2], input);
	}
	else if (input->productionNumber == 60)
	{
		// <arithmeticExpression> ===> <term> <expPrime>
		// <arithmeticExpression>.treenode = <expPrime>.syn
		// <expPrime>.inh = <term>.treenode

		delete node;
		ASTNode* termNode = createAST(input->children[0], input);
		ASTNode* expPrime = createAST(input->children[1], input, termNode);
		return expPrime;
	}
	else if (input->productionNumber == 61)
	{
		// <expPrime> ===> <lowPrecedenceOperators> <term> <expPrime[1]>
		// <expPrime[1]>.inh = createTreeNode(<lowPrecedenceOperators>.data, <expPrime>.inh, <term>.treenode)
		// <expPrime>.syn = <expPrime[1]>.syn

		delete node;

		ASTNode* op = createAST(input->children[0], input);
		ASTNode* term = createAST(input->children[1], input);

		op->children.resize(2);
		op->children[0] = inherited;
		op->children[1] = term;

		return createAST(input->children[2], input, op);
	}
	else if (input->productionNumber == 62)
	{
		// <expPrime> ===> eps
		// <expPrime>.syn = <expPrime>.inh

		delete node;
		return inherited;
	}
	else if (input->productionNumber == 63)
	{
		//<term> ===> <factor> <termPrime>
		//<term>.treenode = <termPrime>.syn
		//<termPrime>.inh = <factor>.treenode

		delete node;
		ASTNode* factorNode = createAST(input->children[0], input);
		ASTNode* termPrime = createAST(input->children[1], input, factorNode);
		return termPrime;
	}
	else if (input->productionNumber == 64)
	{
		//<termPrime> ===> <highPrecedenceOperators> <factor> <termPrime[1]>
		//<termPrime[1]>.treenode = createTreeNode(<highPrecedenceOperators>.data, <termPrime>.inh, <factor>.treenode)
		//<termPrime>.syn = <termPrime[1]>.syn

		delete node;
		ASTNode* op = createAST(input->children[0], input);
		ASTNode* term = createAST(input->children[1], input);

		op->children.resize(2);
		op->children[0] = inherited;
		op->children[1] = term;

		return createAST(input->children[2], input, op);
	}
	else if (input->productionNumber == 65)
	{
		//<termPrime> ===> eps
		//<termPrime>.syn = <termPrime>.inh

		delete node;
		return inherited;
	}
	else if (input->productionNumber == 66)
	{
		//<factor> ===> TK_OP <arithmeticExpression> TK_CL
		//<factor>.treenode = <arithmeticExpression>.treenode

		delete node;
		return createAST(input->children[1], input);
	}
	else if (input->productionNumber == 67)
	{
		//<factor> ===> <var>
		//<factor>.treenode = <var>.treenode

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 68)
	{
		//<highPrecedenceOperators> ===> TK_MUL
		//<highPrecedenceOperators>.data = "*";

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 69)
	{
		//<highPrecedenceOperators> ===> TK_DIV
		//<highPrecedenceOperators>.data = "/";

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 70)
	{
		//<lowPrecedenceOperators> ===> TK_PLUS
		//<lowPrecedenceOperators>.data = "+";

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 71)
	{
		//<lowPrecedenceOperators> ===> TK_MINUS
		//<lowPrecedenceOperators>.data = "-";

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 72)
	{
		//<booleanExpression> ===> TK_OP <booleanExpression>1 TK_CL <logicalOp> TK_OP <booleanExpression>2 TK_CL
		//<booleanExpression>.treenode = createTreeNode(<logicalOp>.data, <booleanExpression>1.treenode, <booleanExpression>2.treenode);

		delete node;
		ASTNode* op = createAST(input->children[3], input);
		op->children.resize(2);
		op->children[0] = createAST(input->children[1], input);
		op->children[1] = createAST(input->children[5], input);
		return op;
	}
	else if (input->productionNumber == 73)
	{
		//<booleanExpression> ===> <var>1 <relationalOp> <var>2

		delete node;
		ASTNode* op = createAST(input->children[1], input);
		op->children.resize(2);
		op->children[0] = createAST(input->children[0], input);
		op->children[1] = createAST(input->children[2], input);
		return op;
	}
	else if (input->productionNumber == 74)
	{
		//<booleanExpression> ===> TK_NOT TK_OP <booleanExpression> TK_CL
		//<booleanExpression>.treenode = createTreeNode("~", <booleanExpression>.treenode);

		delete node;
		ASTNode* op = createAST(input->children[0], input);
		op->children.resize(1);
		op->children[0] = createAST(input->children[2], input);
		return op;
	}
	else if (input->productionNumber == 75)
	{
		//<var> ===> <singleOrRecId>
		//<var>.treenode =  <singleOrRecId>.treenode;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 76)
	{
		//<var> ===> TK_NUM
		//<var>.treenode = createTreeNode(TK_NUM.value);

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 77)
	{
		//<var> ===> TK_RNUM
		//<var>.treenode = createTreeNode(TK_RNUM.value);

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 78)
	{
		//<logicalOp> ===> TK_AND
		//<logicalOp>.data=�&&&�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 79)
	{
		//<logicalOp> ===> TK_OR
		//<logicalOp>.data=�@@@�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 80)
	{
		//<relationalOp> ===> TK_LT
		//<relationalOp>.data=�<�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 81)
	{
		//<relationalOp> ===> TK_LE
		//<relationalOp>.data=�<=�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 82)
	{
		//<relationalOp> ===> TK_EQ
		//<relationalOp>.data=�==�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 83)
	{
		//<relationalOp> ===> TK_GT
		//<relationalOp>.data=�>�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 84)
	{
		//<relationalOp> ===> TK_GE
		//<relationalOp>.data=�>=�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 85)
	{
		//<relationalOp> ===> TK_NE
		//<relationalOp>.data=�!=�;

		delete node;
		return createAST(input->children[0], input);
	}
	else if (input->productionNumber == 86)
	{
		//<returnStmt> ===> TK_RETURN <optionalReturn> TK_SEM
		//<returnStmt>.treenode = <optionalReturn>.treenode;

		delete node;
		return createAST(input->children[1], input);
	}
	else if (input->productionNumber == 87)
	{
		//<optionalReturn> ===> TK_SQL <idList> TK_SQR
		//<optionalReturn>.treenode = <idList>.treenode;

		delete node;
		return createAST(input->children[1], input);
	}
	else if (input->productionNumber == 88)
	{
		//<optionalReturn> ===> eps
		//<optionalReturn>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 89)
	{
		//<idList> ===> TK_ID <more_ids>
		//<idList>.treenode = createTreeNodeList(head = TK_ID.value, tail = <more_ids>.treenode);

		node->token = copy_token(input->children[0]->token);
		node->sibling = createAST(input->children[1], input);
	}
	else if (input->productionNumber == 90)
	{
		//<more_ids> ===> TK_COMMA <idList>
		//<more_ids>.treenode = <idList>.treenode;

		delete node;
		return createAST(input->children[1], input);
	}
	else if (input->productionNumber == 91)
	{
		//<more_ids> ===> eps
		//<more_ids>.treenode = nullptr;

		delete node;
		return nullptr;
	}
	else if (input->productionNumber == 92)
	{
		//<definetypestmt> ===> TK_DEFINETYPE <A> TK_RUID1 TK_AS TK_RUID2
		//<definetypestmt>.treenode = createTreeNode(<A>.data, TK_RUID1.value ,TK_RUID2.value)

		node->token = copy_token(input->children[0]->token);
		node->children.resize(3);
		node->children[0] = createAST(input->children[1], input);
		node->children[1] = createAST(input->children[2], input);
		node->children[2] = createAST(input->children[4], input);
	}
	else if (input->productionNumber == 93)
	{
		// <A> ===> TK_RECORD
		// <A>.data = "record"

		node->token = copy_token(input->children[0]->token);
	}
	else if (input->productionNumber == 94)
	{
		// <A> ===> TK_UNION
		// <A>.data = "union"

		node->token = copy_token(input->children[0]->token);
	}

	return node;
}