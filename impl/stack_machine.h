#ifndef STACK_MACHINE_IMPL 
#define STACK_MACHINE_IMPL 
#include "mathvm.h"
#include "common.h"

#include <map>
#include <string>
#include <stack>
#include <vector>
#include <exception>
#include <iostream>
#include <functional>

namespace mathvm { 

union StackValue {
    StackValue() {
        data_.intValue = 0L;
    }
    StackValue(vm_int_t v) {
        data_.intValue = v;
    }
    StackValue(double v) {
        data_.doubleValue = v;
    }
    StackValue(vm_str_t v) {
        data_.stringValue = v;
    }

    vm_int_t intValue() const {
        return data_.intValue;
    }
    long long wordValue() const {
        return (long long)data_.intValue;
    }
    double doubleValue() const {
        return data_.doubleValue;
    }
    vm_str_t stringValue() const {
        return data_.stringValue;
    }
    explicit operator double() const {
        return doubleValue();
    }
    explicit operator vm_int_t() const {
        return intValue();
    }
    explicit operator vm_str_t() const {
        return stringValue();
    }
private:
    union {
        vm_int_t intValue;
        double doubleValue;
        vm_str_t stringValue;
    } data_;
};

struct CodePointer {
    index_t function_id;
    insn_index_t bytecode_location;
    StackValue * previous_call_of_same_function_start_;
};

class StackMachine {
public:
    StackMachine(ostream & os, size_t max_stack_size) : os_(os), max_stack_size_(max_stack_size / sizeof(StackValue)) {}
    void execute(Code * code);
private:
    void run();
    bool ifSatisfied(Instruction condition, vm_int_t value);
    void processCall(index_t id);
    void processNativeCall(index_t id);
    bool processReturn();

    Bytecode & currentBytecode() {
        return *(current_function_->bytecode());
    }

    void throwError(string const & msg) {
        throw runtime_error(msg);
    }

    void push(StackValue v) {
        *(stack_top_ptr_++) = v;
    }

    StackValue pop() {
        StackValue v = *(--stack_top_ptr_);
        return v;
    }

    double popDouble() {
        return pop().doubleValue();
    }
    
    vm_int_t popInt() {
        return pop().intValue();
    }
    
    vm_str_t popString() {
        return pop().stringValue();
    }

    #define PUSH_BINARY_RESULT_(T,R,f) {\
        T a = (T)pop(); \
        T b = (T)pop(); \
        push(R(f(a,b))); \
    }
    #define PUSH_BINARY_RESULT(T,f) {\
        T a = (T)pop(); \
        T b = (T)pop(); \
        push(T(f(a,b))); \
    }
    
    template<typename T> 
    void loadLocalVar(index_t i) {
        push(T(*(current_stack_frame_start_ + (size_t)i)));
    }
    
    
    template<typename T> 
    void storeLocalVar(index_t i) {
        *(current_stack_frame_start_ + (size_t)i) = T(pop());
    }
    
    template<typename T> 
    void processLoadContextVar() {
        push(T(calculateContextVarOffsetAndShiftLocation()));
    }
    
    template<typename T> 
    void processStoreContextVar() {
        calculateContextVarOffsetAndShiftLocation() = T(pop());
    }

    StackValue & calculateContextVarOffsetAndShiftLocation() {
        index_t f_id = getCurrent2BytesAndShiftLocation();
        index_t i = getCurrent2BytesAndShiftLocation();
        return *(frame_start_of_last_function_call_[f_id] + i); 
    }

    index_t getCurrent2BytesAndShiftLocation() {
        index_t res = currentBytecode().getUInt16(current_location_);
        current_location_ += sizeof(index_t);
        return res;
    }

    index_t calculateTransitionAndShiftLocation() {
        index_t res = current_location_;
        res += getCurrent2BytesAndShiftLocation();
        return res;
    }

    vector<StackValue> data_stack_;
    StackValue * stack_top_ptr_;

    vector<CodePointer> return_addresses_;
    CodePointer * return_addresses_top_ptr_;
    
    vector<StackValue*> stack_frame_starts_;
    StackValue** stack_frame_starts_top_ptr_; 
    
    StackValue * current_stack_frame_start_;

    vector<StackValue*> frame_start_of_last_function_call_;
    insn_index_t  current_location_; 
    BytecodeFunction * current_function_ = nullptr;
    
    Code * code_ = nullptr;
    ostream & os_;
    size_t max_stack_size_;
};

}
#endif
