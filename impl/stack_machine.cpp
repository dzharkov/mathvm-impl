#include "stack_machine.h"

namespace mathvm {

size_t functionsCount(Code * code) {
    size_t result = 0;
    Code::FunctionIterator it(code);
    while (it.hasNext()) {
        ++result;
        it.next();
    }

    return result;
}

void StackMachine::execute(Code * code) {
    code_ = code;
    data_stack_.reserve(max_stack_size_);
    return_addresses_.reserve(max_stack_size_);
    stack_frame_starts_.reserve(max_stack_size_);

    stack_top_ptr_ = data_stack_.data();
    return_addresses_top_ptr_ = return_addresses_.data();
    stack_frame_starts_top_ptr_ = stack_frame_starts_.data();
    
    current_stack_frame_start_ = stack_top_ptr_;

    frame_start_of_last_function_call_.reserve(functionsCount(code));
    current_function_ = nullptr;

    processCall(0);

    run();
}

template<typename T> 
vm_int_t cmp(T a, T b) {
    if (a == b) return 0L;
    return a > b ? 1L : -1L;
}

static const char * __EMPTY_STRING = "";

void StackMachine::run() {
    while (true) {
        Instruction current_instruction = currentBytecode().getInsn(current_location_++);
        char const * v  = 0;
        switch (current_instruction) {
            case BC_DLOAD:
                push(currentBytecode().getDouble(current_location_));
                current_location_ += sizeof(double);
                break;
            case BC_DLOAD0:
                push(0.0);
                break;
            case BC_DLOAD1:
                push(1.0);
                break;
            case BC_ILOAD:
                push(currentBytecode().getInt64(current_location_));
                current_location_ += sizeof(vm_int_t);
                break;
            case BC_ILOAD0:
                push(vm_int_t(0L));
                break;
            case BC_ILOAD1:
                push(vm_int_t(1L));
                break;
            case BC_SLOAD:
                push(code_->constantById(getCurrent2BytesAndShiftLocation()).c_str());
                break;
            case BC_SLOAD0:
                push(__EMPTY_STRING);
                break;
            case BC_DADD:
                PUSH_BINARY_RESULT(double, std::plus<double>());
                break;
            case BC_DSUB:
                PUSH_BINARY_RESULT(double, std::minus<double>());
                break;
            case BC_DMUL:
                PUSH_BINARY_RESULT(double, std::multiplies<double>());
                break;
            case BC_DDIV:
                PUSH_BINARY_RESULT(double, std::divides<double>());
                break;
            case BC_IADD:
                PUSH_BINARY_RESULT(vm_int_t, std::plus<vm_int_t>());
                break;
            case BC_ISUB:
                PUSH_BINARY_RESULT(vm_int_t, std::minus<vm_int_t>());
                break;
            case BC_IMUL:
                PUSH_BINARY_RESULT(vm_int_t, std::multiplies<vm_int_t>());
                break;
            case BC_IDIV:
                PUSH_BINARY_RESULT(vm_int_t, std::divides<vm_int_t>());
                break;
            case BC_IMOD:
                PUSH_BINARY_RESULT(vm_int_t, std::modulus<vm_int_t>());
                break;
            case BC_IAOR:
                PUSH_BINARY_RESULT(vm_int_t, std::bit_or<vm_int_t>());
                break;
            case BC_IAAND:
                PUSH_BINARY_RESULT(vm_int_t, std::bit_and<vm_int_t>());
                break;
            case BC_IAXOR:
                PUSH_BINARY_RESULT(vm_int_t, std::bit_xor<vm_int_t>());
                break;
            case BC_DCMP:
                PUSH_BINARY_RESULT_(double, vm_int_t, cmp<double>);
                break;
            case BC_ICMP:
                PUSH_BINARY_RESULT(vm_int_t, cmp<vm_int_t>);
                break;
            case BC_DNEG:
                push(-popDouble());
                break;
            case BC_INEG:
                push(-popInt());
                break;
            case BC_IPRINT:
                os_ << popInt();
                os_.flush();
                break;
            case BC_DPRINT:
                os_ << popDouble();
                os_.flush();
                break;
            case BC_SPRINT:
                v = popString();
                os_ << (v == 0 ? "" : v);
                os_.flush();
                break;
            case BC_I2D:
                push((double) popInt());
                break;
            case BC_S2I:
                v = popString();
                try {
                    push((vm_int_t) v);
                } catch (std::exception & e) {
                    throwError("S2I conversion error: " + string(v));
                }
                break;
            case BC_D2I:
                push((vm_int_t) popDouble());
                break;
            case BC_POP:
                pop();
                break;
            case BC_LOADDVAR0:
                loadLocalVar<double>(0);
                break;
            case BC_LOADDVAR1:
                loadLocalVar<double>(1);
                break;
            case BC_LOADDVAR2:
                loadLocalVar<double>(2);
                break;
            case BC_LOADDVAR3:
                loadLocalVar<double>(3);
                break;
            case BC_LOADDVAR:
                loadLocalVar<double>(getCurrent2BytesAndShiftLocation());
                break;
            case BC_LOADIVAR0:
                loadLocalVar<vm_int_t>(0);
                break;
            case BC_LOADIVAR1:
                loadLocalVar<vm_int_t>(1);
                break;
            case BC_LOADIVAR2:
                loadLocalVar<vm_int_t>(2);
                break;
            case BC_LOADIVAR3:
                loadLocalVar<vm_int_t>(3);
                break;
            case BC_LOADIVAR:
                loadLocalVar<vm_int_t>(getCurrent2BytesAndShiftLocation());
                break;
            case BC_LOADSVAR0:
                loadLocalVar<vm_str_t>(0);
                break;
            case BC_LOADSVAR1:
                loadLocalVar<vm_str_t>(1);
                break;
            case BC_LOADSVAR2:
                loadLocalVar<vm_str_t>(2);
                break;
            case BC_LOADSVAR3:
                loadLocalVar<vm_str_t>(3);
                break;
            case BC_LOADSVAR:
                loadLocalVar<vm_str_t>(getCurrent2BytesAndShiftLocation());
                break;
            case BC_STOREDVAR0:
                storeLocalVar<double>(0);
                break;
            case BC_STOREDVAR1:
                storeLocalVar<double>(1);
                break;
            case BC_STOREDVAR2:
                storeLocalVar<double>(2);
                break;
            case BC_STOREDVAR3:
                storeLocalVar<double>(3);
                break;
            case BC_STOREDVAR:
                storeLocalVar<double>(getCurrent2BytesAndShiftLocation());
                break;
            case BC_STOREIVAR0:
                storeLocalVar<vm_int_t>(0);
                break;
            case BC_STOREIVAR1:
                storeLocalVar<vm_int_t>(1);
                break;
            case BC_STOREIVAR2:
                storeLocalVar<vm_int_t>(2);
                break;
            case BC_STOREIVAR3:
                storeLocalVar<vm_int_t>(3);
                break;
            case BC_STOREIVAR:
                storeLocalVar<vm_int_t>(getCurrent2BytesAndShiftLocation());
                break;
            case BC_STORESVAR0:
                storeLocalVar<vm_str_t>(0);
                break;
            case BC_STORESVAR1:
                storeLocalVar<vm_str_t>(1);
                break;
            case BC_STORESVAR2:
                storeLocalVar<vm_str_t>(2);
                break;
            case BC_STORESVAR3:
                storeLocalVar<vm_str_t>(3);
                break;
            case BC_STORESVAR:
                storeLocalVar<vm_str_t>(getCurrent2BytesAndShiftLocation());
                break;
            case BC_LOADCTXDVAR:
                processLoadContextVar<double>();
                break;
            case BC_LOADCTXIVAR:
                processLoadContextVar<vm_int_t>();
                break;
            case BC_LOADCTXSVAR:
                processLoadContextVar<vm_str_t>();
                break;
            case BC_STORECTXDVAR:
                processStoreContextVar<double>();
                break;
            case BC_STORECTXIVAR:
                processStoreContextVar<vm_int_t>();
                break;
            case BC_STORECTXSVAR:
                processStoreContextVar<vm_str_t>();
                break;
            case BC_JA:
                current_location_ = calculateTransitionAndShiftLocation();
                continue;
            case BC_IFICMPE:
            case BC_IFICMPNE:
            case BC_IFICMPL:
            case BC_IFICMPLE:
            case BC_IFICMPG:
            case BC_IFICMPGE: {
                    index_t transition = calculateTransitionAndShiftLocation();

                    vm_int_t a = popInt();
                    vm_int_t b = popInt();

                    if (ifSatisfied(current_instruction, cmp(a, b))) {
                        current_location_= transition;
                        continue;
                    }
                    break;
                }
            case BC_CALL:
                processCall(getCurrent2BytesAndShiftLocation());
                break;
            case BC_CALLNATIVE:
                processNativeCall(getCurrent2BytesAndShiftLocation());
                continue;
            case BC_RETURN:
                if (processReturn()) {
                    return;
                }
                continue;
            default:
                throwError("unsupported insn=" + string(bytecodeName(current_instruction)));
                return;
        }
    }
}

bool StackMachine::ifSatisfied(Instruction condition, vm_int_t value) {
    switch(condition) {
        case BC_IFICMPE:
            return value == 0L;
        case BC_IFICMPNE:
            return value != 0L;
        case BC_IFICMPL:
            return value < 0L;
        case BC_IFICMPLE:
            return value <= 0L;
        case BC_IFICMPG:
            return value > 0L;
        case BC_IFICMPGE:
            return value >= 0L;
        default:
            throwError("unsupported condition=" + string(bytecodeName(condition)));
            return false;
    }
}

void StackMachine::processCall(index_t id) {
    if (current_function_ != 0) {
        *(return_addresses_top_ptr_++) = {current_function_->id(), current_location_, frame_start_of_last_function_call_[id]};
    }

    current_function_ = static_cast<BytecodeFunction*>(code_->functionById(id));
    current_location_ = 0;

    StackValue * callee_stack_start = stack_top_ptr_ - current_function_->parametersNumber();
    
    current_stack_frame_start_ = callee_stack_start;
    *(stack_frame_starts_top_ptr_++) = current_stack_frame_start_;

    stack_top_ptr_ += current_function_->localsNumber();

    frame_start_of_last_function_call_[id] = callee_stack_start;  
}

bool StackMachine::processReturn() {
    if (return_addresses_top_ptr_ == return_addresses_.data()) return true;
    
    StackValue * new_stack_top = *(--stack_frame_starts_top_ptr_);
    current_stack_frame_start_ = *(stack_frame_starts_top_ptr_ - 1);
    
    if (current_function_->returnType() != VT_VOID) {
        StackValue return_value = *(--stack_top_ptr_);
        *(new_stack_top++) = return_value;
    }
    
    stack_top_ptr_ = new_stack_top;

    CodePointer return_address = *(--return_addresses_top_ptr_);

    frame_start_of_last_function_call_[current_function_->id()] = return_address.previous_call_of_same_function_start_;

    current_function_ = static_cast<BytecodeFunction*>(code_->functionById(return_address.function_id));
    current_location_ = return_address.bytecode_location;

    return false;
}

void StackMachine::processNativeCall(index_t id) {

    Signature const * signature;
    string const * name;
    void * proxy_addr = const_cast<void*>(code_->nativeById(id, &signature, &name));

    auto returnType = (*signature)[0].first;
    StackValue result;
    StackValue * begin_offset = stack_top_ptr_ - signature->size();

    switch(returnType) {
        case VT_DOUBLE:
        {
            result = reinterpret_cast<double (*)(StackValue *)>(proxy_addr)(begin_offset);
            break;
        }
        case VT_INT:
        case VT_STRING:
        case VT_VOID:
        {
            result = reinterpret_cast<vm_int_t (*)(StackValue *)>(proxy_addr)(begin_offset);
            break;
        }
        default:
            throwError("invalid type in native");
            return;
    }
    
    for (size_t i = 1; i < signature->size(); ++i) {
        pop();
    }

    if (signature->at(0).first != VT_VOID) {
        push(result);
    }
}

}
