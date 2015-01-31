#ifndef COMMON
#define COMMON
#include "mathvm.h"
#include "visitors.h"

namespace mathvm {
    typedef uint16_t index_t;
    typedef int64_t vm_int_t;
    typedef char const * vm_str_t;
    typedef int32_t insn_index_t;
    typedef long long word_t;

    inline bool isFunctionNative(AstFunction & func) {
        return func.node()->body()->nodes() > 0 && func.node()->body()->nodeAt(0)->isNativeCallNode();
    }


}

#endif
