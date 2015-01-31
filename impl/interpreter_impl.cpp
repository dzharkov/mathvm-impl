#include "mathvm.h"
#include "interpreter_impl.h"

namespace mathvm { 

void InterpreterCodeImpl::disassemble(ostream& out, FunctionFilter* filter) {
    FunctionIterator it(this);
    while (it.hasNext()) {
        TranslatedFunction* function = it.next();
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
            out << "locals: " << function->localsNumber() << endl;

            function->disassemble(out);
        }
    }
}

}


