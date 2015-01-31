#ifndef _MATHVM_JIT_H
#define _MATHVM_JIT_H

#include "mathvm.h"

namespace mathvm {

class MachCodeFunction;

class MachCodeImpl : public Code {
    void* _code;
    void* _data;
  public:
    MachCodeImpl();
    virtual ~MachCodeImpl();

    virtual Status* execute(vector<Var*>& vars);

    MachCodeFunction* functionByName(const string& name);
    MachCodeFunction* functionById(uint16_t id);
    void error(const char* format, ...);

    void setCode(void* code) { _code = code; }
    void setData(void* data) { _data = data; }
};

class MachCodeFunction : public TranslatedFunction {
public:
    MachCodeFunction(MachCodeImpl* owner, BytecodeFunction* bytecode);
    virtual ~MachCodeFunction();

    virtual Status* execute(vector<Var*>* vars = 0);
    virtual void disassemble(ostream& out) const;
};



}

#endif
