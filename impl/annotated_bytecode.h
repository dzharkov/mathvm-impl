#ifndef ANNOTATED_BYTECODE_H
#define ANNOTATED_BYTECODE_H

#include "mathvm.h"
#include "common.h"
#include <queue>
#include <algorithm>
#include <asmjit/asmjit.h>

namespace mathvm {
    using namespace asmjit;
    using namespace asmjit::x86;

    struct ContextVarDesc {
        index_t ctx_id;
        index_t var_id;
    };

    struct InstructionWithArgs {
        Instruction insn;

        union {
            insn_index_t transition_id;
            index_t index;
            ContextVarDesc context_var;
            double double_value;
            int64_t int_value;
        };
    };

    class AnnotatedBytecode {
    public:
        AnnotatedBytecode() {

        }

        AnnotatedBytecode(vector<InstructionWithArgs> && insns)
        : insns_(move(insns)){
            setBytecodeLength(insns_.size());
        }

        static AnnotatedBytecode annotate(BytecodeFunction & function, Code & code);

        index_t maxStackSize() const {
            return max_stack_size_;
        }

        index_t stackSizeAt(insn_index_t index) {
            return (index_t)stacks_[index].size();
        }

        VarType stackTypeAt(insn_index_t index, index_t slot_index) {
            return stacks_[index][slot_index];
        }

        index_t gpArgsOnStackBefore(insn_index_t index, index_t slot_index) {
            index_t result = 0;
            for (size_t i = 0; i < slot_index; i++) {
                if (stacks_[index][i] == VT_INT) {
                    result++;
                }
            }

            return result;
        }

        index_t xmmArgsOnStackBefore(insn_index_t index, index_t slot_index) {
            index_t result = 0;
            for (size_t i = 0; i < slot_index; i++) {
                if (stacks_[index][i] == VT_DOUBLE) {
                    result++;
                }
            }

            return result;
        }

        bool usedAsLabel(insn_index_t index) {
            return used_as_label_[index];
        }

        index_t locals_number() const {
            return locals_number_;
        }

        void setUsedAsLabel(insn_index_t index) {
            used_as_label_[index] = true;
        }

        void addTransition(insn_index_t from, insn_index_t to, index_t popped, vector<VarType> & pushed) {
            stacks_[to] = stacks_[from];

            while (popped--) {
                stacks_[to].pop_back();
            }

            stacks_[to].insert(stacks_[to].end(), pushed.begin(), pushed.end());
            max_stack_size_ = max((index_t) stacks_[to].size(), max_stack_size_);
        }

        insn_index_t length() {
            return (insn_index_t) insns_.size();
        }

        Instruction getInsn(insn_index_t index) {
            return insns_[index].insn;
        }

        InstructionWithArgs getInsnWithArgs(insn_index_t index) {
            return insns_[index];
        }

        void setUsedClosure() {
            used_closure_ = true;
        }

        bool used_closure() {
            return used_closure_;
        }
    private:
        void setBytecodeLength(insn_index_t size) {
            used_as_label_.resize(size);
            stacks_.resize(size);
        }

        vector<InstructionWithArgs> insns_;
        index_t max_stack_size_ = 0;
        vector<bool> used_as_label_;
        vector<vector<VarType>> stacks_;
        index_t locals_number_;

        bool used_closure_ = false;
    };
}

#endif
