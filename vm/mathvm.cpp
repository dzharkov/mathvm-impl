#include "mathvm.h"
#include "ast.h"

#include <stdio.h>

#include <iostream>

using namespace std;

namespace mathvm {

void Label::addRelocation(uint32_t bciOfRelocation) {
    _relocations.push_back(bciOfRelocation);
}

void Label::bind(uint32_t address, Bytecode* code) {
    assert(!isBound());
    if (code == 0) {
      code = _code;
    }
    assert(code != 0);
    _bci = address;
    for (uint32_t i = 0; i < _relocations.size(); i++) {
        uint32_t relocBci = _relocations[i];
        int32_t offset = offsetOf(relocBci);
        assert((int16_t)offset == offset);
        assert(code->getInt16(relocBci) == 0x1ead);
        code->setInt16(relocBci, offset);
    }
    _relocations.clear();
}

void Bytecode::dump(ostream& out) const {
    for (size_t bci = 0; bci < length();) {
        size_t length;
        Instruction insn = getInsn(bci);
        out << bci << ": ";
        const char* name = bytecodeName(insn, &length);
        switch (insn) {
            case BC_DLOAD:
                out << name << " " << getDouble(bci + 1);
                break;
            case BC_ILOAD:
                out << name << " " << getInt64(bci + 1);
                break;
            case BC_SLOAD:
                out << name << " @" << getUInt16(bci + 1);
                break;
            case BC_CALL:
            case BC_CALLNATIVE:
                out << name << " *" << getUInt16(bci + 1);
                break;
            case BC_LOADDVAR:
            case BC_STOREDVAR:
            case BC_LOADIVAR:
            case BC_STOREIVAR:
            case BC_LOADSVAR:
            case BC_STORESVAR:
                out << name << " @" << getUInt16(bci + 1);
                break;
            case BC_LOADCTXDVAR:
            case BC_STORECTXDVAR:
            case BC_LOADCTXIVAR:
            case BC_STORECTXIVAR:
            case BC_LOADCTXSVAR:
            case BC_STORECTXSVAR:
                out << name << " @" << getUInt16(bci + 1)
                    << ":" << getUInt16(bci + 3);
                break;
            case BC_IFICMPNE:
            case BC_IFICMPE:
            case BC_IFICMPG:
            case BC_IFICMPGE:
            case BC_IFICMPL:
            case BC_IFICMPLE:
            case BC_JA:
                out << name << " " << getInt16(bci + 1) + bci + 1;
                break;
          default:
                out << name;
        }
        out << endl;
        bci += length;
    }
}

void Bytecode::addBranch(Instruction insn, Label& target) {
    add((uint8_t)insn);
    if (target.isBound()) {
        addInt16(target.offsetOf(current()));
    } else {
        target.addRelocation(current());
        addInt16(0x1ead);
    }
}

Var::Var(VarType type, const string& name) :
    _type(type) {
    _name = string(name);
    switch (type) {
    case VT_DOUBLE:
        setDoubleValue(0.0);
        break;
    case VT_INT:
        setIntValue(0);
        break;
    case VT_STRING:
        setStringValue(0);
        break;
    default:
        assert(false);
    }
}

void Var::print() {
    switch (_type) {
    case VT_DOUBLE:
        cout << getDoubleValue();
        break;
    case VT_INT:
        cout << getIntValue();
        break;
    case VT_STRING:
        cout << getStringValue();
        break;
        default:
            assert(false);
    }
}

const string Code::empty_string;

Code::Code() {
    _constants.push_back(empty_string);
}

Code::~Code() {
    for (uint32_t i = 0; i < _functions.size(); i++) {
        delete _functions[i];
    }
}

uint16_t Code::addFunction(TranslatedFunction* function) {
    uint16_t id = _functions.size();
    _functions.push_back(function);
    _functionById[function->name()] = id;
    function->assignId(id);
    return id;
}

TranslatedFunction* Code::functionById(uint16_t id) const {
    if (id >= _functions.size()) {
        return 0;
    }
    return _functions[id];
}

TranslatedFunction* Code::functionByName(const string& name) const {
    FunctionMap::const_iterator it = _functionById.find(name);
    if (it == _functionById.end()) {
        return 0;
    }
    return functionById((*it).second);
}

uint16_t Code::makeStringConstant(const string& str) {
    ConstantMap::iterator it = _constantById.find(str);
    if (it != _constantById.end()) {
        return (*it).second;
    }
    uint16_t id = _constants.size();
    _constantById[str] = id;
    _constants.push_back(str);
    return id;
}

uint16_t Code::makeNativeFunction(const string& name, const Signature& signature, const void* address) {
    NativeMap::iterator it = _nativeById.find(name);
    if (it != _nativeById.end()) {
        return (*it).second;
    }
    uint16_t id = _natives.size();
    _nativeById[name] = id;
    _natives.push_back(NativeFunctionDescriptor(name, signature, address));
    return id;
}

const string& Code::constantById(uint16_t id) const {
    if (id >= _constants.size()) {
        assert(false);
        return _constants[0];
    }
    return _constants[id];
}

const void* Code::nativeById(uint16_t id,
                             const Signature** signature,
                             const string** name) const {
    if (id >= _natives.size()) {
        return 0;
    }
    const NativeFunctionDescriptor& nfd = _natives[id];
    *name = &nfd.name();
    *signature = &nfd.signature();
    return nfd.code();
}

void Code::disassemble(ostream& out, FunctionFilter* filter) {
    for (uint32_t i = 0; i < _functions.size(); i++) {
        TranslatedFunction* function = _functions[i];
        bool match = filter ? filter->matches(function) : true;
        if (match) {
            out << "function [" << function->id() << "] "
                << typeToName(function->returnType())
                << " " << function->name() << "(";
            for (uint32_t i = 0; i < function->parametersNumber(); i++) {
                out << typeToName(function->parameterType(i));
                if (i + 1 < function->parametersNumber()) {
                    out << ", ";
                }
            }
            out << ")" << endl;
            function->disassemble(out);
        }
    }
}

TranslatedFunction::TranslatedFunction(AstFunction* function) :
  _id(INVALID_ID),
  _locals(0), _params(function->parametersNumber()),
  _name(function->name()) {
    _signature.push_back(SignatureElement(function->returnType(), "return"));
    for (uint32_t i = 0; i < function->parametersNumber(); i++) {
        _signature.push_back(SignatureElement(function->parameterType(i), function->parameterName(i)));
  }
}

TranslatedFunction::TranslatedFunction(const string& name, const Signature& signature) :
    _id(INVALID_ID), _locals(0), _params(signature.size() - 1), _name(name),
    _signature(signature) {
}

TranslatedFunction::~TranslatedFunction() {
}

void ErrorInfoHolder::error(uint32_t position, const char* format, ...) {
    va_list args;
    va_start(args, format);
    verror(position, format, args);
}

void ErrorInfoHolder::verror(uint32_t position, const char* format, va_list args) {
    _position = position;
    vsnprintf(_msgBuffer, sizeof(_msgBuffer), format, args);
    va_end(args);
    throw this;
}

}
