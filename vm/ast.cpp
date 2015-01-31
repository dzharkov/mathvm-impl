#include "ast.h"
#include "visitors.h"

#include <iostream>

namespace mathvm {

#define VISIT_FUNCTION(type, name)           \
    void type::visit(AstVisitor* visitor) {  \
        visitor->visit##type(this);          \
    }

FOR_NODES(VISIT_FUNCTION)

#undef VISIT_FUNCTION

const string AstFunction::top_name = "<top>";
const string AstFunction::invalid = "<invalid>";

AstFunction::~AstFunction() {
  DeleteVisitor terminator;
  terminator.performDelete(node()->body());
  delete _function;
}

const string& AstFunction::name() const {
    return _function->name();
}

VarType AstFunction::returnType() const {
    return _function->returnType();
}

VarType AstFunction::parameterType(uint32_t index) const {
    return _function->parameterType(index);
}

const string& AstFunction::parameterName(uint32_t index) const {
    return _function->parameterName(index);
}

uint32_t AstFunction::parametersNumber() const {
    return _function->parametersNumber();
}

Scope* AstFunction::scope() const {
    return _function->body()->scope()->parent();
}

Scope::~Scope() {
  for (uint32_t i = 0; i < childScopeNumber(); i++) {
    delete childScopeAt(i);
  }

  for (VarMap::iterator it = _vars.begin();
       it != _vars.end(); ++it) {
    AstVar* var = (*it).second;
    delete var;
  }

  for (FunctionMap::iterator it = _functions.begin();
       it != _functions.end(); ++it) {
    AstFunction* function = (*it).second;
    delete function;
  }
}

bool Scope::declareVariable(const string& name, VarType type) {
    if (lookupVariable(name, false) != 0) {
        return false;
    }
   _vars[name] = new AstVar(name, type, this);
   return true;
}

bool Scope::declareFunction(FunctionNode* node) {
    if (lookupFunction(node->name()) != 0) {
        return false;
    }
   _functions[node->name()] = new AstFunction(node, this);
    return true;
}

AstVar* Scope::lookupVariable(const string& name, bool useParent) {
    AstVar* result = 0;
    VarMap::iterator it = _vars.find(name);
    if (it != _vars.end()) {
        result = (*it).second;
    }
    if (!result && useParent && _parent) {
        result = _parent->lookupVariable(name, useParent);
    }
    return result;
}

  AstFunction* Scope::lookupFunction(const string& name, bool useParent) {
    AstFunction* result = 0;
    FunctionMap::iterator it = _functions.find(name);
    if (it != _functions.end()) {
        result = (*it).second;
    }

    if (!result && useParent && _parent) {
      result = _parent->lookupFunction(name, useParent);
    }

    return result;
}

bool Scope::VarIterator::hasNext() {
    if (_it != _scope->_vars.end()) {
        return true;
    }
    if (!_includeOuter) {
        return false;
    }
    if (!_scope->_parent) {
        return false;
    }
    _scope = _scope->_parent;
    _it = _scope->_vars.begin();

    return hasNext();
}

AstVar* Scope::VarIterator::next() {
    if (!hasNext()) {
        return 0;
    }

    AstVar* next = (*_it).second;
    _it++;
    return next;
}


bool Scope::FunctionIterator::hasNext() {
    if (_it != _scope->_functions.end()) {
        return true;
    }
    if (!_includeOuter) {
        return false;
    }
    if (!_scope->_parent) {
        return false;
    }
    _scope = _scope->_parent;
    _it = _scope->_functions.begin();

    return hasNext();
}

AstFunction* Scope::FunctionIterator::next() {
    if (!hasNext()) {
        return 0;
    }

    AstFunction* next = (*_it).second;
    _it++;
    return next;
}

}
