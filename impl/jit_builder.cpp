#include <fstream>
#include "jit_builder.h"
#include "annotated_bytecode.h"
#include "building_context.h"

namespace mathvm {

    using namespace asmjit;
    using namespace asmjit::x86;

    JitBuilder JitBuilder::instance_;
    const X86GpReg JitBuilder::gp_args_regs[kGpArgsRegsCount] = { rdi, rsi, rdx, rcx, r8, r9 };
    const X86XmmReg JitBuilder::double_args_regs[kXmmArgsRegsCount] = { xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7 };

    void * JitBuilder::buildNativeProxy(Signature const &signature, const void *addr) {
        X86Assembler assembler(&runtime_);

        assembler.mov(r11, rdi); // first argument is a pointer to begin offset of arguments
        emitFunctionCall(assembler, signature, imm_ptr(addr), r11);

        assembler.ret();

        return assembler.make();
    }

    void * JitBuilder::buildProgram(Code &code, RuntimeEnvironment & runtime_environment) {
        X86Assembler assembler(&runtime_);

#ifdef JIT_LOG
        StringLogger logger;
        assembler.setLogger(&logger);
#endif

        BuildingContext buildingContext(code, assembler, runtime_environment);
        buildingContext.emitAll();

#ifdef JIT_LOG
        ofstream of("jit.log");
        of<< logger.getString() << endl;
        of.close();
#endif
        return assembler.make();
    }

}