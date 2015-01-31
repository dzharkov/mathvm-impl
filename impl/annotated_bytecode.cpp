#include "annotated_bytecode.h"

namespace mathvm {

    vector<InstructionWithArgs> buildInsns(Bytecode &bytecode) {
        vector<InstructionWithArgs> result;
        vector<pair<index_t, index_t>> unresolved_transitions;
        vector<insn_index_t> index_by_offset(bytecode.length(), -1);

        for (insn_index_t current_location = 0; current_location < (insn_index_t)bytecode.length();) {
            insn_index_t current_index = result.size();
            index_by_offset[current_location] = current_index;

            Instruction current_insn = bytecode.getInsn(current_location);
            InstructionWithArgs current;
            current.insn = current_insn;

            current_location++;
            switch (current_insn) {
                case BC_DLOAD:
                    current.double_value = bytecode.getDouble(current_location);
                    current_location += sizeof(double);
                    break;
                case BC_ILOAD:
                    current.int_value = bytecode.getInt64(current_location);
                    current_location += sizeof(vm_int_t);
                    break;
                case BC_SLOAD:
                case BC_LOADDVAR:
                case BC_LOADIVAR:
                case BC_LOADSVAR:
                case BC_STOREDVAR:
                case BC_STORESVAR:
                case BC_STOREIVAR:
                case BC_CALL:
                case BC_CALLNATIVE:
                    current.index = bytecode.getUInt16(current_location);
                    current_location += 2;
                    break;
                case BC_LOADCTXDVAR:
                case BC_LOADCTXIVAR:
                case BC_LOADCTXSVAR:
                case BC_STORECTXDVAR:
                case BC_STORECTXIVAR:
                case BC_STORECTXSVAR:
                    current.context_var = {bytecode.getUInt16(current_location), bytecode.getUInt16(current_location + 2)};
                    current_location += 4;
                    break;
                case BC_JA:
                case BC_IFICMPE:
                case BC_IFICMPNE:
                case BC_IFICMPL:
                case BC_IFICMPLE:
                case BC_IFICMPG:
                case BC_IFICMPGE:
                    unresolved_transitions.push_back(make_pair(current_index, current_location + bytecode.getInt16(current_location)));
                    current_location += 2;
                    break;
                case BC_DLOAD0:
                case BC_DLOAD1:
                case BC_ILOAD0:
                case BC_ILOAD1:
                case BC_SLOAD0:
                case BC_DADD:
                case BC_DSUB:
                case BC_DMUL:
                case BC_DDIV:
                case BC_IADD:
                case BC_ISUB:
                case BC_IMUL:
                case BC_IDIV:
                case BC_IMOD:
                case BC_IAOR:
                case BC_IAAND:
                case BC_IAXOR:
                case BC_ICMP:
                case BC_DCMP:
                case BC_DNEG:
                case BC_INEG:
                case BC_DPRINT:
                case BC_IPRINT:
                case BC_SPRINT:
                case BC_I2D:
                case BC_S2I:
                case BC_D2I:
                case BC_POP:
                case BC_LOADDVAR0:
                case BC_LOADDVAR1:
                case BC_LOADDVAR2:
                case BC_LOADDVAR3:
                case BC_LOADIVAR0:
                case BC_LOADIVAR1:
                case BC_LOADIVAR2:
                case BC_LOADIVAR3:
                case BC_LOADSVAR0:
                case BC_LOADSVAR1:
                case BC_LOADSVAR2:
                case BC_LOADSVAR3:
                case BC_STOREDVAR0:
                case BC_STOREDVAR1:
                case BC_STOREDVAR2:
                case BC_STOREDVAR3:
                case BC_STOREIVAR0:
                case BC_STOREIVAR1:
                case BC_STOREIVAR2:
                case BC_STOREIVAR3:
                case BC_STORESVAR0:
                case BC_STORESVAR1:
                case BC_STORESVAR2:
                case BC_STORESVAR3:
                case BC_RETURN:
                    break;
                default:
                    throw runtime_error("unsupported ainsn=" + to_string(current_insn));
            };

            result.push_back(current);
        }

        for (auto & p : unresolved_transitions) {
            result[p.first].transition_id = index_by_offset[p.second];
        }

        return result;
    }

    AnnotatedBytecode AnnotatedBytecode::annotate(BytecodeFunction & function, Code &code) {
        Bytecode & bytecode = *function.bytecode();
        AnnotatedBytecode result(buildInsns(bytecode));

        vector<bool> visited(result.length());

        queue<insn_index_t> q;
        q.push(0);

        while (!q.empty()) {
            insn_index_t current_insn_index = q.front();
            InstructionWithArgs current_insn_with_args = result.getInsnWithArgs(current_insn_index);

            q.pop();

            vector<insn_index_t> next_transitions;
            if (current_insn_index + 1 < result.length()) {
                next_transitions.push_back(current_insn_index + 1);
            }

            index_t popped = 0;
            vector<VarType> pushed;

            switch (current_insn_with_args.insn) {
                case BC_DLOAD:
                    pushed = {VT_DOUBLE};
                    break;
                case BC_DLOAD0:
                case BC_DLOAD1:
                    pushed = {VT_DOUBLE};
                    break;
                case BC_ILOAD:
                    pushed = {VT_INT};
                    break;
                case BC_ILOAD0:
                case BC_ILOAD1:
                    pushed = {VT_INT};
                    break;
                case BC_SLOAD:
                    pushed = {VT_INT};
                    break;
                case BC_SLOAD0:
                    pushed = {VT_INT};
                    break;
                case BC_DADD:
                case BC_DSUB:
                case BC_DMUL:
                case BC_DDIV:
                case BC_IADD:
                case BC_ISUB:
                case BC_IMUL:
                case BC_IDIV:
                case BC_IMOD:
                case BC_IAOR:
                case BC_IAAND:
                case BC_IAXOR:
                case BC_ICMP:
                    popped = 1;
                    break;
                case BC_DCMP:
                    popped = 2;
                    pushed = {VT_INT};
                    break;
                case BC_DNEG:
                case BC_INEG:
                    break;
                case BC_DPRINT:
                    popped = 1;
                    break;
                case BC_IPRINT:
                case BC_SPRINT:
                    popped = 1;
                    break;
                case BC_I2D:
                    popped = 1;
                    pushed = {VT_DOUBLE};
                    break;
                case BC_S2I:
                    break;
                case BC_D2I:
                    popped = 1;
                    pushed = {VT_INT};
                    break;
                case BC_POP:
                    popped = 1;
                    break;
                case BC_LOADDVAR0:
                case BC_LOADDVAR1:
                case BC_LOADDVAR2:
                case BC_LOADDVAR3:
                    pushed = {VT_DOUBLE};
                    break;
                case BC_LOADIVAR0:
                case BC_LOADIVAR1:
                case BC_LOADIVAR2:
                case BC_LOADIVAR3:
                case BC_LOADSVAR0:
                case BC_LOADSVAR1:
                case BC_LOADSVAR2:
                case BC_LOADSVAR3:
                    pushed = {VT_INT};
                    break;
                case BC_LOADDVAR:
                    pushed = {VT_DOUBLE};
                    break;
                case BC_LOADIVAR:
                case BC_LOADSVAR:
                    pushed = {VT_INT};
                    break;
                case BC_STOREDVAR0:
                case BC_STOREDVAR1:
                case BC_STOREDVAR2:
                case BC_STOREDVAR3:
                case BC_STOREIVAR0:
                case BC_STOREIVAR1:
                case BC_STOREIVAR2:
                case BC_STOREIVAR3:
                case BC_STORESVAR0:
                case BC_STORESVAR1:
                case BC_STORESVAR2:
                case BC_STORESVAR3:
                    popped = 1;
                    break;
                case BC_STOREDVAR:
                case BC_STORESVAR:
                case BC_STOREIVAR:
                    popped = 1;
                    break;
                case BC_LOADCTXDVAR:
                    pushed = {VT_DOUBLE};
                    break;
                case BC_LOADCTXIVAR:
                case BC_LOADCTXSVAR:
                    pushed = {VT_INT};
                    break;
                case BC_STORECTXDVAR:
                case BC_STORECTXIVAR:
                case BC_STORECTXSVAR:
                    popped = 1;
                    break;
                case BC_JA:
                    next_transitions.clear();
                    next_transitions.push_back(current_insn_with_args.transition_id);
                    result.setUsedAsLabel(current_insn_with_args.transition_id);
                    break;
                case BC_IFICMPE:
                case BC_IFICMPNE:
                case BC_IFICMPL:
                case BC_IFICMPLE:
                case BC_IFICMPG:
                case BC_IFICMPGE:
                    next_transitions.push_back(current_insn_with_args.transition_id);
                    result.setUsedAsLabel(current_insn_with_args.transition_id);
                    popped = 2;
                    break;
                case BC_CALL: {
                    Signature signature = code.functionById(current_insn_with_args.index)->signature();

                    if (signature[0].first != VT_VOID) {
                        pushed = {signature[0].first};
                    };

                    popped = signature.size() - 1;

                    break;
                }
                case BC_CALLNATIVE: {
                    Signature const *signature;
                    string const *name;
                    code.nativeById(current_insn_with_args.index, &signature, &name);

                    if ((*signature)[0].first != VT_VOID) {
                        pushed = {(*signature)[0].first};
                    };

                    popped = signature->size() - 1;
                    break;
                }
                case BC_RETURN:
                    popped = function.signature()[0].first != VT_VOID ? 0 : 1;
                    break;
                default:
                    throw runtime_error("unsupported ainsn=" + to_string(current_insn_with_args.insn));
            }

            for (insn_index_t next_transition : next_transitions) {
                if (!visited[next_transition]) {
                    visited[next_transition] = true;
                    result.addTransition(current_insn_index, next_transition, popped, pushed);
                    q.push(next_transition);
                }
            }
        }

        return result;
    }
}
