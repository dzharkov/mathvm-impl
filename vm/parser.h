#ifndef _MATHVM_PARSER_H
#define _MATHVM_PARSER_H

#include "mathvm.h"
#include "ast.h"
#include "scanner.h"

namespace mathvm {

// We implement simple top down parser.
class Parser : public ErrorInfoHolder {
    AstFunction* _top;
    Scope* _currentScope;
    Scope* _topmostScope;
    TokenList _tokens;
    TokenKind _currentToken;
    uint32_t _currentTokenIndex;

    void error(const char* msg, ...);
    TokenKind currentToken();
    TokenKind lookaheadToken(uint32_t count);
    string currentTokenValue();
    void consumeToken();
    void ensureToken(TokenKind token);
    void ensureKeyword(const string& keyword);

    void pushScope();
    void popScope();

    Status* parseTopLevel();
    AstNode* parseStatement();
    CallNode* parseCall();
    StoreNode* parseAssignment();
    AstNode* parseExpression();
    AstNode* parseUnary();
    AstNode* parseBinary(int minPrecedence);
    FunctionNode* parseFunction();
    PrintNode* parsePrint();
    ForNode* parseFor();
    WhileNode* parseWhile();
    IfNode*  parseIf();
    ReturnNode* parseReturn();
    BlockNode* parseBlock(bool needBraces);
    AstNode* parseDeclaration(VarType type);

    static int64_t parseInt(const string& str);
    static double  parseDouble(const string& str);
  public:
    Parser();
    ~Parser();

    Status* parseProgram(const string& code);
    AstFunction* top() const;
    const TokenList& tokens() const { return _tokens; }
    uint32_t tokenIndexToOffset(uint32_t tokenIndex) const;
};

}

#endif // _MATHVM_PARSER_H
