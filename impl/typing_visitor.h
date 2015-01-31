#ifndef TYPING_VISITOR
#define TYPING_VISITOR
#include "mathvm.h"
#include "visitors.h"
#include "parser.h"
#include "common.h"
#include <map>
#include <string>
#include <exception>
#include <stack>
#include <vector>
#include <dlfcn.h>
namespace mathvm { 


class TypingVisitor : public AstBaseVisitor {
public:
    TypingVisitor(map<AstNode *, VarType> & types_map, map<AstFunction *, void *> & natives_map) 
    : types_map_(types_map), natives_map_(natives_map) {
            
    }
    
    void visitBlockNode(BlockNode* node) { 
        scopes_.push(node->scope()); 
        
        Scope::FunctionIterator funcs_it(node->scope());

        while (funcs_it.hasNext()) {
            AstFunction * func = funcs_it.next();

            current_function_return_type_.push(func->returnType());
            
            if (isFunctionNative(*func)) {
                processNativeFunction(func);
            } else {
                func->node()->visit(this);
            }
            current_function_return_type_.pop();
        }
        
        processStatement(node);
        
        scopes_.pop(); 
    }

    virtual void visitBinaryOpNode(BinaryOpNode* node) {
        assertThat(node->kind() != tRANGE, "Range binary operator");

        VarType a = typeOf(node->left());
        VarType b = typeOf(node->right());
        VarType lup = typeLup(a, b);
        
        assertThat(lup != VT_STRING, "bin op for string");

        TokenKind op = node->kind();
        if ((op >= tEQ && op <= tLE) || op == tMOD) {
            lup = VT_INT;
        } else if (op >= tOR && op <= tAXOR) {
            assertThat(lup == VT_INT, "logic operation for " + string(typeToName(lup)));
        }

        markAs(node, lup);
    }
    
    virtual void visitUnaryOpNode(UnaryOpNode* node) {
        VarType a = typeOf(node->operand());
         
        if (node->kind() == tNOT) {
            markAs(node, VT_INT);
        } else {
            markAs(node, typeLup(a, VT_INT));
        }
    }
    
    virtual void visitStringLiteralNode(StringLiteralNode* node) {
        markAs(node, VT_STRING);
    }
    
    virtual void visitDoubleLiteralNode(DoubleLiteralNode* node) {
        markAs(node, VT_DOUBLE);
    }

    virtual void visitIntLiteralNode(IntLiteralNode* node) {
        markAs(node, VT_INT);
    }
    
    virtual void visitLoadNode(LoadNode* node) {
        markAs(node, node->var()->type());
    }

    virtual void visitStoreNode(StoreNode* node) {
        processStatement(node);
        checkIsAssignable(typeOf(node->value()), node->var()->type());

        if (node->op() != tASSIGN) {
            assertThat(node->var()->type() != VT_STRING, "decr/incr on string");
        }
    }   
    
    virtual void visitForNode(ForNode* node) {
        assertThat(node->var() != 0, "undeclared var in for");
        if (node->var() == 0) return;
        assertThat(scopes_.top()->lookupVariable(node->var()->name()) != 0, "undeclared var " + node->var()->name());
        assertThat(node->var()->type() == VT_INT, "type of iterable should be int");
        
        assertThat(node->inExpr()->isBinaryOpNode() && node->inExpr()->asBinaryOpNode()->kind() == tRANGE, "in expresssion should be range");
        
        if (errors_.size() == 0) {
            BinaryOpNode * inExpr = node->inExpr()->asBinaryOpNode();
            
            checkIsAssignable(typeOf(inExpr->left()), VT_INT);
            checkIsAssignable(typeOf(inExpr->right()), VT_INT);
        }
        node->body()->visit(this);
        markAs(node, VT_VOID);
    }
    
    virtual void visitWhileNode(WhileNode* node) {
        processStatement(node);
        checkIsAssignable(typeOf(node->whileExpr()), VT_INT);
    }
    
    virtual void visitIfNode(IfNode* node) {
        processStatement(node);
        checkIsAssignable(typeOf(node->ifExpr()), VT_INT);
    }
    
    virtual void visitReturnNode(ReturnNode* node) {
        processStatement(node);
        
        VarType ret_type = node->returnExpr() != 0 ? typeOf(node->returnExpr()) : VT_VOID;
        
        if (current_function_return_type_.empty()) return; // top level

        checkIsAssignable(ret_type, current_function_return_type_.top());
    }
    
    virtual void visitCallNode(CallNode* node) {
        node->visitChildren(this);
        auto func = scopes_.top()->lookupFunction(node->name());
        markAs(node, func->returnType());
        
        assertThat(node->parametersNumber() == func->parametersNumber(), "wrong args number for " + node->name());

        for (size_t i = 0; i < func->parametersNumber(); ++i) {
            checkIsAssignable(typeOf(node->parameterAt(i)), func->parameterType(i));
        }
    }

    virtual void visitPrintNode(PrintNode* node) {
        processStatement(node);

        for (size_t i = 0; i < node->operands(); ++i) {
            assertThat(typeOf(node->operandAt(i)) != VT_VOID, "printing void");
        }
    }

    vector<string> const & errors() const {
        return errors_;
    }

private:
    void processNativeFunction(AstFunction * func) {
        NativeCallNode * call = func->node()->body()->nodeAt(0)->asNativeCallNode();
        
        void * address = dlsym(RTLD_DEFAULT, call->nativeName().c_str());
        
        if (address == 0) {
            throwError("native " + call->nativeName() + " not found");
        }

        natives_map_[func] = address;
    }
    
    void processStatement(AstNode * node) {
        node->visitChildren(this);
        markAs(node, VT_VOID);
    }
    
    void markAs(AstNode * node, VarType type) {
        types_map_[node] = type;
        last_type_ = type;
    }

    VarType typeOf(AstNode * node) {
        auto type_iter = types_map_.find(node);
        if (type_iter != types_map_.end()) return type_iter->second;

        node->visit(this);
        return last_type_;
    }
    
    void checkIsAssignable(VarType from, VarType to) {
        if (from == to) {
            return;
        }
        
        if (to == VT_VOID) {
            return;
        }
        
        assertThat(to != VT_STRING && from != VT_VOID, "There is not conversion from " + string(typeToName(from)) + " to " + string(typeToName(to)));
    }
    
    VarType typeLup(VarType a, VarType b) {
        if (a == b) return a;
        assertThat(a != VT_VOID && b != VT_VOID, string("Unknown typeLup for ") + typeToName(a) + " " + typeToName(b));
        
        if (a == VT_DOUBLE || b == VT_DOUBLE) {
            //assertThat(a == VT_INT || b == VT_INT, string("Unknown typeLup for ") + typeToName(a) + " " + typeToName(b));
            return VT_DOUBLE;   
        }


        //throwError(string("Unknown typeLup for ") + typeToName(a) + " " + typeToName(b));
        return VT_INT;
    }

    void throwError(string const & msg) {
        errors_.push_back(msg);
    }

    void assertThat(bool value, string const & msg) {
        if (!value) {
            throwError(msg);
        }
    }
    
    map<AstNode *, VarType> & types_map_;
    map<AstFunction *, void *> & natives_map_;
    VarType last_type_;
    stack<Scope *> scopes_;
    stack<VarType> current_function_return_type_;
    Scope * current_scope_ = nullptr;
    vector<string> errors_;
};

}
#endif
