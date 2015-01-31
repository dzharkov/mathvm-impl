#include "mathvm.h"
#include "visitors.h"
#include "parser.h"
#include <map>

namespace mathvm {
    struct AstPrinterVisitor : AstBaseVisitor {
        AstPrinterVisitor(ostream & os) : 
            os_(os), tab_level_(1) 
        {
            init_tokens();
        }

        void printScopeVars(Scope * scope) {
            Scope::VarIterator var_iter(scope);

            while (var_iter.hasNext()) {
                AstVar* var = var_iter.next();
                append_tabs();
                os_ << typeName(var->type()) << " " << var->name() << ";" << endl;
            }
        }

        virtual void visitBlockNode(BlockNode* node) { 
            bool is_top_level = tab_level_++ == 0;
            if (!is_top_level) {
                os_ << " {" << endl;
                printScopeVars(node->scope());
            }

            for (size_t i = 0; i < node->nodes(); ++i) {
                append_tabs();
                node->nodeAt(i)->visit(this);
                
                if (node->nodeAt(i)->isCallNode()) {
                    os_ << ";";
                }
                
                os_ << endl;
            }
            
            --tab_level_;
            append_tabs();
            if (!is_top_level) {
                os_ << "}" << endl;
            }
        }

        virtual void visitBinaryOpNode(BinaryOpNode* node) {
            os_ << "(";
            node->left()->visit(this);
            os_ << tokens_[node->kind()];
            node->right()->visit(this);
            os_ << ")";
        }
        
        virtual void visitUnaryOpNode(UnaryOpNode* node) {
            os_ << "(";
            os_ << tokens_[node->kind()];
            node->operand()->visit(this);
            os_ << ")";
        }
        
        virtual void visitStringLiteralNode(StringLiteralNode* node) {
            os_ << "'" << escapeString(node->literal()) << "'";
        }
        
        virtual void visitDoubleLiteralNode(DoubleLiteralNode* node) {
            os_ << showpoint << node->literal();
        }
        virtual void visitIntLiteralNode(IntLiteralNode* node) {
            os_ << node->literal();
        }
        
        virtual void visitLoadNode(LoadNode* node) {
            os_ << node->var()->name(); 
        }

        virtual void visitStoreNode(StoreNode* node) {
            os_ << node->var()->name() << " = ";
            node->value()->visit(this);
            os_ << ";";
        }
        
        virtual void visitForNode(ForNode* node) {
            os_ << "for (" << node->var()->name() << " in ";
            node->inExpr()->visit(this);
            os_ << ")";
            node->body()->visit(this);
        }
        
        virtual void visitWhileNode(WhileNode* node) {
            os_ << "while (";
            node->whileExpr()->visit(this);
            os_ << ")";
            node->loopBlock()->visit(this);
        }
        
        virtual void visitIfNode(IfNode* node) {
            os_ << "if (";
            node->ifExpr()->visit(this);
            os_ << ")";
            node->thenBlock()->visit(this);

            if (node->elseBlock() != NULL) {
                os_ << " else ";
                node->elseBlock()->visit(this);
            }
        }
        
        virtual void visitFunctionNode(FunctionNode* node) {
            if (node->name() != AstFunction::top_name) {
                os_ << "function " << typeName(node->returnType()) << " " << node->name() << "(";
                for (size_t i = 0; i < node->parametersNumber(); ++i) {
                    os_ << typeName(node->parameterType(i)) << " " << node->parameterName(i);
                    if (i + 1 < node->parametersNumber()) {
                        os_ << ", ";
                    }
                }
                os_ << ")";
            } else {
                tab_level_ = 0;
            }
            
            if (node->body()->nodes() > 0 && node->body()->nodeAt(0)->isNativeCallNode()) {
                os_ << " native '" << node->body()->nodeAt(0)->asNativeCallNode()->nativeName() << "';";
                os_ << endl;
            } else {
                node->body()->visit(this);
            }
        }
        
        virtual void visitReturnNode(ReturnNode* node) {
            if (node->returnExpr() == NULL) {
                os_ << "return;";
            } else {
                os_ << "return ";
                node->returnExpr()->visit(this);
                os_ << ";";
            }
        }
        
        virtual void visitCallNode(CallNode* node) {
            os_ << node->name() << "(";
            for (size_t i = 0; i < node->parametersNumber(); ++i) {
                node->parameterAt(i)->visit(this);
                if (i + 1 < node->parametersNumber()) {
                    os_ << ", ";
                }
            }
            os_ << ")";
        }
        
        virtual void visitPrintNode(PrintNode* node) {
            os_ << "print(";
            for (size_t i = 0; i < node->operands(); ++i) {
                node->operandAt(i)->visit(this);
                if (i + 1 < node->operands()) {
                    os_ << ", ";
                }
            }
            os_ << ");";
        }

        private:
            static void init_tokens() {
                if (tokens_.size() == 0) {
#define TOKEN_ELEM(t, s, p) tokens_[t] = s;
                    FOR_TOKENS(TOKEN_ELEM)
#undef TOKEN_ELEM
                }
            }
            
            void append_tabs() {
                for (size_t i = 1; i < tab_level_; ++i) {
                    os_ << "\t";
                }
            }
            
            string typeName(VarType type) {
                switch(type) {
                    case VT_VOID:
                        return "void";
                    case VT_INT:
                        return "int";
                    case VT_DOUBLE:
                        return "double";
                    case VT_STRING:
                        return "string";
                    case VT_INVALID:
                        return "invalid";
                }
                return "<unknown>";
            }
            
            string escapeString(const string & s) {
                string result;
                for (size_t i = 0; i < s.length(); ++i) {
                    string c(s.substr(i, 1));
                    switch(s[i]) {
                        case '\n': c = "\\n"; break;
                        case '\t': c = "\\t"; break;
                        case '\r': c = "\\r"; break;
                        case '\\': c = "\\\\"; break;
                        case '\'': c = "\\'"; break;
                        default:
                            break;
                            
                    }
                    result += c;
                }

                return result;
            } 

            static map<TokenKind, string> tokens_;
            ostream & os_;
            size_t tab_level_;
    };

    map<TokenKind, string> AstPrinterVisitor::tokens_;

    struct AstPrinter : Translator {
        virtual Status* translate(string const & src, Code ** code) {
            Parser parser;
            Status * status = parser.parseProgram(src);
            if (status != NULL && status->isError()) return status;
            
            Scope::FunctionIterator it(parser.top()->scope()->childScopeAt(0));
            AstPrinterVisitor visitor(cout);
            
            visitor.printScopeVars(parser.top()->scope()->childScopeAt(0));

            while (it.hasNext()) {
                it.next()->node()->visit(&visitor);   
            }

            AstNode * root = parser.top()->node();
             
            root->visit(&visitor);

            assert(root != NULL);
            
            return Status::Ok();
        }
    };
    

}
