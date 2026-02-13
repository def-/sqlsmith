/// @file
/// @brief grammar: Value expression productions

#ifndef EXPR_HH
#define EXPR_HH

#include "prod.hh"
#include <string>

using std::shared_ptr;
using std::vector;
using std::string;

struct value_expr: prod {
  sqltype *type;
  virtual void out(std::ostream &out) = 0;
  virtual ~value_expr() { }
  value_expr(prod *p) : prod(p) { }
  static shared_ptr<value_expr> factory(prod *p, sqltype *type_constraint, bool can_return_set);
};

struct case_expr : value_expr {
  shared_ptr<value_expr> condition;
  shared_ptr<value_expr> true_expr;
  shared_ptr<value_expr> false_expr;
  vector<std::pair<shared_ptr<value_expr>, shared_ptr<value_expr>>> extra_when_clauses;
  case_expr(prod *p, sqltype *type_constraint = 0);
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct column_reference;

struct funcall : value_expr {
  routine *proc;
  bool is_aggregate;
  vector<shared_ptr<value_expr> > parms;
  shared_ptr<struct bool_expr> filter;
  vector<shared_ptr<value_expr> > agg_order_by;
  virtual void out(std::ostream &out);
  virtual ~funcall() { }
  funcall(prod *p, sqltype *type_constraint = 0, bool can_return_set = false, bool agg = false);
  virtual void accept(prod_visitor *v);
};

struct opcall : value_expr {
  op *oper;
  shared_ptr<value_expr> lhs, rhs;
  virtual ~opcall() { }
  opcall(prod *p, sqltype *type_constraint = 0);
  virtual void out(std::ostream &o) {
    if(oper->left)
      o << "(" << *lhs << ") " << oper->name << " (" << *rhs << ")";
    else
      o << oper->name << " " << *rhs;
  }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    if(oper->left)
      lhs->accept(v);
    rhs->accept(v);
  }
};

struct atomic_subselect : value_expr {
  table *tab;
  column *col;
  int offset;
  routine *agg;
  atomic_subselect(prod *p, sqltype *type_constraint = 0);
  virtual void out(std::ostream &out);
};

struct const_expr: value_expr {
  std::string expr;
  const_expr(prod *p, sqltype *type_constraint = 0);
  virtual void out(std::ostream &out) { out << expr; }
  virtual ~const_expr() { }
};

struct column_reference: value_expr {
  column_reference(prod *p, sqltype *type_constraint = 0);
  virtual void out(std::ostream &out) { out << reference; }
  std::string reference;
  virtual ~column_reference() { }
};

struct coalesce : value_expr {
  const char *abbrev_;
  vector<shared_ptr<value_expr> > value_exprs;
  virtual ~coalesce() { };
  coalesce(prod *p, sqltype *type_constraint = 0, const char *abbrev = "coalesce");
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto p : value_exprs)
      p->accept(v);
  }
};

struct nullif : coalesce {
 virtual ~nullif() { };
     nullif(prod *p, sqltype *type_constraint = 0)
	  : coalesce(p, type_constraint, "nullif")
	  { };
};

struct bool_expr : value_expr {
  virtual ~bool_expr() { }
  bool_expr(prod *p) : value_expr(p) { type = scope->schema->booltype; }
  static shared_ptr<bool_expr> factory(prod *p);
};

struct truth_value : bool_expr {
  virtual ~truth_value() { }
  const char *op;
  virtual void out(std::ostream &out) { out << op; }
  truth_value(prod *p) : bool_expr(p) {
    op = ( (d6() < 4) ? scope->schema->true_literal : scope->schema->false_literal);
  }
};

struct null_predicate : bool_expr {
  virtual ~null_predicate() { }
  const char *negate;
  shared_ptr<value_expr> expr;
  null_predicate(prod *p) : bool_expr(p) {
    negate = ((d6()<4) ? "not " : "");
    expr = value_expr::factory(this, nullptr, false);
  }
  virtual void out(std::ostream &out) {
    out << *expr << " is " << negate << "NULL";
  }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    expr->accept(v);
  }
};

struct exists_predicate : bool_expr {
  shared_ptr<struct query_spec> subquery;
  virtual ~exists_predicate() { }
  exists_predicate(prod *p);
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct bool_binop : bool_expr {
  shared_ptr<value_expr> lhs, rhs;
  bool_binop(prod *p) : bool_expr(p) { }
  virtual void out(std::ostream &out) = 0;
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    lhs->accept(v);
    rhs->accept(v);
  }
};

struct bool_term : bool_binop {
  virtual ~bool_term() { }
  const char *op;
  virtual void out(std::ostream &out) {
    out << "(" << *lhs << ") ";
    indent(out);
    out << op << " (" << *rhs << ")";
  }
  bool_term(prod *p) : bool_binop(p)
  {
    op = ((d6()<4) ? "or" : "and");
    lhs = bool_expr::factory(this);
    rhs = bool_expr::factory(this);
  }
};

struct distinct_pred : bool_binop {
  distinct_pred(prod *p);
  virtual ~distinct_pred() { };
  virtual void out(std::ostream &o) {
    o << *lhs << " is distinct from " << *rhs;
  }
};

struct comparison_op : bool_binop {
  op *oper;
  comparison_op(prod *p);
  virtual ~comparison_op() { };
  virtual void out(std::ostream &o) {
    o << "(" << *lhs << ") " << oper->name << " (" << *rhs << ")";
  }
};

struct window_function : value_expr {
  virtual void out(std::ostream &out);
  virtual ~window_function() { }
  window_function(prod *p, sqltype *type_constraint);
  vector<shared_ptr<column_reference> > partition_by;
  vector<shared_ptr<column_reference> > order_by;
  shared_ptr<funcall> aggregate;
  string frame_clause;
  static bool allowed(prod *pprod);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    aggregate->accept(v);
    for (auto p : partition_by)
      p->accept(v);
    for (auto p : order_by)
      p->accept(v);
  }
};

struct between_expr : bool_expr {
  shared_ptr<value_expr> expr;
  shared_ptr<value_expr> lo;
  shared_ptr<value_expr> hi;
  bool negated;
  between_expr(prod *p);
  virtual ~between_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    expr->accept(v);
    lo->accept(v);
    hi->accept(v);
  }
};

struct like_expr : bool_expr {
  shared_ptr<value_expr> expr;
  string pattern;
  bool is_ilike;
  like_expr(prod *p);
  virtual ~like_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    expr->accept(v);
  }
};

struct in_expr : bool_expr {
  shared_ptr<value_expr> expr;
  vector<shared_ptr<value_expr>> value_list;
  shared_ptr<struct query_spec> subquery;
  bool negated;
  bool use_subquery;
  in_expr(prod *p);
  virtual ~in_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct cast_expr : value_expr {
  shared_ptr<value_expr> inner;
  string target_type;
  cast_expr(prod *p, sqltype *type_constraint = 0);
  virtual ~cast_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    inner->accept(v);
  }
};

struct temporal_filter : bool_expr {
  shared_ptr<column_reference> col_ref;
  string interval_str;
  temporal_filter(prod *p);
  virtual ~temporal_filter() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    col_ref->accept(v);
  }
};

struct bool_test : bool_expr {
  shared_ptr<value_expr> expr;
  string test_type;
  bool_test(prod *p);
  virtual ~bool_test() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    expr->accept(v);
  }
};

struct not_expr : bool_expr {
  shared_ptr<bool_expr> inner;
  not_expr(prod *p);
  virtual ~not_expr() { }
  virtual void out(std::ostream &out) { out << "NOT (" << *inner << ")"; }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    inner->accept(v);
  }
};

struct any_all_expr : bool_expr {
  shared_ptr<value_expr> lhs;
  shared_ptr<struct query_spec> subquery;
  string cmp_op;
  string quantifier;
  any_all_expr(prod *p);
  virtual ~any_all_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct greatest_least : value_expr {
  string func_name;
  vector<shared_ptr<value_expr>> args;
  greatest_least(prod *p, sqltype *type_constraint = 0);
  virtual ~greatest_least() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto &a : args) a->accept(v);
  }
};

struct row_constructor : value_expr {
  vector<shared_ptr<value_expr>> elems;
  row_constructor(prod *p);
  virtual ~row_constructor() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto &e : elems) e->accept(v);
  }
};

struct array_subscript : value_expr {
  shared_ptr<value_expr> arr;
  int index_val;
  array_subscript(prod *p, sqltype *type_constraint = 0);
  virtual ~array_subscript() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    arr->accept(v);
  }
};

struct jsonb_access : value_expr {
  shared_ptr<value_expr> obj;
  string accessor;
  bool returns_text;
  jsonb_access(prod *p, sqltype *type_constraint = 0);
  virtual ~jsonb_access() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    obj->accept(v);
  }
};

struct dedicated_window_func : value_expr {
  string func_name;
  vector<shared_ptr<value_expr>> args;
  vector<shared_ptr<column_reference>> partition_by;
  vector<shared_ptr<column_reference>> order_by;
  dedicated_window_func(prod *p, sqltype *type_constraint = 0);
  virtual ~dedicated_window_func() { }
  virtual void out(std::ostream &out);
  static bool allowed(prod *pprod);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto &a : args) a->accept(v);
    for (auto &p : partition_by) p->accept(v);
    for (auto &o : order_by) o->accept(v);
  }
};

struct position_expr : value_expr {
  shared_ptr<value_expr> substring_expr;
  shared_ptr<value_expr> string_expr;
  position_expr(prod *p, sqltype *type_constraint = 0);
  virtual ~position_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    substring_expr->accept(v);
    string_expr->accept(v);
  }
};

struct substring_expr : value_expr {
  shared_ptr<value_expr> string_expr;
  int from_pos;
  int for_len;
  substring_expr(prod *p, sqltype *type_constraint = 0);
  virtual ~substring_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    string_expr->accept(v);
  }
};

struct extract_expr : value_expr {
  shared_ptr<value_expr> source;
  string field;
  extract_expr(prod *p, sqltype *type_constraint = 0);
  virtual ~extract_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    source->accept(v);
  }
};

#endif
