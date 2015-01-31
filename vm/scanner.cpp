#include "mathvm.h"
#include "scanner.h"
#include "ast.h"

#include <iostream>

namespace mathvm {

void TokenList::add(uint32_t position,
                    TokenKind kind,
                    const string& value) {
    _tokens.push_back(TokenInfo(kind, value, position));
}

uint32_t TokenList::positionOf(uint32_t index) const {
    if (index >= _tokens.size()) {
        return Status::INVALID_POSITION;
    }
    return _tokens[index]._position;
}

TokenKind TokenList::kindAt(uint32_t index) {
    if (index >= _tokens.size()) {
        return tEOF;
    }
    return _tokens[index]._kind;
}

const string TokenList::valueAt(uint32_t index) {
    if (index >= _tokens.size()) {
        return "";
    }
    return _tokens[index]._value;
}

void TokenList::dump() {
    for (size_t i = 0; i < _tokens.size(); i++) {
        cout << i << ": " << tokenStr(kindAt(i))
             << " " << valueAt(i) << endl;
    }
}

bool Scanner::isLetter(char ch) {
    return (('A' <= ch) && (ch <= 'Z')) ||
            (('a' <= ch) && (ch <= 'z')) ||
            (ch == '_');
}

bool Scanner::isDigit(char ch) {
    return '0' <= ch && ch <= '9';
}

bool Scanner::isWhitespace(char ch) {
    return (ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r');
}

void Scanner::readChar() {
    if (_position >= _maxPosition) {
        _ch = '\0';
        return;
    }
    _position++;
    _ch = (*_code)[_position];
}

char Scanner::lookAhead(int32_t offset) {
    if (_position + offset + 1 >= _maxPosition) {
        return '\0';
    }
    return (*_code)[_position + offset];
}

void Scanner::scanIdent() {
    int32_t tokenStart = _position;
    readChar();
    while (isLetter(_ch) || isDigit(_ch)) {
        readChar();
    }
    size_t len = _position - tokenStart;
    _tokens->add(tokenStart, tIDENT, _code->substr(tokenStart, len));
    _position--;
}

void Scanner::scanNumber() {
    int32_t tokenStart = _position;
    TokenKind kind = tINT;

    // Figure out if it is double or integer.
    int32_t pos = 0;
    char symbol;
    while (1) {
        symbol = lookAhead(pos++);
        if (isDigit(symbol)) {
            continue;
        }

        if (symbol == '.' && isDigit(lookAhead(pos))) {
            kind = tDOUBLE;
            continue;
        }
        if (symbol == 'e' &&
            (isDigit(lookAhead(pos)) ||
             ((lookAhead(pos) == '-' || lookAhead(pos) == '+')
              && isDigit(lookAhead(pos+1))))) {
            kind = tDOUBLE;
            continue;
        }
        break;
    }

    while (isDigit(_ch) ||
           (_ch == '.' && isDigit(lookAhead())) ||
           (_ch == 'e' && (isDigit(lookAhead()) || lookAhead() == '-' || lookAhead() == '+'))
        ) {
        if (_ch == 'e') {
            readChar();
        }
        readChar();
    }
    size_t len = _position - tokenStart;
    _tokens->add(tokenStart, kind, _code->substr(tokenStart, len));
    _position--;
}

char Scanner::unescape(char ch) {
    switch (ch) {
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case '\\':
        return '\\';
    case 't':
        return '\t';
    default:
        error("Unknown escape sequence \\%c", ch);
        return '\0';
    }
}

void Scanner::scanString() {
    int32_t tokenStart = _position + 1;
    string result;
    while (1) {
        readChar();
        if (_ch == '\\') {
            readChar();
            result.append(1, unescape(_ch));
        } else if (_ch == '\'' || _ch == '\0') {
            break;
        } else {
            result.append(1, _ch);
        }
    }
    _tokens->add(tokenStart, tSTRING, result);
}

Status* Scanner::scan(const string& code, TokenList& tokens) {
    _code = &code;
    _position = -1;
    _maxPosition = code.size();
    _tokenStart = -1;
    _tokens = &tokens;

    while (true) {
        readChar();

        if (_ch == 0) {
            break;
        }

        if (isWhitespace(_ch)) {
            continue;
        }

        if (isLetter(_ch)) {
            scanIdent();
            continue;
        }

        if (isDigit(_ch)) {
            scanNumber();
            continue;
        }

        if (_ch == '\'') {
            scanString();
            continue;
        }

        // Skip comment.
        if ((_ch == '/') && (lookAhead(1) == '/')) {
            while (_ch != 0 && _ch != '\n') {
                readChar();
            }
            continue;
        }

        TokenKind kind;
        switch (_ch) {
        case '(':
            kind = tLPAREN;
            break;
        case ')':
            kind = tRPAREN;
            break;
        case '=':
            if (lookAhead(1) == '=') {
                readChar();
                kind = tEQ;
            } else {
                kind = tASSIGN;
            }
            break;
        case '|':
            if (lookAhead(1) == '|') {
                readChar();
                kind = tOR;
            } else {
                kind = tAOR;
            }
            break;
        case '&':
            if (lookAhead(1) == '&') {
                readChar();
                kind = tAND;
            } else {
                kind = tAAND;
            }
            break;
        case '^':
            kind = tAXOR;
            break;
        case '<':
            if (lookAhead(1) == '=') {
                readChar();
                kind = tLE;
            } else {
                kind = tLT;
            }
            break;
        case '>':
            if (lookAhead(1) == '=') {
                readChar();
                kind = tGE;
            } else {
                kind = tGT;
            }
            break;
        case '!':
            if (lookAhead(1) == '=') {
                readChar();
                kind = tNEQ;
            } else {
                kind = tNOT;
            }
            break;
        case '+':
            if (lookAhead(1) == '=') {
                readChar();
                kind = tINCRSET;
            } else {
                kind = tADD;
            }
            break;
        case '-':
            if (lookAhead(1) == '=') {
                readChar();
                kind = tDECRSET;
            } else {
                kind = tSUB;
            }
            break;
        case '*':
            kind = tMUL;
            break;
        case '/':
            kind = tDIV;
            break;
        case '%':
            kind = tMOD;
            break;
        case ';':
            kind = tSEMICOLON;
            break;
        case ',':
            kind = tCOMMA;
            break;
        case '{':
            kind = tLBRACE;
            break;
        case '}':
            kind = tRBRACE;
            break;
        case '.':
            if (lookAhead(1) == '.') {
                readChar();
                kind = tRANGE;
            } else {
                kind = tUNDEF;
            }
            break;
        default:
            kind = tUNDEF;
        }

        if (kind != tUNDEF) {
          _tokens->add(_position, kind);
        } else {
          error("Bad token: %c", _ch);
        }
    }

    _tokens->add(_position, tEOF);

    //_tokens->dump();

    return Status::Ok();
}

void Scanner::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    verror(_position, format, args);
}

}
