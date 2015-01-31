#ifndef TRANSLATOR_IMPL
#define TRANSLATOR_IMPL
#include "mathvm.h"
#include "visitors.h"
#include "common.h"

#include <map>
#include <string>
#include <stack>
#include <memory>
#include <exception>

namespace mathvm { 

struct VarInsns {
    Instruction local, ctx, optimized;
};

class TranslatorVisitor : public AstBaseVisitor {
public:
    TranslatorVisitor(Code & code, map<AstNode*, VarType> & types_map, map<AstFunction*, void *> & natives_map) 
        : code_(code), types_map_(types_map), natives_map_(natives_map) {
        fillVarInsns(); 
    }
    
    void visitBlockNode(BlockNode* node); 

    void visitBinaryOpNode(BinaryOpNode* node);
    
    void visitUnaryOpNode(UnaryOpNode* node);
    
    void visitStringLiteralNode(StringLiteralNode* node);
    
    void visitDoubleLiteralNode(DoubleLiteralNode* node);
    
    void visitIntLiteralNode(IntLiteralNode* node);
    
    void visitLoadNode(LoadNode* node);

    void visitStoreNode(StoreNode* node);
    
    void visitForNode(ForNode* node);
    
    void visitWhileNode(WhileNode* node);

    void visitIfNode(IfNode* node);
    
    void visitReturnNode(ReturnNode* node);
    
    void visitCallNode(CallNode* node);
    void visitNativeCallNode(NativeCallNode* node);
    
    void visitPrintNode(PrintNode* node);

    void processTopLevel(AstFunction & top_level);
private:
    void processFunction(AstFunction & function);
    
    void pushScope(Scope * scope);
    void popScope();
    
    void pushVar(string const & name);
    void popVar(string const & name);

    void inlineCall(CallNode * node);

    AstVar genTempVar(VarType type);
    
    void gen(AstNode * node, VarType type);
    
    VarType gen(AstNode * node);

    VarType typeOf(AstNode * node);

    void coerceStackTop(VarType from, VarType to);

    Bytecode & bytecodeStream();

    BytecodeFunction * currentBytecodeFunction();

    index_t currentFunctionId();

    Instruction chooseInsn(VarType type, Instruction intInsn, Instruction doubleInsn);

    void throwError(string const & msg);

    void localVariableInsn(AstVar const * var, bool is_load);
    void generateComparision(BinaryOpNode * node);
    
    index_t & current_vars_count() {
        return vars_count_.top();
    }

    Code & code_;
    map<AstNode*, VarType> & types_map_;
    map<AstFunction*, void *> & natives_map_;
    stack<BytecodeFunction *> bytecode_functions_stack_;  
    stack<index_t> vars_count_; 
    map<string, stack<pair<index_t, index_t>>> varmap_;
    map<string, stack<index_t>> native_funcmap_;
    
    stack<Scope*> scopes_;

    vector<bool> is_function_recursive_;
    stack<pair<Label*, VarType>> inlining_contexts_;

    void fillVarInsns() {
        pair<VarInsns, VarInsns> insns;
#define VAR_INSNS(t, name) \
        insns.first.local = BC_LOAD##t##VAR; \
        insns.second.local = BC_STORE##t##VAR; \
        insns.first.ctx = BC_LOADCTX##t##VAR; \
        insns.second.ctx = BC_STORECTX##t##VAR; \
        insns.first.optimized = BC_LOAD##t##VAR0; \
        insns.second.optimized = BC_STORE##t##VAR0; \
        var_insns_by_type_[name] = insns;
        
        VAR_INSNS(I, VT_INT);
        VAR_INSNS(D, VT_DOUBLE);
        VAR_INSNS(S, VT_STRING);
#undef VAR_INSNS
    }   

    map<VarType, pair<VarInsns, VarInsns>> var_insns_by_type_;
};

}
#endif
