#ifndef BUILDING_CONTEXT_H
#define BUILDING_CONTEXT_H

#include "asmjit/asmjit.h"
#include "annotated_bytecode.h"
#include "runtime_environment.h"
#include "jit_builder.h"

namespace mathvm {
#pragma GCC visibility push(hidden)
    class BuildingContext {
    public:

        BuildingContext(Code & code, X86Assembler & assembler, RuntimeEnvironment & runtime_environment)
        : code_(code), assembler_(assembler), runtime_environment_(runtime_environment)
        {

        }

        void emitAll();
    private:
        void emitFunctionCode(
                BytecodeFunction & function
        );

        void spillArguments(Signature const & signature);

        asmjit::X86Mem memoryStackSlotLocationFromTos(index_t slot_offset_from_tos) {
            return memoryStackSlotLocation(getStackSlotByOffsetFromTos(slot_offset_from_tos));
        }

        index_t getStackSlotByOffsetFromTos(index_t slot_offset_from_tos) {
            return annotated_bytecode_.stackSizeAt(current_insn_index_) - slot_offset_from_tos;
        }

        asmjit::X86Mem memoryStackSlotLocation(index_t stack_slot) {
            return ptr(rsp,
                    stack_slot * sizeof(word_t),
                    sizeof(word_t)
            );
        }

        static index_t gpStackRegisterCount() {
            return sizeof(gp_stack_registers_) / sizeof(gp_stack_registers_[0]);
        }

        static index_t xmmStackRegisterCount() {
            return sizeof(xmm_stack_registers_) / sizeof(xmm_stack_registers_[0]);
        }

        Operand getStackSlotLocationByOffsetFromTos(index_t slot_offset_from_tos, bool next_insn = false) {
            insn_index_t insn_index = current_insn_index_ + (next_insn || !slot_offset_from_tos);
            index_t stack_slot = (annotated_bytecode_.stackSizeAt(insn_index) - max(slot_offset_from_tos, (index_t)1));

            return getStackSlotLocation(insn_index, stack_slot);
        }

        Operand getStackSlotLocation(insn_index_t insn_index, index_t stack_slot);

        void spillOrRestoreStack(bool next_insn, bool spill);

        template<typename T>
        void movToStackLocation(index_t slot_offset_from_tos, T source, bool next_insn = false) {
            auto stack_slot = getStackSlotLocationByOffsetFromTos(slot_offset_from_tos, next_insn);

            movFromTo(source, stack_slot);
        }

        template<typename T>
        void movFromStackLocation(index_t slot_offset_from_tos, T dst) {
            auto stack_slot = getStackSlotLocationByOffsetFromTos(slot_offset_from_tos);

            movFromTo(stack_slot, dst);
        }

        void loadVariable(index_t index) {
            loadVariable(varLocation(index));
        }

        void loadVariable(index_t ctx_id, index_t index) {
            loadVariable(varLocation(ctx_id, index));
        }

        void loadVariable(Operand varLocation) {
            auto stack_top = getStackSlotLocationByOffsetFromTos(0);

            if (stack_top.isReg()) {
                movFromTo(varLocation, stack_top);
            } else {
                movFromTo(varLocation, rax);
                movFromTo(rax, stack_top);
            }
        }

        void storeVariable(index_t index) {
            storeVariable(varLocation(index));
        }

        void storeVariable(index_t ctx_id, index_t index) {
            storeVariable(varLocation(ctx_id, index));
        }

        void storeVariable(Operand varLocation) {
            auto stack_top = getStackSlotLocationByOffsetFromTos(1);

            if (stack_top.isReg()) {
                movFromTo(stack_top, varLocation);
            } else {
                movFromTo(stack_top, rax);
                movFromTo(rax, varLocation);
            }
        }

        X86Mem varLocation(index_t ctx_id, index_t index) {
            assembler_.mov(r11, imm_ptr(runtime_environment_.pointerToBspOfLastCall(ctx_id)));
            assembler_.mov(r13, ptr(r11));
            return ptr(r13, index * sizeof(word_t));
        }

        X86Mem varLocation(index_t index) {
            return ptr(rsp, (annotated_bytecode_.maxStackSize() + index) * sizeof(word_t), 8);
        }

        void emitDoubleOperation();
        void emitIntegerOperation();
        void emitBinaryOperation(uint32_t opcode, Reg tmp_register, bool need_store_result = true);

        void emitConditionalJump(bool is_xmm);
        template<typename T>
        void emitFunctionCall(vector<VarType> signature, T addr);
        template<typename T>
        void emitFunctionCall(Signature const & signature, T addr);
        template <typename T>
        void emitFunctionCallByAddr(vector<VarType> signature, T addr);

        bool isConiditionalJump(Instruction insn) {
            return insn >= BC_IFICMPNE && insn <= BC_IFICMPLE;
        }

        void generateCmpInsn();
        void emitCmpInsnByOpcode(uint32_t opcode, Reg const & tmp_reg);
        bool tryEmitNotPattern();
        bool tryEmitComparisionPattern(bool is_xmm);

        bool isRegistersReadyForCall(bool isNotVoid) {
            index_t tos_stack_slot = annotated_bytecode_.stackSizeAt(current_insn_index_);
            if (tos_stack_slot == 0) return true;

            return annotated_bytecode_.gpArgsOnStackBefore(current_insn_index_, tos_stack_slot) < JitBuilder::kGpArgsRegsCount &&
                   annotated_bytecode_.xmmArgsOnStackBefore(current_insn_index_, tos_stack_slot) < JitBuilder::kXmmArgsRegsCount &&
                   annotated_bytecode_.stackSizeAt(current_insn_index_ + 1) == isNotVoid;
        }

        template <typename T>
        void emitFastFunctionCall(T addr) {
            if (!is_rsp_odd_) {
                assembler_.sub(rsp, 8);
            }

            if (annotated_bytecode_.gpArgsOnStackBefore(current_insn_index_, annotated_bytecode_.stackSizeAt(current_insn_index_)) > 2) {
                assembler_.mov(JitBuilder::gp_args_regs[2], gp_stack_registers_[2]);
            }

            assembler_.call(addr);

            if (!is_rsp_odd_) {
                assembler_.add(rsp, 8);
            }
        }

        InstructionWithArgs getCurrentInsnWithArg() {
            return annotated_bytecode_.getInsnWithArgs(current_insn_index_);
        }

        Instruction getCurrentInsn() {
            return annotated_bytecode_.getInsnWithArgs(current_insn_index_).insn;
        }

        void movFromTo(Operand src, Operand dst);

        static const X86GpReg gp_stack_registers_[7];
        static const X86XmmReg xmm_stack_registers_[14];

        Code & code_;
        X86Assembler & assembler_;
        RuntimeEnvironment & runtime_environment_;
        AnnotatedBytecode annotated_bytecode_;
        insn_index_t current_insn_index_;

        asmjit::Label magic_label_;
        vector<asmjit::Label> functions_labels_;
        vector<asmjit::Label> all_labels_;
        vector<pair<asmjit::Label, double>> double_constants_;

        bool is_rsp_odd_ = false;
    };

#pragma GCC visibility pop
}

#endif
