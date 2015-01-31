
#ifndef FUNCTION_ANALYSIS_H
#define FUNCTION_ANALYSIS_H
#include <ast.h>
#include <mathvm.h>
#include <map>
#include <stack>
#include <visitors.h>
#include "common.h"

namespace mathvm {
    class FunctionAnalysisVisitor : AstBaseVisitor {
    public:

        FunctionAnalysisVisitor(Code & code) : code_(code) {}

        void processTopLevel(AstFunction & top_level) {
            addFunction(top_level, AstFunction::top_name);
            processFunction(top_level);

            findRecursiveFunctions();
        }

        void visitBlockNode(BlockNode * node) {
            pushScope(node->scope());

            Scope::FunctionIterator funcs_it(node->scope());

            while (funcs_it.hasNext()) {
                AstFunction * func = funcs_it.next();
                processFunction(*func);
            }

            node->visitChildren(this);

            popScope();
        }

        void pushScope(Scope * scope) {
            scopes_.push(scope);
            Scope::FunctionIterator funcs_it(scope);

            while (funcs_it.hasNext()) {
                AstFunction * func = funcs_it.next();
                if (!isFunctionNative(*func)) {
                    addFunction(*func, func->name());
                }
            }
        }

        void popScope() {
            Scope * scope = scopes_.top();
            scopes_.pop();
            Scope::FunctionIterator funcs_it(scope);

            while (funcs_it.hasNext()) {
                AstFunction * func = funcs_it.next();
                if (!isFunctionNative(*func)) {
                    funcmap_[func->name()].pop();
                }
            }
        }

        virtual void visitCallNode(CallNode * node) override {
            auto func = scopes_.top()->lookupFunction(node->name());

            if (!isFunctionNative(*func)) {
                index_t id = funcmap_[node->name()].top()->id();
                calls_[functions_stack_.top()].push_back(id);
                node->setInfo(funcmap_[node->name()].top());
            }

            node->visitChildren(this);
        }

        vector<bool> is_function_recursive() {
            return is_recursive_;
        }

    private:
        void findRecursiveFunctions() {
            is_recursive_.resize(calls_.size(), false);
            for (size_t i = 0; i < calls_.size(); i++) {
                vector<bool> visited(calls_.size(), false);
                if (isThereCallTo(i, i, visited)) {
                    is_recursive_[i] = true;
                }
            }
        }

        bool isThereCallTo(index_t id, index_t current_function_id, vector<bool> & visited) {
            visited[current_function_id] = true;

            for (index_t next_function_id : calls_[current_function_id]) {
                if (next_function_id == id) return true;
                if (!visited[next_function_id] && isThereCallTo(id, next_function_id, visited)) {
                    return true;
                }
            }

            return false;
        }

        void processFunction(AstFunction & function) {
            if (isFunctionNative(function)) {
                return;
            }

            functions_stack_.push(funcmap_[function.name()].top()->id());
            function.node()->body()->visit(this);
            functions_stack_.pop();
        }

        void addFunction(AstFunction & function, string const & name) {
            BytecodeFunction * bcf = new BytecodeFunction(&function);
            code_.addFunction(bcf);
            function.setInfo(bcf);
            funcmap_[name].push(bcf);

            if (calls_.size() <= bcf->id()) {
                calls_.resize(bcf->id() + 1);
            }
        }

        Code & code_;
        stack<Scope*> scopes_;
        stack<index_t> functions_stack_;
        vector<vector<index_t>> calls_;
        vector<bool> is_recursive_;
        map<string, stack<BytecodeFunction*>> funcmap_;

    };
}
#endif