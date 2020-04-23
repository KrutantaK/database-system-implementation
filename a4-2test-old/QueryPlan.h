#ifndef QUERY_PLAN_H_
#define QUERY_PLAN_H_

#include <iostream>
#include <vector>

#include "Schema.h"
#include "Function.h"
#include "ParseTree.h"
#include "Statistics.h"
#include "Comparison.h"

#define MAX_RELS 12
#define MAX_RELNAME 50
#define MAX_ATTS 100

class QueryNode;
class QueryPlan {
public:
  QueryPlan(Statistics* st);
  ~QueryPlan() {}

private:
  void makeLeafs();
  void makeJoins();
  void orderJoins();
  void makeSums();
  void makeProjects();
  void makeDistinct();
  void makeWrite();
  int evalOrder(std::vector<QueryNode*> operands, Statistics st, int bestFound); // intentional copy
  void print(std::ostream& os = std::cout);
  void printInOrder(QueryNode* node, std::ostream& os = std::cout);

  QueryNode* root;
  std::vector<QueryNode*> nodes;
  int num_selects;
  int num_joins;

  Statistics* stat;
  AndList* used;  // reconstruct AndList so that it can be used next time
                  // should be assigned to boolean after each round
  void recycleList(AndList* alist) { concatList(used, alist); }
  static void concatList(AndList*& left, AndList*& right);

  QueryPlan(const QueryPlan&);
  QueryPlan& operator=(const QueryPlan&);
};

class QueryNode {
  friend class QueryPlan;
  friend class UnaryNode;
  friend class BinaryNode;   // passed as argument to binary node
  friend class ProjectNode;
  friend class DedupNode;
  friend class JoinNode;
  friend class SumNode;
  friend class GroupByNode;
  friend class WriteNode;
public:
  virtual ~QueryNode();
  int num_children;

protected:
  QueryNode(const std::string& op, Schema* out, Statistics* st);
  QueryNode(const std::string& op, Schema* out, char* rName, Statistics* st);
  QueryNode(const std::string& op, Schema* out, char* rNames[], size_t num, Statistics* st);

  virtual void print(std::ostream& os = std::cout, size_t level = 0) const;
  virtual void printOperator(std::ostream& os = std::cout, size_t level = 0) const;
  virtual void printSchema(std::ostream& os = std::cout, size_t level = 0) const;
  virtual void printAnnot(std::ostream& os = std::cout, size_t level = 0) const = 0; // operator specific
  virtual void printPipe(std::ostream& os, size_t level = 0) const = 0;
  virtual void printChildren(std::ostream& os, size_t level = 0) const = 0;

  static AndList* pushSelection(AndList*& alist, Schema* target);
  static bool containedIn(OrList* ors, Schema* target);
  static bool containedIn(ComparisonOp* cmp, Schema* target);

  std::string opName;
  Schema* outSchema;
  char* relNames[MAX_RELS];
  size_t numRels;
  double estimate, cost;  // estimated number of tuples and total cost
  Statistics* stat;
  int pout;  // output pipe
  static int pipeId;
};

class LeafNode: public QueryNode {  // read from file
  friend class QueryPlan;
  LeafNode (AndList*& boolean, AndList*& pushed,
            char* relName, char* alias, Statistics* st);
  // void printOperator(std::ostream& os = std::cout, size_t level = 0) const;
  void printAnnot(std::ostream& os = std::cout, size_t level = 0) const;
  void printPipe(std::ostream& os, size_t level) const;
  void printChildren(std::ostream& os, size_t level) const {}
  CNF selOp;
  Record literal;
};

class UnaryNode: public QueryNode {
  friend class QueryPlan;
protected:
  UnaryNode(const std::string& opName, Schema* out, QueryNode* c, Statistics* st);
  virtual ~UnaryNode() { delete child; }
  void printPipe(std::ostream& os, size_t level) const;
  void printChildren(std::ostream& os, size_t level) const { child->print(os, level+1); }
  int pin;  // input pipe
public:
  QueryNode* child;
};

class BinaryNode: public QueryNode {  // not including set operations.
  friend class QueryPlan;
protected:
  BinaryNode(const std::string& opName, QueryNode* l, QueryNode* r, Statistics* st);
  virtual ~BinaryNode() { delete left; delete right; }
  void printPipe(std::ostream& os, size_t level) const;
  void printChildren(std::ostream& os, size_t level) const
  { left->print(os, level+1); right->print(os, level+1); }
  int pleft, pright; // input pipes
public:
  QueryNode* left;
  QueryNode* right;
};

class ProjectNode: private UnaryNode {
  friend class QueryPlan;
  ProjectNode(NameList* atts, QueryNode* c);
  void printAnnot(std::ostream& os = std::cout, size_t level = 0) const;
  int keepMe[MAX_ATTS];
  int numAttsIn, numAttsOut;
};

class DedupNode: private UnaryNode {
  friend class QueryPlan;
  DedupNode(QueryNode* c);
  void printAnnot(std::ostream& os = std::cout, size_t level = 0) const {}
  OrderMaker dedupOrder;
};

class SumNode: private UnaryNode {
  friend class QueryPlan;
  SumNode(FuncOperator* parseTree, QueryNode* c);
  Schema* resultSchema(FuncOperator* parseTree, QueryNode* c);
  void printAnnot(std::ostream& os = std::cout, size_t level = 0) const;
  Function f;
};

class GroupByNode: private UnaryNode {
  friend class QueryPlan;
  GroupByNode(NameList* gAtts, FuncOperator* parseTree, QueryNode* c);
  Schema* resultSchema(NameList* gAtts, FuncOperator* parseTree, QueryNode* c);
  void printAnnot(std::ostream& os = std::cout, size_t level = 0) const;
  OrderMaker grpOrder;
  Function f;
};

class JoinNode: private BinaryNode {
  friend class QueryPlan;
  JoinNode(AndList*& boolean, AndList*& pushed, QueryNode* l, QueryNode* r, Statistics* st);
  void printAnnot(std::ostream& os = std::cout, size_t level = 0) const;
  CNF selOp;
  Record literal;
};

class WriteNode: private UnaryNode {
  friend class QueryPlan;
  WriteNode(FILE* out, QueryNode* c);
  void print(std::ostream& os = std::cout, size_t level = 0) const {}
  void printAnnot(std::ostream& os = std::cout, size_t level = 0) const;
  FILE* outFile;
};

#endif