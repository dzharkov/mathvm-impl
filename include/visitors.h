#ifndef _MATHVM_VISITORS_H
#define _MATHVM_VISITORS_H

#include "mathvm.h"
#include "ast.h"

namespace mathvm {

class AstBaseVisitor : public AstVisitor {
  public:
    AstBaseVisitor() {
    }
    virtual ~AstBaseVisitor() {
    }

#define VISITOR_FUNCTION(type, name) \
    virtual void visit##type(type* node) { node->visitChildren(this); }

    FOR_NODES(VISITOR_FUNCTION)
#undef VISITOR_FUNCTION
};

class DeleteVisitor : public AstVisitor {
public:
    DeleteVisitor() {}

    void performDelete(AstNode* root) {
        if (root) {
            root->visit(this);
        }
    }

#define VISITOR_FUNCTION(type, name)            \
    virtual void visit##type(type* node) {      \
      assert(!node->isFunctionNode());          \
      node->visitChildren(this);                \
      delete node;                              \
    }
    FOR_NODES(VISITOR_FUNCTION)
#undef VISITOR_FUNCTION
};

class AstDumper : public AstVisitor {
public:
    AstDumper() {}

    void dump(AstNode* root);

#define VISITOR_FUNCTION(type, name)            \
    virtual void visit##type(type* node);

    FOR_NODES(VISITOR_FUNCTION)
#undef VISITOR_FUNCTION
};

}

#endif // _MATHVM_VISITORS_H
