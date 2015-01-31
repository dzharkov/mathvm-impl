#ifndef JIT_BUILDER
#define JIT_BUILDER
#include "mathvm.h"
#include "common.h"
#include "asmjit/asmjit.h"
#include "runtime_environment.h"

namespace mathvm {
    using namespace asmjit;
    using namespace asmjit::x86;

    class JitBuilder {
    public:

        void *buildNativeProxy(Signature const & signature, const void * addr);
        template <typename T>
        static void emitFunctionCall(
                X86Assembler &assembler, Signature signature, T addr, asmjit::X86GpReg start_args = r11, int16_t offset = 0,
                bool is_rsp_odd = false
        ) {
            vector<index_t> gp_args;
            gp_args.reserve(kGpArgsRegsCount);
            vector<index_t> double_args;
            double_args.reserve(kXmmArgsRegsCount);
            vector<index_t> stack_args;
            stack_args.reserve(signature.size());

            for (index_t i = 1; i < signature.size(); ++i) {
                switch(signature[i].first) {
                    case VT_DOUBLE:
                        if (double_args.size() < kXmmArgsRegsCount) {
                            double_args.push_back(offset + i);
                        } else {
                            stack_args.push_back(offset + i);
                        }

                        break;
                    case VT_STRING:
                    case VT_INT:
                        if (gp_args.size() < kGpArgsRegsCount) {
                            gp_args.push_back(offset + i);
                        } else {
                            stack_args.push_back(offset + i);
                        }
                        break;
                    default:
                        throw runtime_error("unexpected type in native");
                }
            }

            // rule of thumb: (used stack size + 8 % 16) == 0, i.e. (used stack slots & 1) != 0
            size_t stack_delta = (stack_args.size() + !((stack_args.size() + is_rsp_odd) & 1)) * sizeof(word_t);

            for (size_t i = 0; i < gp_args.size(); ++i) {
                assembler.mov(gp_args_regs[i], ptr(start_args, gp_args[i] * sizeof(word_t)));
            }

            for (size_t i = 0; i < double_args.size(); ++i) {
                assembler.movsd(double_args_regs[i], ptr(start_args, double_args[i] * sizeof(word_t)));
            }

            for (size_t i = 0; i < stack_args.size(); ++i) {
                assembler.mov(rax, ptr(start_args, stack_args[i] * sizeof(word_t)));
                assembler.mov(ptr(rsp, i * sizeof(word_t) - stack_delta), rax);
            }

            assembler.sub(rsp, imm(stack_delta));

            assembler.call(addr);

            assembler.add(rsp, imm(stack_delta));
        }
        void * buildProgram(Code & code, RuntimeEnvironment & runtime_environment);
        static asmjit::Imm imm_ptr(const void * ptr) {
            return asmjit::imm(reinterpret_cast<int64_t >(ptr));
        }


        static JitBuilder & instance() {
            return instance_;
        }

        static const constexpr index_t kGpArgsRegsCount = 6;
        static const constexpr index_t kXmmArgsRegsCount = 8;
        static const X86GpReg gp_args_regs[kGpArgsRegsCount];
        static const X86XmmReg double_args_regs[kXmmArgsRegsCount];

    private:
        JitBuilder() {}

        asmjit::JitRuntime runtime_;
        static JitBuilder instance_;

    };

}
#endif //JIT_BUILDER
