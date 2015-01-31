#include "runtime_environment.h"

namespace mathvm {
    const double RuntimeEnvironment::DOUBLE_ONE = 1.0;
    const double RuntimeEnvironment::DOUBLE_ZERO = 0.0;
    void print_int(vm_int_t t) {
        cout<< t;
    }
    void print_double(double t) {
        cout<< t;
    }
    void print_str(char const * str) {
        if (str != 0) {
            cout << str;
        }
    }
    vm_int_t cmp_int(vm_int_t b, vm_int_t a) {
        if (a == b) return 0;
        if (a > b) return 1;
        return -1;
    }
    vm_int_t cmp_double(double b, double a) {
        if (a == b) return 0;
        if (a > b) return 1;
        return -1;
    }
}
