#ifndef _MATHVM_SCANNER_H
#define _MATHVM_SCANNER_H

#include "mathvm.h"
#include "ast.h"

namespace mathvm {

class TokenList {
    struct TokenInfo {
        TokenKind _kind;
        string _value;
        uint32_t _position;

        TokenInfo(TokenKind kind,
                  const string& value,
                  uint32_t position) :
            _kind(kind), _value(value), _position(position) {
        }
    };
    vector<TokenInfo> _tokens;

  public:
    TokenList() {
    }
    void add(uint32_t position, TokenKind kind, const string& value = "");
    uint32_t positionOf(uint32_t index) const;
    TokenKind kindAt(uint32_t index);
    const string valueAt(uint32_t index);
    void dump();
};

class Scanner : ErrorInfoHolder {
    int32_t _position, _maxPosition;
    int32_t _tokenStart;
    const string* _code;
    char _ch;
    TokenKind _kind;
    TokenList* _tokens;

    static bool isLetter(char ch);
    static bool isDigit(char ch);
    static bool isWhitespace(char ch);

    void readChar();
    char lookAhead(int32_t offset = 1);
    void scanIdent();
    void scanNumber();
    void scanString();

    char unescape(char ch);
    
    void error(const char* msg, ...);

  public:
    Scanner() :
        _position(0), _maxPosition(0), _code(0) {
    }
    ~Scanner() {
    }
    Status* scan(const string& code, TokenList& tokens);
};

}

#endif // _MATHVM_SCANNER_H
