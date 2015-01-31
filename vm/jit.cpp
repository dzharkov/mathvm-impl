#include "jit.h"
#include "mathvm.h"
#include "parser.h"
#include "visitors.h"

#include <asmjit/asmjit.h>

using namespace asmjit;
using namespace asmjit::x86;
using namespace std;

namespace mathvm {

MachCodeImpl::MachCodeImpl() : _code(0) {
}

MachCodeImpl::~MachCodeImpl() {
  (reinterpret_cast<JitRuntime*>(_data))->release(_code);
}

Status* MachCodeImpl::execute(vector<Var*>& vars) {
  int result = asmjit_cast<int (*)()>(_code)();
  cout << "returned " << result << endl;
  return Status::Ok();
}

MachCodeFunction* MachCodeImpl::functionByName(const string& name) {
    return dynamic_cast<MachCodeFunction*>(Code::functionByName(name));
}

MachCodeFunction* MachCodeImpl::functionById(uint16_t id) {
    return dynamic_cast<MachCodeFunction*>(Code::functionById(id));
}

void MachCodeImpl::error(const char* format, ...) {
}

class MachCodeGenerator : public AstVisitor {
    AstFunction* _top;
    MachCodeImpl* _code;
    JitRuntime _runtime;
    X86Assembler _;
public:
    MachCodeGenerator(AstFunction* top,
                      MachCodeImpl* code) :
    _top(top), _code(code), _(&_runtime) {
    }

    Status* generate();

#define VISITOR_FUNCTION(type, name)            \
    virtual void visit##type(type* node) {}

    FOR_NODES(VISITOR_FUNCTION)
#undef VISITOR_FUNCTION
};

Status* MachCodeGenerator::generate() {
  // Prologue.
  _.push(rbp);
  _.mov(rbp, rsp);

  // Return value.
  _.mov(rax, 239);

  // Epilogue.
  _.mov(rsp, rbp);
  _.pop(rbp);
  _.ret();

  _code->setCode(_.make());
  _code->setData(&_runtime);

  return Status::Ok();
}

Status* MachCodeTranslatorImpl::translateMachCode(const string& program,
                                                  MachCodeImpl* *result) {
  MachCodeImpl* code = 0;
  Parser parser;

  // Build an AST.
  Status* s = parser.parseProgram(program);
  if (s->isOk()) {
    delete s;
    code = new MachCodeImpl();
    MachCodeGenerator codegen(parser.top(), code);
    s = codegen.generate();
  }

  if (s->isError()) {
    *result = NULL;
    delete code;
  } else {
    *result = code;
  }
  return s;
}

MachCodeTranslatorImpl::MachCodeTranslatorImpl() {
}

MachCodeTranslatorImpl::~MachCodeTranslatorImpl() {
}

Status* MachCodeTranslatorImpl::translate(const string& program, Code* *result) {
    MachCodeImpl* code = 0;
    Status* status = translateMachCode(program, &code);

    if (status->isError()) {
        assert(code == 0);
        *result = 0;
        return status;
    }

    //code->disassemble();
    assert(code);
    *result = code;

    return status;
}

MachCodeFunction::MachCodeFunction(MachCodeImpl* owner, BytecodeFunction* bcfunc) :
  TranslatedFunction(bcfunc->name(), bcfunc->signature()) {
}

MachCodeFunction::~MachCodeFunction() {
}


Status* MachCodeFunction::execute(vector<Var*>* vars) {
  return Status::Ok();
}

void MachCodeFunction::disassemble(ostream& out) const {
}

}
