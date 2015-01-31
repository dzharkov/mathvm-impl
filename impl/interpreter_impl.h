#ifndef INTERPRETER_IMPL 
#define INTERPRETER_IMPL 
#include "mathvm.h"
#include "common.h"

#include <map>
#include <string>
#include <stack>
#include <vector>
#include <exception>
#include <iostream>

namespace mathvm { 

class InterpreterCodeImpl : public Code {
public:
    Status* execute(vector<Var*>&) {
        return Status::Ok();
    }

    void disassemble(ostream& out, FunctionFilter* filter);

    void setNativeSource(index_t id, void const * addr) {
        if (source_natives_.size() <= id) {
            source_natives_.resize(id + 1);
        }
        source_natives_[id] = addr;
    }

    void const * getNativeSource(index_t id) {
        return source_natives_[id];
    }

private:
    vector<void const *> source_natives_;
};

}
#endif
