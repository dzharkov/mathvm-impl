
#include "asmjit/asmjit.h"
#include "building_context.h"
#include "jit_builder.h"
#include "interpreter_impl.h"

namespace mathvm {
    const X86GpReg BuildingContext::gp_stack_registers_[] = {
            rdi, rsi, r10, rcx, r8, r9, rbx
    };
    const X86XmmReg BuildingContext::xmm_stack_registers_[] = {
            xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13
    };

    vector<BytecodeFunction*> functionsFromCode(Code & code) {
        vector<BytecodeFunction*> result;
        Code::FunctionIterator it(&code);
        while (it.hasNext()) {
            result.push_back((BytecodeFunction *) it.next());
        }
        return result;
    }

    void analyzeClosureUsage(vector<AnnotatedBytecode> & bytecodes) {
        for (AnnotatedBytecode & b : bytecodes) {
            for (insn_index_t i = 0; i < b.length(); i++) {
                Instruction insn = b.getInsn(i);
                if (insn >= BC_LOADCTXDVAR && insn <= BC_STORECTXSVAR) {
                    index_t ctx_id = b.getInsnWithArgs(i).context_var.ctx_id;
                    bytecodes[ctx_id].setUsedClosure();
                }
            }
        }
    }

    void BuildingContext::emitAll() {
        vector<BytecodeFunction *> all_functions = functionsFromCode(code_);
        runtime_environment_.setFunctionsCount(all_functions.size());

        functions_labels_.resize(all_functions.size());
        for (size_t i = 0; i < all_functions.size(); i++) {
            functions_labels_[i] = asmjit::Label(assembler_);
        }

        magic_label_ = asmjit::Label(assembler_);

        vector<AnnotatedBytecode> annotated_bytecodes(all_functions.size());
        for (size_t i = 0; i < all_functions.size(); i++) {
            annotated_bytecodes[all_functions[i]->id()] = AnnotatedBytecode::annotate(*all_functions[i], code_);
        }
        analyzeClosureUsage(annotated_bytecodes);

        for (size_t i = 0; i < all_functions.size(); i++) {
            assembler_.bind(functions_labels_[i]);
            annotated_bytecode_ = annotated_bytecodes[i];
            emitFunctionCode(*all_functions[i]);
        }

        assembler_.bind(magic_label_);
        assembler_.dint64(-0x8000000000000000LL);

        for (auto & p : double_constants_) {
            assembler_.bind(p.first);
            assembler_.ddouble(p.second);
        }
    }

    void BuildingContext::emitFunctionCode(BytecodeFunction &function) {
        all_labels_.resize(annotated_bytecode_.length());

        for (insn_index_t i = 0; i < annotated_bytecode_.length(); i++) {
            all_labels_[i] = asmjit::Label(assembler_);
        }
                                   // stack + locals + prev ebp + rbx
        size_t stack_frame_size = (annotated_bytecode_.maxStackSize() + function.localsNumber() + 2) * sizeof(word_t);
        is_rsp_odd_ = (bool)(stack_frame_size % 16);

        assembler_.mov(ptr(rsp, -((int32_t) sizeof(word_t)), sizeof(word_t)), rbx);

        if (annotated_bytecode_.used_closure()) {
            assembler_.mov(rax, imm_ptr(runtime_environment_.pointerToBspOfLastCall(function.id())));
            assembler_.mov(r11, ptr(rax, 0, sizeof(word_t)));
            assembler_.mov(ptr(rsp, -16, sizeof(word_t)), r11);
        }

        assembler_.sub(rsp, imm(stack_frame_size));

        if (annotated_bytecode_.used_closure()) {
            assembler_.mov(r11, rsp);
            assembler_.add(r11, imm(annotated_bytecode_.maxStackSize() * sizeof(word_t)));
            assembler_.mov(ptr(rax, 0, sizeof(word_t)), r11);
        }

        spillArguments(function.signature());

        for (current_insn_index_ = 0; current_insn_index_ < annotated_bytecode_.length(); current_insn_index_++) {

            if (annotated_bytecode_.usedAsLabel(current_insn_index_)) {
                assembler_.bind(all_labels_[current_insn_index_]);
            }

            switch (getCurrentInsn()) {
                case BC_DLOAD: {
                    asmjit::Label label(assembler_);
                    double_constants_.push_back(make_pair(label, getCurrentInsnWithArg().double_value));
                    movToStackLocation(0, ptr(label, 0, sizeof(word_t)));
                    break;
                }
                case BC_ILOAD:
                    assembler_.mov(
                            rax,
                            imm(getCurrentInsnWithArg().int_value)
                    );
                    movToStackLocation(0, rax);

                    break;
                case BC_SLOAD:
                    assembler_.mov(
                            rax,
                            JitBuilder::imm_ptr(code_.constantById(getCurrentInsnWithArg().index).c_str())
                    );
                    movToStackLocation(0, rax);

                    break;
                case BC_DLOAD0:
                    assembler_.mov(
                            rax,
                            imm_ptr(const_cast<double*>(&RuntimeEnvironment::DOUBLE_ZERO))
                    );
                    movToStackLocation(0, ptr(rax, 0, sizeof(word_t)));
                    break;
                case BC_ILOAD0:
                    if (!tryEmitNotPattern()) {
                        movToStackLocation(0, imm(0L));
                    }
                    break;
                case BC_SLOAD0:
                    movToStackLocation(0, JitBuilder::imm_ptr(RuntimeEnvironment::EMPTY_STRING));
                    break;
                case BC_DLOAD1: {
                    assembler_.mov(
                            rax,
                            imm_ptr(const_cast<double*>(&RuntimeEnvironment::DOUBLE_ONE))
                    );
                    movToStackLocation(0, ptr(rax, 0, sizeof(word_t)));
                    break;
                }
                case BC_ILOAD1:
                    movToStackLocation(0, imm(1L));
                    break;
                case BC_DADD:
                case BC_DSUB:
                case BC_DMUL:
                case BC_DDIV:
                    emitDoubleOperation();
                    break;
                case BC_DCMP:
                case BC_ICMP:
                    generateCmpInsn();
                    break;
                case BC_IADD:
                case BC_ISUB:
                case BC_IMUL:
                case BC_IDIV:
                case BC_IMOD:
                case BC_IAOR:
                case BC_IAAND:
                case BC_IAXOR:
                    emitIntegerOperation();
                    break;
                case BC_DNEG: {
                    Operand operand = getStackSlotLocationByOffsetFromTos(1);

                    if (!operand.isReg()) {
                        movFromTo(operand, xmm15);
                        operand = xmm15;
                    }

                    assembler_.movsd(xmm14, ptr(magic_label_, 0, sizeof(word_t)));
                    assembler_.emit(kX86InstIdXorpd, operand, xmm14);

                    if (operand.getRegIndex() == xmm15.getRegIndex()) {
                        movToStackLocation(1, xmm15);
                    }

                    break;
                }
                case BC_INEG:
                    movFromStackLocation(1, rax);
                    assembler_.neg(rax);
                    movToStackLocation(1, rax);
                    break;
                case BC_IPRINT:
                    emitFunctionCallByAddr({VT_VOID, VT_INT}, &print_int);
                    break;
                case BC_SPRINT:
                    emitFunctionCallByAddr({VT_VOID, VT_INT}, &print_str);
                    break;
                case BC_DPRINT:
                    emitFunctionCallByAddr({VT_VOID, VT_DOUBLE}, &print_double);
                    break;
                case BC_I2D:
                    movFromStackLocation(1, rax);
                    assembler_.cvtsi2sd(xmm15, rax);
                    movToStackLocation(1, xmm15, true);
                    break;
                case BC_D2I:
                    movFromStackLocation(1, xmm14);
                    assembler_.cvttsd2si(rax, xmm14);
                    movToStackLocation(1, rax, true);
                    break;
                case BC_S2I:
                    break;
                case BC_POP:
                    break;
                case BC_LOADDVAR0:
                case BC_LOADIVAR0:
                case BC_LOADSVAR0:
                    loadVariable(0);
                    break;
                case BC_LOADDVAR1:
                case BC_LOADIVAR1:
                case BC_LOADSVAR1:
                    loadVariable(1);
                    break;
                case BC_LOADDVAR2:
                case BC_LOADIVAR2:
                case BC_LOADSVAR2:
                    loadVariable(2);
                    break;
                case BC_LOADDVAR3:
                case BC_LOADIVAR3:
                case BC_LOADSVAR3:
                    loadVariable(3);
                    break;
                case BC_LOADDVAR:
                case BC_LOADIVAR:
                case BC_LOADSVAR:
                    loadVariable(getCurrentInsnWithArg().index);
                    break;
                case BC_STOREDVAR0:
                case BC_STOREIVAR0:
                case BC_STORESVAR0:
                    storeVariable(0);
                    break;
                case BC_STOREDVAR1:
                case BC_STOREIVAR1:
                case BC_STORESVAR1:
                    storeVariable(1);
                    break;
                case BC_STOREDVAR2:
                case BC_STOREIVAR2:
                case BC_STORESVAR2:
                    storeVariable(2);
                    break;
                case BC_STOREDVAR3:
                case BC_STOREIVAR3:
                case BC_STORESVAR3:
                    storeVariable(3);
                    break;
                case BC_STOREDVAR:
                case BC_STORESVAR:
                case BC_STOREIVAR:
                    storeVariable(getCurrentInsnWithArg().index);
                    break;
                case BC_LOADCTXDVAR:
                case BC_LOADCTXIVAR:
                case BC_LOADCTXSVAR: {
                    index_t f_id = getCurrentInsnWithArg().context_var.ctx_id;
                    index_t var_id = getCurrentInsnWithArg().context_var.var_id;
                    loadVariable(f_id, var_id);
                    break;
                }
                case BC_STORECTXDVAR:
                case BC_STORECTXIVAR:
                case BC_STORECTXSVAR: {
                    index_t f_id = getCurrentInsnWithArg().context_var.ctx_id;
                    index_t var_id = getCurrentInsnWithArg().context_var.var_id;
                    storeVariable(f_id, var_id);
                    break;
                }
                case BC_JA: {
                    insn_index_t next_transition = getCurrentInsnWithArg().transition_id;
                    if (next_transition != current_insn_index_ + 1) {
                        assembler_.jmp(all_labels_[next_transition]);
                    }
                    break;
                }
                case BC_IFICMPE:
                case BC_IFICMPNE:
                case BC_IFICMPL:
                case BC_IFICMPLE:
                case BC_IFICMPG:
                case BC_IFICMPGE: {
                    emitBinaryOperation(kX86InstIdCmp, rax, false);
                    emitConditionalJump(false);
                    break;
                }

                case BC_CALL: {
                    index_t function_id = getCurrentInsnWithArg().index;
                    Signature signature = code_.functionById(function_id)->signature();

                    emitFunctionCall(signature, functions_labels_[function_id]);

                    break;
                }
                case BC_CALLNATIVE: {
                    index_t function_id = getCurrentInsnWithArg().index;

                    Signature const * signature;
                    string const * name;
                    code_.nativeById(function_id, &signature, &name);
                    void const * addr = reinterpret_cast<InterpreterCodeImpl&>(code_).getNativeSource(function_id);

                    emitFunctionCall(const_cast<Signature&>(*signature), JitBuilder::imm_ptr(addr));

                    break;
                }
                case BC_RETURN:
                    switch((function.signature())[0].first) {
                        case VT_DOUBLE:
                            movFromStackLocation(1, xmm0);
                            break;
                        case VT_INT:
                        case VT_STRING:
                            movFromStackLocation(1, rax);
                            break;
                        default:
                            break;
                    }

                    assembler_.add(rsp, imm(stack_frame_size));

                    assembler_.mov(rbx, ptr(rsp, -((int32_t) sizeof(word_t)), sizeof(word_t)));
                    if (annotated_bytecode_.used_closure()) {
                        assembler_.mov(r11, ptr(rsp, -16, sizeof(word_t)));
                        assembler_.mov(rcx, imm_ptr(runtime_environment_.pointerToBspOfLastCall(function.id())));
                        assembler_.mov(ptr(rcx), r11);
                    }

                    assembler_.ret();
                    break;
                default:
                    throw runtime_error("unsupported insn=" + to_string(getCurrentInsn()));
            }
        }
    }

    Operand BuildingContext::getStackSlotLocation(insn_index_t insn_index, index_t stack_slot) {
        switch(annotated_bytecode_.stackTypeAt(insn_index, stack_slot)) {
            case VT_STRING:
            case VT_INT: {
                index_t gp_stack_slot = annotated_bytecode_.gpArgsOnStackBefore(insn_index, stack_slot);
                if (gpStackRegisterCount() > gp_stack_slot) {
                    return gp_stack_registers_[gp_stack_slot];
                }
                break;
            }
            case VT_DOUBLE: {
                index_t xmm_stack_slot = annotated_bytecode_.xmmArgsOnStackBefore(insn_index, stack_slot);
                if (xmmStackRegisterCount() > xmm_stack_slot) {
                    return xmm_stack_registers_[xmm_stack_slot];
                }
                break;
            }
            default:
                throw runtime_error("Unexpected slot type");
        }

        return memoryStackSlotLocation(stack_slot);
    }


    void BuildingContext::emitDoubleOperation() {
        uint32_t opcode;
        switch(getCurrentInsn()) {
            case BC_DADD:
                opcode = kX86InstIdAddsd;
                break;
            case BC_DSUB:
                opcode = kX86InstIdSubsd;
                break;
            case BC_DMUL:
                opcode = kX86InstIdMulsd;
                break;
            case BC_DDIV:
                opcode = kX86InstIdDivsd;
                break;
            default:
                throw runtime_error("Unexpected float operation: " + to_string(getCurrentInsn()));
        }

        emitBinaryOperation(opcode, xmm15);
    }

    void BuildingContext::emitIntegerOperation() {
        auto first_operand = getStackSlotLocationByOffsetFromTos(1);
        auto second_operand = getStackSlotLocationByOffsetFromTos(2);

        if (getCurrentInsn() == BC_IDIV || getCurrentInsn() == BC_IMOD) {

            movFromTo(first_operand, rax);
            assembler_.cqo();
            assembler_.emit(kX86InstIdIdiv, second_operand);

            if (getCurrentInsn() == BC_IMOD) {
                movFromTo(rdx, second_operand);
            } else {
                movFromTo(rax, second_operand);
            }

            return;
        }

        uint32_t opcode;
        switch(getCurrentInsn()) {
            case BC_IADD:
                opcode = kX86InstIdAdd;
                break;
            case BC_ISUB:
                opcode = kX86InstIdSub;
                break;
            case BC_IMUL:
                opcode = kX86InstIdImul;
                break;
            case BC_IAOR:
                opcode = kX86InstIdOr;
                break;
            case BC_IAAND:
                opcode = kX86InstIdAnd;
                break;
            case BC_IAXOR:
                opcode = kX86InstIdXor;
                break;
            default:
                throw runtime_error("Unexpected float operation: " + to_string(getCurrentInsn()));
        }

        emitBinaryOperation(opcode, rax);
    }

    bool isCommutative(Instruction insn) {
        return insn == BC_IADD || insn == BC_DMUL || insn == BC_DADD || insn == BC_IMUL || insn == BC_IAOR ||
               insn == BC_IAAND || insn == BC_IAXOR;
    }

    void BuildingContext::emitBinaryOperation(uint32_t opcode, Reg tmp_register, bool need_store_result) {
        auto first_operand = getStackSlotLocationByOffsetFromTos(1);
        auto second_operand = getStackSlotLocationByOffsetFromTos(2);

        if (!first_operand.isReg() && !second_operand.isReg()) {
            movFromTo(first_operand, tmp_register);
            first_operand = tmp_register;
        }

        if (isCommutative(getCurrentInsn()) && second_operand.isReg()) {
            assembler_.emit(opcode, second_operand, first_operand);
        } else {
            assembler_.emit(opcode, first_operand, second_operand);
            if (need_store_result) {
                movFromTo(first_operand, second_operand);
            }
        }
    }

    void BuildingContext::emitConditionalJump(bool is_xmm) {
        if (tryEmitComparisionPattern(is_xmm)) {
            return;
        }

        InstructionWithArgs insnWithArgs = getCurrentInsnWithArg();
        asmjit::Label success_label = all_labels_.at(insnWithArgs.transition_id);
        X86InstId opcode;
        switch(insnWithArgs.insn) {
            case BC_IFICMPE:
                opcode = kX86InstIdJe;
                break;
            case BC_IFICMPNE:
                opcode = kX86InstIdJne;
                break;
            case BC_IFICMPL:
                opcode = is_xmm ? kX86InstIdJb : kX86InstIdJl;
                break;
            case BC_IFICMPLE:
                opcode = is_xmm ? kX86InstIdJbe : kX86InstIdJle;
                break;
            case BC_IFICMPG:
                opcode = is_xmm ? kX86InstIdJa : kX86InstIdJg;
                break;
            case BC_IFICMPGE:
                opcode = is_xmm ? kX86InstIdJae : kX86InstIdJge;
                break;
            default:
                throw runtime_error("Unexpected jump operation: " + to_string(insnWithArgs.insn));
        }

        assembler_.emit(opcode, success_label);
    }

    template <typename T>
    void BuildingContext::emitFunctionCallByAddr(vector<VarType> signature, T addr) {
        emitFunctionCall(signature, JitBuilder::imm_ptr(reinterpret_cast<void*>(addr)));
    }

    template <typename T>
    void BuildingContext::emitFunctionCall(vector<VarType> signature, T addr) {
        Signature sig;
        for (auto &p : signature) {
            sig.push_back(make_pair(p, ""));
        }
        emitFunctionCall(sig, addr);
    }

    template <typename T>
    void BuildingContext::emitFunctionCall(Signature const & signature, T addr) {
        int16_t offset = annotated_bytecode_.stackSizeAt(current_insn_index_) - signature.size();

        if (isRegistersReadyForCall(signature[0].first != VT_VOID)) {
            spillOrRestoreStack(true, true);
            emitFastFunctionCall(addr);
        } else {
            spillOrRestoreStack(false, true);
            JitBuilder::emitFunctionCall(assembler_, signature, addr, rsp, offset, is_rsp_odd_);
        }

        switch (signature[0].first) {
            case VT_DOUBLE:
                assembler_.movsd(memoryStackSlotLocationFromTos(signature.size() - 1), xmm0);
                break;
            case VT_INT:
            case VT_STRING:
                assembler_.mov(memoryStackSlotLocationFromTos(signature.size() - 1), rax);
                break;
            default:
                break;
        }

        spillOrRestoreStack(true, false);
    }

    void BuildingContext::spillArguments(Signature const & signature) {
        size_t double_args = 0;
        size_t stack_args = 0;
        size_t gp_args = 0;

        int32_t stack_args_offset = (annotated_bytecode_.maxStackSize() + annotated_bytecode_.locals_number() + 2) * sizeof(word_t);

        for (index_t i = 0; i < signature.size() - 1; ++i) {
            switch(signature[i + 1].first) {
                case VT_DOUBLE:
                    if (double_args < JitBuilder::kGpArgsRegsCount) {
                        assembler_.movsd(varLocation(i), JitBuilder::double_args_regs[double_args]);
                        double_args++;
                    } else {
                        assembler_.mov(rax, ptr(rsp, (stack_args_offset + stack_args) * sizeof(word_t), sizeof(word_t)));
                        assembler_.mov(varLocation(i), rax);
                        stack_args++;
                    }

                    break;
                case VT_STRING:
                case VT_INT:
                    if (gp_args < JitBuilder::kXmmArgsRegsCount) {
                        assembler_.mov(varLocation(i), JitBuilder::gp_args_regs[gp_args]);
                        gp_args++;
                    } else {
                        assembler_.mov(rax, ptr(rsp, (stack_args_offset + stack_args) * sizeof(word_t), sizeof(word_t)));
                        assembler_.mov(varLocation(i), rax);
                        stack_args++;
                    }
                    break;
                default:
                    throw runtime_error("unexpected type in native");
            }
        }
    }

    void BuildingContext::spillOrRestoreStack(bool next_insn, bool spill) {
        insn_index_t insn_index = current_insn_index_ + next_insn;
        for (index_t stack_slot = 0; stack_slot < annotated_bytecode_.stackSizeAt(insn_index); stack_slot++) {
            auto stack_slot_location = getStackSlotLocation(insn_index, stack_slot);
            if (stack_slot_location.isReg()) {
                if (spill) {
                    movFromTo(stack_slot_location, memoryStackSlotLocation(stack_slot));
                } else {
                    movFromTo(memoryStackSlotLocation(stack_slot), stack_slot_location);
                }
            }
        }
    }

    void BuildingContext::emitCmpInsnByOpcode(uint32_t opcode, Reg const & tmp_reg) {
        auto first_operand = getStackSlotLocationByOffsetFromTos(2);
        if (!first_operand.isReg()) {
            movFromStackLocation(2, tmp_reg);
            first_operand = tmp_reg;
        }
        assembler_.emit(opcode, first_operand, getStackSlotLocationByOffsetFromTos(1));
    }

    void BuildingContext::generateCmpInsn() {
        if (annotated_bytecode_.getInsn(current_insn_index_ + 1) == BC_ILOAD0 &&
                isConiditionalJump(annotated_bytecode_.getInsn(current_insn_index_ + 2))) {

            bool is_xmm = getCurrentInsn() == BC_DCMP;
            if (is_xmm) {
                emitCmpInsnByOpcode(kX86InstIdUcomisd, xmm15);
            } else {
                assert(getCurrentInsn() == BC_ICMP);
                emitCmpInsnByOpcode(kX86InstIdCmp, rax);
            }

            current_insn_index_ += 2;
            emitConditionalJump(is_xmm);
            return;
        }

        if (getCurrentInsn() == BC_DCMP) {
            emitFunctionCallByAddr({VT_INT, VT_DOUBLE, VT_DOUBLE}, &cmp_double);
        } else {
            assert(getCurrentInsn() == BC_ICMP);
            emitFunctionCallByAddr({VT_INT, VT_INT, VT_INT}, &cmp_int);
        }
    }

    void BuildingContext::movFromTo(Operand src, Operand dst) {
        bool is_gp = true;

        if (!src.isReg() && !dst.isReg()) {
            assembler_.emit(kX86InstIdMov, rax, src);
            src = rax;
        }

        if (src.isReg() && src.isRegType(kX86RegTypeXmm)) {
            is_gp = false;
        }
        if (dst.isReg() && dst.isRegType(kX86RegTypeXmm)) {
            is_gp = false;
        }

        uint32_t opcode = is_gp ? kX86InstIdMov : kX86InstIdMovsd;
        assembler_.emit(opcode, dst, src);
    }

    bool BuildingContext::tryEmitComparisionPattern(bool is_xmm) {
        if (annotated_bytecode_.length() > current_insn_index_ + 3 &&
            annotated_bytecode_.getInsn(current_insn_index_ + 1) == BC_ILOAD0 &&
            annotated_bytecode_.getInsn(current_insn_index_ + 2) == BC_JA &&
            annotated_bytecode_.getInsnWithArgs(current_insn_index_ + 2).transition_id == current_insn_index_ + 4 &&
            annotated_bytecode_.getInsn(current_insn_index_ + 3) == BC_ILOAD1) {
            InstructionWithArgs insnWithArgs = getCurrentInsnWithArg();
            X86InstId opcode;
            switch(insnWithArgs.insn) {
                case BC_IFICMPE:
                    opcode = kX86InstIdSete;
                    break;
                case BC_IFICMPNE:
                    opcode = kX86InstIdSetne;
                    break;
                case BC_IFICMPL:
                    opcode = is_xmm ? kX86InstIdSetb : kX86InstIdSetl;
                    break;
                case BC_IFICMPLE:
                    opcode = is_xmm ? kX86InstIdSetbe : kX86InstIdSetle;
                    break;
                case BC_IFICMPG:
                    opcode = is_xmm ? kX86InstIdSeta : kX86InstIdSetg;
                    break;
                case BC_IFICMPGE:
                    opcode = is_xmm ? kX86InstIdSetae : kX86InstIdSetge;
                    break;
                default:
                    throw runtime_error("Unexpected comparision operation: " + to_string(insnWithArgs.insn));
            }

            assembler_.emit(opcode, al);
            assembler_.movzx(rax, al);
            current_insn_index_ += 3;
            movToStackLocation(0, rax);
            return true;
        }

        return false;
    }

    bool BuildingContext::tryEmitNotPattern() {
        assert(getCurrentInsn() == BC_ILOAD0);
        if (current_insn_index_ + 5 < annotated_bytecode_.length() &&
                annotated_bytecode_.getInsn(current_insn_index_ + 1) == BC_ICMP &&
                annotated_bytecode_.getInsn(current_insn_index_ + 2) == BC_ILOAD &&
                annotated_bytecode_.getInsnWithArgs(current_insn_index_ + 2).int_value == -1L &&
                annotated_bytecode_.getInsn(current_insn_index_ + 3) == BC_IAXOR &&
                annotated_bytecode_.getInsn(current_insn_index_ + 4) == BC_ILOAD1 &&
                annotated_bytecode_.getInsn(current_insn_index_ + 5) == BC_IAAND) {

            assembler_.emit(kX86InstIdCmp, getStackSlotLocationByOffsetFromTos(1), 0);
            assembler_.sete(al);

            current_insn_index_ += 5;

            assembler_.emit(kX86InstIdMovzx, rax, al);
            movToStackLocation(0, rax);

            return true;
        }

        return false;
    }
}
