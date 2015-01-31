#ifndef RUNTIME_ENVIRONMENT_H
#define RUNTIME_ENVIRONMENT_H

#include <vector>
#include "common.h"

namespace mathvm {
    using namespace std;

    void print_int(vm_int_t t);
    void print_double(double t);
    void print_str(char const * str);
    vm_int_t cmp_int(vm_int_t b, vm_int_t a);
    vm_int_t cmp_double(double b, double a);

    class RuntimeEnvironment {
    public:
        RuntimeEnvironment() {
        }

        void setFunctionsCount(size_t count) {
            bsp_of_last_call_by_function_id_.resize(count, 0);
        }

        void * pointerToBspOfLastCall(index_t function_id) {
            return &bsp_of_last_call_by_function_id_[function_id];
        }

        static constexpr const char * EMPTY_STRING = "";
        static const double DOUBLE_ONE;
        static const double DOUBLE_ZERO;

    private:
        vector<void*> bsp_of_last_call_by_function_id_;

    };
}

#endif 
