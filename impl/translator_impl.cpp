#include "translator_impl.h"
#include "typing_visitor.h"
#include "interpreter_impl.h"
#include "jit_builder.h"
#include "function_analysis.h"

#include <map>

namespace mathvm { 

Status* BytecodeTranslatorImpl::translateBytecode(const string& program, InterpreterCodeImpl* *code) {
    Parser parser;
    Status * status = parser.parseProgram(program);
    if (status != NULL && status->isError()) return status;
    
    map<AstNode *, VarType> types_map;
    map<AstFunction *, void *> natives_map;
    TypingVisitor typing_visitor(types_map, natives_map);
    
    BlockNode * topBlock = parser.top()->node()->body();

    topBlock->visit(&typing_visitor);
    
    if (typing_visitor.errors().size() > 0) {
        string msg = "";
        for (auto & err_msg : typing_visitor.errors()) {
            msg += err_msg + ",";
        }

        return Status::Error(msg.substr(0, msg.length()-1).c_str());
    }

    TranslatorVisitor translator_visitor(**code, types_map, natives_map);
    translator_visitor.processTopLevel(*(parser.top()));

    return Status::Ok(); 
}

Status * BytecodeTranslatorImpl::translate(const string & program, Code* *code) {
    InterpreterCodeImpl* result_code = new InterpreterCodeImpl(); 
    *code = result_code;
    return translateBytecode(program, &result_code);
}

void TranslatorVisitor::processTopLevel(AstFunction & top_level) {
    FunctionAnalysisVisitor functionAnalysisVisitor(code_);
    functionAnalysisVisitor.processTopLevel(top_level);

    is_function_recursive_ = functionAnalysisVisitor.is_function_recursive();

    processFunction(top_level);

    reinterpret_cast<BytecodeFunction*>(top_level.info())->bytecode()->addInsn(BC_RETURN);
}

void TranslatorVisitor::processFunction(AstFunction & function) {
    if (isFunctionNative(function) || !inlining_contexts_.empty()) {
        return;
    }

    vars_count_.push(0); 
            
    bytecode_functions_stack_.push(reinterpret_cast<BytecodeFunction*>(function.info()));

    for (size_t i = 0; i < function.parametersNumber(); ++i) {
        pushVar(function.parameterName(i));
    }

    function.node()->body()->visit(this);
    
    for (size_t i = 0; i < function.parametersNumber(); ++i) {
        popVar(function.parameterName(i));
    }

    bytecode_functions_stack_.pop();
    vars_count_.pop();
}

void TranslatorVisitor::visitBlockNode(BlockNode* node) { 
    pushScope(node->scope());
    
    Scope::FunctionIterator funcs_it(node->scope());
    
    while (funcs_it.hasNext()) {
        AstFunction * func = funcs_it.next();
        processFunction(*func);
    }
    
    for (size_t i = 0; i < node->nodes(); ++i) {
        gen(node->nodeAt(i), VT_VOID);
    }

    popScope();
}

void TranslatorVisitor::visitBinaryOpNode(BinaryOpNode* node) {
    if (node->kind() >= tEQ && node->kind() <= tLE) {
        return generateComparision(node);
    }

    VarType result_type = typeOf(node);
    
    if (node->kind() == tOR || node->kind() == tAND) {
        gen(node->left(), result_type);
        bytecodeStream().addInsn(BC_ILOAD0);
        
        Instruction cmp_insn = node->kind() == tOR ? BC_IFICMPNE : BC_IFICMPE;
        
        Label label1(&bytecodeStream());
        bytecodeStream().addBranch(cmp_insn, label1);
        
        gen(node->right(), result_type);
        
        Label label2(&bytecodeStream());
        bytecodeStream().addBranch(BC_JA, label2);

        bytecodeStream().bind(label1);
        bytecodeStream().addInsn(node->kind() == tOR ? BC_ILOAD1 : BC_ILOAD0);
        
        bytecodeStream().bind(label2);
        return;
    }

    gen(node->right(), result_type);
    gen(node->left(), result_type);
    
    Instruction insn;

    switch(node->kind()) {
        case tMUL:
            insn = chooseInsn(result_type, BC_IMUL, BC_DMUL);
            break;
        case tADD:
            insn = chooseInsn(result_type, BC_IADD, BC_DADD);
            break;
        case tSUB: 
            insn = chooseInsn(result_type, BC_ISUB, BC_DSUB);
            break;
        case tDIV: 
            insn = chooseInsn(result_type, BC_IDIV, BC_DDIV);
            break;
        case tAOR:
            insn = BC_IAOR;
            break;
        case tAAND:
            insn = BC_IAAND;
            break;
        case tAXOR:  
            insn = BC_IAXOR;
            break;
        case tMOD:  
            insn = BC_IMOD;
            break;
        default:
            throwError(string("unknown binary operation ") + tokenStr(node->kind()));
            return;
    }

    bytecodeStream().addInsn(insn);
}

void TranslatorVisitor::generateComparision(BinaryOpNode * node) {
    Instruction insn;
    
    VarType lup = (typeOf(node->right()) == VT_DOUBLE || typeOf(node->left()) == VT_DOUBLE) ? VT_DOUBLE : VT_INT; 

    gen(node->right(), lup);
    gen(node->left(), lup);
    bytecodeStream().addInsn(lup == VT_DOUBLE ? BC_DCMP : BC_ICMP);
    bytecodeStream().addInsn(BC_ILOAD0);

    switch (node->kind()) {
        case tEQ:
            insn = BC_IFICMPE;
            break;
        case tNEQ:
            insn = BC_IFICMPNE;
            break;
        case tGT:
            insn = BC_IFICMPL;
            break;
        case tLT:
            insn = BC_IFICMPG;
            break;
        case tGE:
            insn = BC_IFICMPLE;
            break;
        case tLE:
            insn = BC_IFICMPGE; 
            break;
        default:
            throwError(string("unknown cmp operation ") + tokenStr(node->kind()));
            return;
    }

    Label success(&bytecodeStream());
    Label after(&bytecodeStream());
    bytecodeStream().addBranch(insn, success);
    bytecodeStream().addInsn(BC_ILOAD0);
    bytecodeStream().addBranch(BC_JA, after);
    bytecodeStream().bind(success);
    bytecodeStream().addInsn(BC_ILOAD1);
    bytecodeStream().bind(after);
}

void TranslatorVisitor::visitUnaryOpNode(UnaryOpNode* node) {
    VarType type = typeOf(node);
    gen(node->operand(), type);
    
    Instruction insn;
    
    switch (node->kind()) {
        case tNOT:
            bytecodeStream().addInsn(BC_ILOAD0);
            bytecodeStream().addInsn(BC_ICMP);
            bytecodeStream().addInsn(BC_ILOAD);
            bytecodeStream().addInt64(~(0L));

            bytecodeStream().addInsn(BC_IAXOR);
            bytecodeStream().addInsn(BC_ILOAD1);
            bytecodeStream().addInsn(BC_IAAND);
            return;
        case tSUB:
            insn = chooseInsn(type, BC_INEG, BC_DNEG);
            break;
        default:
            throwError(string("unknown unary operation ") + tokenStr(node->kind()));
            return;
    }

    bytecodeStream().addInsn(insn);
}

void TranslatorVisitor::visitStringLiteralNode(StringLiteralNode* node) {
    if (node->literal().size() == 0) {
        bytecodeStream().addInsn(BC_SLOAD0);
    } else {
        index_t cp_index = code_.makeStringConstant(node->literal());
        bytecodeStream().addInsn(BC_SLOAD);
        bytecodeStream().addUInt16(cp_index);
    }
}

void TranslatorVisitor::visitDoubleLiteralNode(DoubleLiteralNode* node) {
    if (node->literal() == -1.0 || node->literal() == 1.0 || node->literal() == 0.0) {
        Instruction insn = BC_DLOAD0;
        if (node->literal() == -1.0) {
            insn = BC_DLOADM1;
        } else if (node->literal() == 1.0) {
            insn = BC_DLOAD1;
        }

        bytecodeStream().addInsn(insn);
        return;
    }

    bytecodeStream().addInsn(BC_DLOAD);
    bytecodeStream().addDouble(node->literal());
}

void TranslatorVisitor::visitIntLiteralNode(IntLiteralNode* node) {
    if (node->literal() >= 0L && node->literal() <= 1L) {
        Instruction insn = BC_ILOAD0;
        if (node->literal() == -1L) {
            insn = BC_ILOADM1;
        } else if (node->literal() == 1L) {
            insn = BC_ILOAD1;
        }

        bytecodeStream().addInsn(insn);
        return;
    }

    bytecodeStream().addInsn(BC_ILOAD);
    bytecodeStream().addInt64(node->literal());
}

void TranslatorVisitor::visitLoadNode(LoadNode* node) {
    localVariableInsn(node->var(), true); 
}

void TranslatorVisitor::visitStoreNode(StoreNode* node) {
    if (node->op() != tASSIGN) {
        gen(node->value(), node->var()->type());
        localVariableInsn(node->var(), true);
        if (node->op() == tINCRSET) {
            bytecodeStream().addInsn(chooseInsn(node->var()->type(), BC_IADD, BC_DADD));
        } else {
            bytecodeStream().addInsn(chooseInsn(node->var()->type(), BC_ISUB, BC_DSUB));
        }
    } else {
        gen(node->value(), node->var()->type());
    }

    localVariableInsn(node->var(), false); 
}

void TranslatorVisitor::visitForNode(ForNode* node) {
    BinaryOpNode * range = node->inExpr()->asBinaryOpNode();
    AstVar  const * var = node->var();
    
    StoreNode init(0, var, range->left(), tASSIGN);
    gen(&init);

    AstVar rangeEndVar = genTempVar(VT_INT);
    StoreNode rangeEnd(0, &rangeEndVar, range->right(), tASSIGN);
    gen(&rangeEnd);
    
    Label begin(&bytecodeStream());
    bytecodeStream().bind(begin);
    Label end(&bytecodeStream());

    localVariableInsn(&rangeEndVar, true);
    localVariableInsn(var, true);
    bytecodeStream().addBranch(BC_IFICMPG, end);

    gen(node->body(), VT_VOID);
    
    StoreNode incr(0, var, new IntLiteralNode(0, 1L), tINCRSET);
    gen(&incr);
    
    bytecodeStream().addBranch(BC_JA, begin);
    bytecodeStream().bind(end);
}

void TranslatorVisitor::visitWhileNode(WhileNode* node) {
    Label begin(&bytecodeStream());
    bytecodeStream().bind(begin);
    Label end(&bytecodeStream());
    
    gen(node->whileExpr(), VT_INT);
    bytecodeStream().addInsn(BC_ILOAD0);
    bytecodeStream().addBranch(BC_IFICMPE, end);
    
    gen(node->loopBlock(), VT_VOID);
    
    bytecodeStream().addBranch(BC_JA, begin);

    bytecodeStream().bind(end);
}

void TranslatorVisitor::visitIfNode(IfNode* node) {
    Label unless_branch(&bytecodeStream());

    gen(node->ifExpr(), VT_INT);
    bytecodeStream().addInsn(BC_ILOAD0);
    bytecodeStream().addBranch(BC_IFICMPE, unless_branch);

    gen(node->thenBlock(), VT_VOID);

    if (node->elseBlock() != 0) {
        Label after_else(&bytecodeStream());
        bytecodeStream().addBranch(BC_JA, after_else);
        bytecodeStream().bind(unless_branch);
        gen(node->elseBlock(), VT_VOID);
        bytecodeStream().bind(after_else);
    } else {
        bytecodeStream().bind(unless_branch);
    }
}

void TranslatorVisitor::visitReturnNode(ReturnNode* node) {
    VarType retType = inlining_contexts_.empty() ? bytecode_functions_stack_.top()->returnType() : inlining_contexts_.top().second;
    
    if (node->returnExpr() != 0) {
        gen(node->returnExpr(), retType);
    }

    if (inlining_contexts_.empty()) {
        bytecodeStream().addInsn(BC_RETURN);
    } else {
        bytecodeStream().addBranch(BC_JA, *(inlining_contexts_.top().first));
    }
}

void TranslatorVisitor::visitCallNode(CallNode* node) {
    auto func = scopes_.top()->lookupFunction(node->name());
    
    for (size_t i = 0; i < node->parametersNumber(); ++i) {
        gen(node->parameterAt(i), func->parameterType(i));
    }
    
    if (isFunctionNative(*func)) {
        index_t id = native_funcmap_[node->name()].top();        
        bytecodeStream().addInsn(BC_CALLNATIVE);
        bytecodeStream().addUInt16(id);
    } else {
        index_t id = reinterpret_cast<BytecodeFunction*>(func->info())->id();
        if (is_function_recursive_[id]) {
            bytecodeStream().addInsn(BC_CALL);
            bytecodeStream().addUInt16(id);
        } else {
            inlineCall(node);
        }
    }
}

void TranslatorVisitor::visitNativeCallNode(NativeCallNode* node) {
    throwError("unexpected native call");
}

void TranslatorVisitor::visitPrintNode(PrintNode* node) {
    for (size_t i = 0; i < node->operands(); ++i) {
        VarType type = gen(node->operandAt(i));
        
        switch (type) {
            case VT_INT:
               bytecodeStream().addInsn(BC_IPRINT);
               break;
            case VT_DOUBLE:
               bytecodeStream().addInsn(BC_DPRINT);
               break;
            case VT_STRING:
               bytecodeStream().addInsn(BC_SPRINT);
               break;
            default:
               throwError("There is no way to print " + string(typeToName(type)));
        }
    }
}

void TranslatorVisitor::pushScope(Scope * scope) {
    scopes_.push(scope);
    Scope::FunctionIterator funcs_it(scope);
    
    while (funcs_it.hasNext()) {
        AstFunction * func = funcs_it.next();
        if (isFunctionNative(*func)) {
            void * proxy_addr = JitBuilder::instance().buildNativeProxy(func->node()->signature(), (const void *)natives_map_[func]);
            index_t id = code_.makeNativeFunction(func->name(), func->node()->signature(), proxy_addr);
            native_funcmap_[func->name()].push(id);

            reinterpret_cast<InterpreterCodeImpl&>(code_).setNativeSource(id, (const void *)natives_map_[func]);

        }
    }

    Scope::VarIterator var_it(scope);
    while (var_it.hasNext()) {
        AstVar * var = var_it.next();
        pushVar(var->name());
    }
}

void TranslatorVisitor::popScope() {
    Scope * scope = scopes_.top();
    scopes_.pop();
    Scope::FunctionIterator funcs_it(scope);
    
    while (funcs_it.hasNext()) {
        AstFunction * func = funcs_it.next();
        if (isFunctionNative(*func)) {
            native_funcmap_[func->name()].pop();
        }
    }

    Scope::VarIterator var_it(scope);
    while (var_it.hasNext()) {
        AstVar * var = var_it.next();
        popVar(var->name());
    }
}

void TranslatorVisitor::pushVar(string const & name) {
    varmap_[name].push(make_pair(currentFunctionId(), current_vars_count()++));
    currentBytecodeFunction()->setLocalsNumber(max(current_vars_count(), (index_t)currentBytecodeFunction()->localsNumber()));
}

void TranslatorVisitor::popVar(string const & name) {
    varmap_[name].pop();
    --current_vars_count();
}

AstVar TranslatorVisitor::genTempVar(VarType type) {
    AstVar v("temp$" + to_string(currentFunctionId()) + "$" + to_string(current_vars_count()), type, NULL);
    pushVar(v.name());
    return v;
}

void TranslatorVisitor::gen(AstNode * node, VarType type) {
    coerceStackTop(gen(node), type);
}

VarType TranslatorVisitor::gen(AstNode * node) {
    node->visit(this);
    return typeOf(node);
}

VarType TranslatorVisitor::typeOf(AstNode * node) {
    return types_map_[node];
}

void TranslatorVisitor::coerceStackTop(VarType from, VarType to) {
    if (from == to) {
        return;
    }
    
    if (to == VT_VOID) {
        bytecodeStream().addInsn(BC_POP);
        return;
    }
    
    assert(to != VT_STRING);
    
    if (from == VT_DOUBLE && to == VT_INT) {
        bytecodeStream().addInsn(BC_D2I);
        return;
    }

    // converting STRING/INT to INT/DOUBLE
    if (from == VT_STRING) {
        bytecodeStream().addInsn(BC_S2I);
        from = VT_INT;
    }

    if (to == VT_DOUBLE) {
        bytecodeStream().addInsn(BC_I2D);
    }
}

Bytecode & TranslatorVisitor::bytecodeStream() {
    return *(currentBytecodeFunction()->bytecode());
}

BytecodeFunction * TranslatorVisitor::currentBytecodeFunction() {
    return bytecode_functions_stack_.top();
}

index_t TranslatorVisitor::currentFunctionId() {
    return currentBytecodeFunction()->id();
}

Instruction TranslatorVisitor::chooseInsn(VarType type, Instruction intInsn, Instruction doubleInsn) {
    if (type == VT_INT) return intInsn;
    if (type == VT_DOUBLE) return doubleInsn;
    throwError(string("unexpected type in chooseInsn: ") + typeToName(type));
    
    return BC_INVALID;
}

void TranslatorVisitor::throwError(string const & msg) {
    throw runtime_error(msg);
}

void TranslatorVisitor::localVariableInsn(AstVar const * var, bool is_load) {
    pair<index_t, index_t> var_desc = varmap_[var->name()].top();
    bool is_same_context = var_desc.first == currentFunctionId();
     
    auto insns_sets = var_insns_by_type_[var->type()];
    VarInsns insns = is_load ? insns_sets.first : insns_sets.second;
    
    if (!is_same_context) {
        bytecodeStream().addInsn(insns.ctx);
        bytecodeStream().addInt16(var_desc.first);
        bytecodeStream().addInt16(var_desc.second);
    } else {
        if (var_desc.second < 4) {
            bytecodeStream().addInsn((Instruction)(insns.optimized + var_desc.second));
        } else {
            bytecodeStream().addInsn(insns.local);
            bytecodeStream().addUInt16(var_desc.second);
        }
    }
}

void TranslatorVisitor::inlineCall(CallNode * node) {
    Label end_of_function(&bytecodeStream());

    AstFunction & function = *(scopes_.top()->lookupFunction(node->name()));
    inlining_contexts_.push(make_pair(&end_of_function, function.returnType()));

    for (size_t i = 0; i < function.parametersNumber(); ++i) {
        pushVar(function.parameterName(i));
    }

    for (int i = function.parametersNumber() - 1; i >= 0; --i) {
        AstVar var(function.parameterName(i), function.parameterType(i), scopes_.top());
        localVariableInsn(&var, false);
    }

    function.node()->body()->visit(this);

    for (size_t i = 0; i < function.parametersNumber(); ++i) {
        popVar(function.parameterName(i));
    }

    bytecodeStream().bind(end_of_function);
    inlining_contexts_.pop();
}

}
