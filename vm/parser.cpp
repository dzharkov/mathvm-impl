#include "ast.h"
#include "mathvm.h"
#include "parser.h"

#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

namespace mathvm {

Parser::Parser() : _top(0),  _currentScope(0),  _topmostScope(0) {
}

Parser::~Parser() {
    delete _topmostScope;
}

Status* Parser::parseProgram(const string& code) {
    Scanner scanner;
    try {
      Status* s = scanner.scan(code, _tokens);
      if (s->isError()) {
        return s;
      }
      delete s;
    } catch (ErrorInfoHolder* error) {
      return Status::Error(error->getMessage(), error->getPosition());
    }
    return parseTopLevel();
}

AstFunction* Parser::top() const {
    return _top;
}

TokenKind Parser::currentToken() {
  if (_currentToken == tUNDEF) {
      _currentToken = _tokens.kindAt(_currentTokenIndex);
      if (_currentToken == tERROR) {
          error("parse error");
      }
    }
  return _currentToken;
}

TokenKind Parser::lookaheadToken(uint32_t count) {
    return _tokens.kindAt(_currentTokenIndex + count);
}

string Parser::currentTokenValue() {
    TokenKind kind = currentToken();
    if (kind == tIDENT || kind == tDOUBLE || kind == tINT || kind == tSTRING) {
        return _tokens.valueAt(_currentTokenIndex);
    }
    return "";
}

void Parser::consumeToken() {
    _currentToken = tUNDEF;
    _currentTokenIndex++;
}


void Parser::ensureToken(TokenKind token) {
  if (currentToken() != token) {
      error("'%s' expected, seen %s", tokenStr(token), tokenStr(currentToken()));
  }
  consumeToken();
}

void  Parser::ensureKeyword(const string& keyword) {
    if (currentToken() != tIDENT || currentTokenValue() != keyword) {
        error("keyword '%s' expected", currentTokenValue().c_str());
    }
    consumeToken();
}

void Parser::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    verror(tokenIndexToOffset(_currentTokenIndex), format, args);
}

uint32_t Parser::tokenIndexToOffset(uint32_t tokenIndex) const {
    return _tokens.positionOf(tokenIndex);
}

void Parser::pushScope() {
    Scope* newScope = new Scope(_currentScope);
    _currentScope->addChildScope(newScope);
    _currentScope = newScope;
}

void Parser::popScope() {
    Scope* parentScope = _currentScope->parent();
    assert(parentScope);
    _currentScope = parentScope;
}

Status* Parser::parseTopLevel() {
    BlockNode* topBlock = 0;
    _currentToken = tUNDEF;
    _currentTokenIndex = 0;
    _topmostScope = _currentScope = new Scope(0);
    _top = 0;
    try {
        topBlock = parseBlock(false);
    } catch (ErrorInfoHolder* error) {
      return Status::Error(error->getMessage(), error->getPosition());
    }
    assert(topBlock != 0);
    // Make pseudo-function for top level code.
    Signature signature;
    signature.push_back(SignatureElement(VT_VOID, "return"));
    _topmostScope->declareFunction(
      new FunctionNode(0, AstFunction::top_name,
                       signature, topBlock));
    _top = _topmostScope->lookupFunction(AstFunction::top_name);
    return Status::Ok();
}

static inline bool isAssignment(TokenKind token) {
  return (token == tASSIGN) ||
         (token == tINCRSET) ||
         (token == tDECRSET);
}

AstNode* Parser::parseStatement() {
    if (isKeyword(currentTokenValue())) {
      if (currentTokenValue() == "function") {
          return parseFunction();
      } else if (currentTokenValue() == "for") {
          return parseFor();
      } else if (currentTokenValue() == "if") {
          return parseIf();
      } else if (currentTokenValue() == "while") {
          return parseWhile();
      } else if (currentTokenValue() == "print") {
          return parsePrint();
      } else if (currentTokenValue() == "int") {
          return parseDeclaration(VT_INT);
      } else if (currentTokenValue() == "double") {
          return parseDeclaration(VT_DOUBLE);
      } else if (currentTokenValue() == "string") {
          return parseDeclaration(VT_STRING);
      } else if (currentTokenValue() == "return") {
          return parseReturn();
      } else {
          cout << "unhandled keyword " << currentTokenValue() << endl;
          assert(false);
          return 0;
      }
    }
    if (currentToken() == tLBRACE) {
       return parseBlock(true);
    }
    if ((currentToken() == tIDENT) && isAssignment(lookaheadToken(1))) {
        return parseAssignment();
    }
    return parseExpression();
}

CallNode* Parser::parseCall() {
    uint32_t tokenIndex = _currentTokenIndex;
    assert(currentToken() == tIDENT);
    const string& callee = currentTokenValue();
    consumeToken();
    ensureToken(tLPAREN);
    vector<AstNode*> args;
    while (currentToken() != tRPAREN) {
        args.push_back(parseExpression());
        if (currentToken() == tCOMMA) {
            consumeToken();
        } else {
            break;
        }
    }
    ensureToken(tRPAREN);

    return new CallNode(tokenIndex, callee, args);
}

StoreNode* Parser::parseAssignment() {
    assert(currentToken() == tIDENT);
    AstVar* lhs = _currentScope->lookupVariable(currentTokenValue());
    if (lhs == 0) {
        error("undeclared variable: %s", currentTokenValue().c_str());
    }
    consumeToken();

    TokenKind op = currentToken();
    if (op == tASSIGN ||
        op == tINCRSET ||
        op == tDECRSET) {
        consumeToken();
    } else {
        error("assignment expected");
    }

    StoreNode* result = new StoreNode(_currentTokenIndex,
                                      lhs,
                                      parseExpression(),
                                      op);

    if (currentToken() == tSEMICOLON) {
        consumeToken();
    }
    return result;
}

PrintNode* Parser::parsePrint() {
    uint32_t token = _currentToken;
    ensureKeyword("print");
    ensureToken(tLPAREN);

    PrintNode* result = new PrintNode(token);
    while (currentToken() != tRPAREN) {
        AstNode* operand = parseExpression();
        result->add(operand);
        if (currentToken() == tCOMMA) {
            consumeToken();
        }
    }
    ensureToken(tRPAREN);
    ensureToken(tSEMICOLON);
    return result;
}

static inline AstNode* defaultReturnExpr(VarType type) {
    switch (type) {
        case VT_INT:
            return new IntLiteralNode(0, 0);
        case VT_DOUBLE:
            return new DoubleLiteralNode(0, 0.0);
        case VT_STRING:
            return new StringLiteralNode(0, "");
        case VT_VOID:
            return 0;
      default:
            assert(false);
            return 0;
    }
}

FunctionNode* Parser::parseFunction() {
    uint32_t tokenIndex = _currentTokenIndex;
    ensureKeyword("function");

    if (currentToken() != tIDENT) {
        error("identifier expected");
    }
    const string& returnTypeName = currentTokenValue();
    VarType returnType = nameToType(returnTypeName);
    if (returnType == VT_INVALID) {
      error("wrong return type");
    }
    consumeToken();

    if (currentToken() != tIDENT) {
        error("name expected");
    }
    const string& name = currentTokenValue();
    consumeToken();

    Signature signature;
    signature.push_back(SignatureElement(returnType, "return"));

    ensureToken(tLPAREN);
    while (currentToken() != tRPAREN) {
        const string& parameterTypeName = currentTokenValue();
        VarType parameterType = nameToType(parameterTypeName);
        if (parameterType == VT_INVALID) {
            error("wrong parameter type");
        }
        consumeToken();
        const string& parameterName = currentTokenValue();
        if (currentToken() != tIDENT) {
            error("identifier expected");
        }
        consumeToken();
        signature.push_back(SignatureElement(parameterType, parameterName));

        if (currentToken() == tCOMMA) {
            consumeToken();
        }
    }
    ensureToken(tRPAREN);

    BlockNode* body = 0;
    pushScope();
    for (uint32_t i = 1; i < signature.size(); i++) {
      const string& name = signature[i].second;
      VarType type = signature[i].first;
      if (!_currentScope->declareVariable(name, type)) {
          error("Formal \"%s\" already declared", name.c_str());
      }
    }
    if ((currentToken() == tIDENT) && currentTokenValue() == "native") {
      consumeToken();
      if (currentToken() != tSTRING) {
          error("Native name expected, got %s", tokenStr(currentToken()));
      }
      pushScope();
      body = new BlockNode(_currentTokenIndex, _currentScope);
      body->add(new NativeCallNode(tokenIndex, currentTokenValue(), signature));
      consumeToken();
      ensureToken(tSEMICOLON);
      body->add(new ReturnNode(0, 0));
      popScope();
    } else {
      body = parseBlock(true);
      if (body->nodes() == 0 ||
          !(body->nodeAt(body->nodes() - 1)->isReturnNode())) {
        body->add(new ReturnNode(0, defaultReturnExpr(returnType)));
      }
    }
    popScope();

    if (_currentScope->lookupFunction(name) != 0) {
        error("Function %s already defined", name.c_str());
    }
    FunctionNode* result = new FunctionNode(tokenIndex, name, signature, body);
    _currentScope->declareFunction(result);

    // We don't add function node into AST.
    return 0;
}

ForNode* Parser::parseFor() {
    uint32_t token = _currentTokenIndex;
    ensureKeyword("for");
    ensureToken(tLPAREN);

    if (currentToken() != tIDENT) {
        error("identifier expected");
    }

    const string& varName = currentTokenValue();
    consumeToken();

    ensureKeyword("in");

    AstNode* inExpr = parseExpression();
    ensureToken(tRPAREN);

    BlockNode* forBody = parseBlock(true);
    AstVar* forVar = forBody->scope()->lookupVariable(varName);

    return new ForNode(token, forVar, inExpr, forBody);
}

WhileNode* Parser::parseWhile() {
    uint32_t token = _currentTokenIndex;
    ensureKeyword("while");
    ensureToken(tLPAREN);
    AstNode* whileExpr = parseExpression();
    ensureToken(tRPAREN);

    BlockNode* loopBlock = parseBlock(true);

    return new WhileNode(token, whileExpr, loopBlock);
}

IfNode* Parser::parseIf() {
    uint32_t token = _currentTokenIndex;
    ensureKeyword("if");
    ensureToken(tLPAREN);
    AstNode* ifExpr = parseExpression();
    ensureToken(tRPAREN);

    BlockNode* thenBlock = parseBlock(true);
    BlockNode* elseBlock = 0;
    if (currentTokenValue() == "else") {
        consumeToken();
        elseBlock = parseBlock(true);
    }

    return new IfNode(token, ifExpr, thenBlock, elseBlock);
}

ReturnNode* Parser::parseReturn() {
    uint32_t token = _currentTokenIndex;
    ensureKeyword("return");

    AstNode* returnExpr = 0;
    if (currentToken() != tSEMICOLON) {
        returnExpr = parseExpression();
    }
    return new ReturnNode(token, returnExpr);
}

BlockNode* Parser::parseBlock(bool needBraces) {
    if (needBraces) {
        ensureToken(tLBRACE);
    }

    pushScope();

    BlockNode* block = new BlockNode(_currentTokenIndex,
                                     _currentScope);
    TokenKind sentinel = needBraces ? tRBRACE : tEOF;

     while (currentToken() != sentinel) {
         if (currentToken() == tSEMICOLON) {
             consumeToken();
             continue;
         }
         AstNode* statement = parseStatement();
         // Ignore statements that doesn't result in AST nodes, such
         // as variable or function declaration.
         if (statement != 0) {
             block->add(statement);
         }
     }

     popScope();

     if (needBraces) {
         ensureToken(tRBRACE);
     }

     return block;
}

AstNode* Parser::parseDeclaration(VarType type) {
    // Skip type.
    ensureToken(tIDENT);
    const string& var = currentTokenValue();
    if (!_currentScope->declareVariable(var, type)) {
      error("Variable %s already declared", var.c_str());
    }

    if (lookaheadToken(1) == tASSIGN) {
        return parseAssignment();
    }
    ensureToken(tIDENT);
    ensureToken(tSEMICOLON);
    return 0;
}

static inline bool isUnaryOp(TokenKind token) {
    return (token == tSUB) || (token == tNOT);
}

static inline bool isBinaryOp(TokenKind token) {
    return (token >= tADD && token <= tMOD) ||
           (token == tRANGE) ||
           (token >= tEQ && token <= tLE) ||
           (token == tAND  || token == tOR ||
            token == tAAND || token == tAOR || token == tAXOR);
}

AstNode* Parser::parseUnary() {
    if (isUnaryOp(currentToken())) {
        TokenKind op = currentToken();
        consumeToken();
        return new UnaryOpNode(_currentTokenIndex, op, parseUnary());
    } else if (currentToken() == tIDENT && lookaheadToken(1) == tLPAREN) {
        AstNode* expr = parseCall();
        return expr;
    } else if (currentToken() == tIDENT) {
        AstVar* var = _currentScope->lookupVariable(currentTokenValue());
        if (var == 0) {
            error("undeclared variable: %s", currentTokenValue().c_str());
        }
        LoadNode* result = new LoadNode(_currentTokenIndex, var);
        consumeToken();
        return result;
    } else if (currentToken() == tDOUBLE) {
        DoubleLiteralNode* result =
            new DoubleLiteralNode(_currentTokenIndex,
                                  parseDouble(currentTokenValue()));
        consumeToken();
        return result;
    } else if (currentToken() == tINT) {
        IntLiteralNode* result =
            new IntLiteralNode(_currentTokenIndex,
                               parseInt(currentTokenValue()));
        consumeToken();
        return result;
    } else if (currentToken() == tSTRING) {
        StringLiteralNode* result =
            new StringLiteralNode(_currentTokenIndex,
                                  currentTokenValue());
        consumeToken();
        return result;
    } else if (currentToken() == tLPAREN) {
        consumeToken();
        AstNode* expr = parseExpression();
        ensureToken(tRPAREN);
        return expr;
    } else {
        error("Unexpected token: %s", tokenStr(currentToken()));
        return 0;
    }
}

AstNode* Parser::parseBinary(int minPrecedence) {
    AstNode* left = parseUnary();
    int precedence = tokenPrecedence(currentToken());

    while (precedence >= minPrecedence) {
        while (tokenPrecedence(currentToken()) == precedence) {
            TokenKind op = currentToken();
            uint32_t op_pos = _currentTokenIndex;
            consumeToken();
            AstNode* right = parseBinary(precedence + 1);
            left = new BinaryOpNode(op_pos, op, left, right);
        }
        precedence--;
    }
    return left;
}

AstNode* Parser::parseExpression() {
    return parseBinary(tokenPrecedence(tOR));
}

int64_t Parser::parseInt(const string& str) {
    char* p;
    int64_t result = strtoll(str.c_str(), &p, 10);
    assert(*p == '\0');
    return result;
}

double Parser::parseDouble(const string& str) {
    char* p;
    double result = strtod(str.c_str(), &p);
    assert(*p == '\0');
    return result;
}

}
