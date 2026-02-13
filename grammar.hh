/// @file
/// @brief grammar: Top-level and unsorted grammar productions

#ifndef GRAMMAR_HH
#define GRAMMAR_HH

#include <ostream>
#include "relmodel.hh"
#include <memory>
#include "schema.hh"

#include "prod.hh"
#include "expr.hh"

using std::shared_ptr;

static int g_joins = 1;

struct table_ref : prod {
  vector<shared_ptr<named_relation> > refs;
  static shared_ptr<table_ref> factory(prod *p);
  table_ref(prod *p) : prod(p) { }
  virtual ~table_ref() { }
};

struct table_or_query_name : table_ref {
  virtual void out(std::ostream &out);
  table_or_query_name(prod *p);
  virtual ~table_or_query_name() { }
  named_relation *t;
};

struct target_table : table_ref {
  virtual void out(std::ostream &out);
  target_table(prod *p, table *victim = 0);
  virtual ~target_table() { }
  table *victim_;
};

struct table_sample : table_ref {
  virtual void out(std::ostream &out);
  table_sample(prod *p);
  virtual ~table_sample() { }
  struct table *t;
private:
  string method;
  double percent;
};

struct table_subquery : table_ref {
  bool is_lateral;
  virtual void out(std::ostream &out);
  shared_ptr<struct query_spec> query;
  table_subquery(prod *p, bool lateral = false);
  virtual ~table_subquery();
  virtual void accept(prod_visitor *v);
};

struct lateral_subquery : table_subquery {
  lateral_subquery(prod *p)
    : table_subquery(p, true) {  }
};

struct join_cond : prod {
     static shared_ptr<join_cond> factory(prod *p, table_ref &lhs, table_ref &rhs);
     join_cond(prod *p, table_ref &lhs, table_ref &rhs)
	  : prod(p) { (void) lhs; (void) rhs;}
};

struct simple_join_cond : join_cond {
     std::string condition;
     simple_join_cond(prod *p, table_ref &lhs, table_ref &rhs);
     virtual void out(std::ostream &out);
};

struct expr_join_cond : join_cond {
     struct scope joinscope;
     shared_ptr<bool_expr> search;
     expr_join_cond(prod *p, table_ref &lhs, table_ref &rhs);
     virtual void out(std::ostream &out);
     virtual void accept(prod_visitor *v) {
	  search->accept(v);
	  v->visit(this);
     }
};

struct joined_table : table_ref {
  virtual void out(std::ostream &out);  
  joined_table(prod *p);
  std::string type;
  std::string alias;
  virtual std::string ident() { return alias; }
  shared_ptr<table_ref> lhs;
  shared_ptr<table_ref> rhs;
  shared_ptr<join_cond> condition;
  virtual ~joined_table() {
  }
  virtual void accept(prod_visitor *v) {
    lhs->accept(v);
    rhs->accept(v);
    condition->accept(v);
    v->visit(this);
  }
};

struct from_clause : prod {
  std::vector<shared_ptr<table_ref> > reflist;
  virtual void out(std::ostream &out);
  from_clause(prod *p);
  ~from_clause() { }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto p : reflist)
      p->accept(v);
  }
};

struct select_list : prod {
  std::vector<shared_ptr<value_expr> > value_exprs;
  relation derived_table;
  int columns = 0;
  select_list(prod *p);
  virtual void out(std::ostream &out);
  ~select_list() { }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto p : value_exprs)
      p->accept(v);
  }
};

struct query_spec : prod {
  std::string set_quantifier;
  shared_ptr<struct from_clause> from_clause;
  shared_ptr<struct select_list> select_list;
  shared_ptr<bool_expr> search;
  bool has_group_by = false;
  std::vector<shared_ptr<column_reference>> group_by_cols;
  shared_ptr<bool_expr> having;
  bool has_order_by = false;
  std::vector<shared_ptr<column_reference>> order_by_cols;
  bool has_limit = false;
  int limit_val = 0;
  bool has_offset = false;
  int offset_val = 0;
  bool has_distinct_on = false;
  std::vector<shared_ptr<column_reference>> distinct_on_cols;
  struct scope myscope;
  virtual void out(std::ostream &out);
  query_spec(prod *p, struct scope *s, bool lateral = 0);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    select_list->accept(v);
    from_clause->accept(v);
    search->accept(v);
    for (auto &c : group_by_cols) c->accept(v);
    if (having) having->accept(v);
    for (auto &c : order_by_cols) c->accept(v);
    for (auto &c : distinct_on_cols) c->accept(v);
  }
};

struct select_for_update : query_spec {
  const char *lockmode;
  virtual void out(std::ostream &out);
  select_for_update(prod *p, struct scope *s, bool lateral = 0);
};

struct prepare_stmt : prod {
  query_spec q;
  static long seq;
  long id;
  virtual void out(std::ostream &out) {
    out << "prepare prep" << id << " as " << q;
  }
  prepare_stmt(prod *p) : prod(p), q(p, scope) {
    id = seq++;
  }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    q.accept(v);
  }
};

struct modifying_stmt : prod {
  table *victim;
  struct scope myscope;
  modifying_stmt(prod *p, struct scope *s, struct table *victim = 0);
//   shared_ptr<modifying_stmt> modifying_stmt::factory(prod *p, struct scope *s);
  virtual void pick_victim();
};

struct delete_stmt : modifying_stmt {
  shared_ptr<bool_expr> search;
  delete_stmt(prod *p, struct scope *s, table *v);
  virtual ~delete_stmt() { }
  virtual void out(std::ostream &out) {
    out << "delete from " << victim->ident();
    indent(out);
    out << "where " << std::endl << *search;
  }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    search->accept(v);
  }
};

struct delete_returning : delete_stmt {
  shared_ptr<struct select_list> select_list;
  delete_returning(prod *p, struct scope *s, table *victim = 0);
  virtual void out(std::ostream &out) {
    delete_stmt::out(out);
    // Syntax: ERROR:  Expected end of statement, found RETURNING
    //out << std::endl << "returning " << *select_list;
  }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    search->accept(v);
    select_list->accept(v);
  }
};

struct comment_stmt : modifying_stmt {
  shared_ptr<column> victim_column;
  comment_stmt(prod *p, struct scope *s, table *victim = 0);
  virtual ~comment_stmt() {  }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct insert_stmt : modifying_stmt {
  vector<shared_ptr<value_expr> > value_exprs;
  insert_stmt(prod *p, struct scope *s, table *victim = 0);
  virtual ~insert_stmt() {  }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto p : value_exprs) p->accept(v);
  }
};

struct set_list : prod {
  vector<shared_ptr<value_expr> > value_exprs;
  vector<string> names;
  set_list(prod *p, table *target);
  virtual ~set_list() {  }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto p : value_exprs) p->accept(v);
  }
};

struct upsert_stmt : insert_stmt {
  shared_ptr<struct set_list> set_list;
  string constraint;
  shared_ptr<bool_expr> search;
  upsert_stmt(prod *p, struct scope *s, table *v = 0);
  virtual void out(std::ostream &out) {
    insert_stmt::out(out);
    out << " on conflict on constraint " << constraint << " do update ";
    out << *set_list << " where " << *search;
  }
  virtual void accept(prod_visitor *v) {
    insert_stmt::accept(v);
    set_list->accept(v);
    search->accept(v);
  }
  virtual ~upsert_stmt() {  }
};

struct update_stmt : modifying_stmt {
  shared_ptr<bool_expr> search;
  shared_ptr<struct set_list> set_list;
  update_stmt(prod *p, struct scope *s, table *victim = 0);
  virtual ~update_stmt() {  }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    search->accept(v);
  }
};

struct when_clause : prod {
  bool matched;
  shared_ptr<bool_expr> condition;  
//   shared_ptr<prod> merge_action;
  when_clause(struct merge_stmt *p);
  virtual ~when_clause() { }
  static shared_ptr<when_clause> factory(struct merge_stmt *p);
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct when_clause_update : when_clause {
  shared_ptr<struct set_list> set_list;
  struct scope myscope;
  when_clause_update(struct merge_stmt *p);
  virtual ~when_clause_update() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct when_clause_insert : when_clause {
  vector<shared_ptr<value_expr> > exprs;
  when_clause_insert(struct merge_stmt *p);
  virtual ~when_clause_insert() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct merge_stmt : modifying_stmt {
  merge_stmt(prod *p, struct scope *s, table *victim = 0);
  shared_ptr<table_ref> target_table_;
  shared_ptr<table_ref> data_source;
  shared_ptr<join_cond> join_condition;
  vector<shared_ptr<when_clause> > clauselist;
  virtual ~merge_stmt() {  }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
};

struct update_returning : update_stmt {
  shared_ptr<struct select_list> select_list;
  update_returning(prod *p, struct scope *s, table *victim = 0);
  virtual void out(std::ostream &out) {
    update_stmt::out(out);
    // Syntax: ERROR:  Expected end of statement, found RETURNING
    //out << std::endl << "returning " << *select_list;
  }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    search->accept(v);
    set_list->accept(v);
    select_list->accept(v);
  }
};

shared_ptr<prod> statement_factory(struct scope *s, long max_joins=1, struct prod *parent = 0);

struct explain_stmt : prod {
  shared_ptr<prod> q;
  virtual void out(std::ostream &out);
  explain_stmt(struct prod* p, shared_ptr<prod> query) : prod(p), q(query) {
  }
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    q->accept(v);
  }
};

shared_ptr<prod> explain_factory(struct scope *s, long max_joins=1);

struct common_table_expression : prod {
  vector<shared_ptr<prod> > with_queries;
  shared_ptr<prod> query;
  vector<shared_ptr<named_relation> > refs;
  struct scope myscope;
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
  common_table_expression(prod *parent, struct scope *s);
};

struct values_expr : table_ref {
  int num_rows;
  int num_cols;
  std::vector<std::vector<shared_ptr<value_expr>>> rows;
  relation derived_table;
  values_expr(prod *p);
  virtual ~values_expr() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    for (auto &row : rows)
      for (auto &e : row)
        e->accept(v);
  }
};

struct table_func_ref : table_ref {
  std::string func_call;
  std::string col_alias;
  relation derived_table;
  table_func_ref(prod *p);
  virtual ~table_func_ref() { }
  virtual void out(std::ostream &out);
};

struct set_operation : prod {
  std::string op_type;
  shared_ptr<query_spec> lhs;
  shared_ptr<query_spec> rhs;
  struct scope myscope;
  set_operation(prod *p, struct scope *s);
  virtual ~set_operation() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    lhs->accept(v);
    rhs->accept(v);
  }
};

struct subscribe_stmt : prod {
  shared_ptr<query_spec> query;
  struct scope myscope;
  subscribe_stmt(prod *p, struct scope *s);
  virtual ~subscribe_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    query->accept(v);
  }
};

struct recursive_cte : prod {
  shared_ptr<query_spec> base_query;
  shared_ptr<query_spec> final_query;
  std::string cte_name;
  relation cte_relation;
  shared_ptr<aliased_relation> cte_ref;
  struct scope myscope;
  struct scope inner_scope;
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v);
  recursive_cte(prod *parent, struct scope *s);
};

struct create_mat_view_stmt : prod {
  shared_ptr<query_spec> query;
  std::string view_name;
  struct scope myscope;
  static long seq;
  create_mat_view_stmt(prod *p, struct scope *s);
  virtual ~create_mat_view_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    query->accept(v);
  }
};

struct create_view_stmt : prod {
  shared_ptr<query_spec> query;
  std::string view_name;
  struct scope myscope;
  static long seq;
  create_view_stmt(prod *p, struct scope *s);
  virtual ~create_view_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
    query->accept(v);
  }
};

struct create_index_stmt : prod {
  std::string index_name;
  named_relation *target;
  std::vector<std::string> columns;
  struct scope myscope;
  static long seq;
  create_index_stmt(prod *p, struct scope *s);
  virtual ~create_index_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct create_table_stmt : prod {
  std::string table_name;
  std::vector<std::pair<std::string, std::string>> col_defs;
  struct scope myscope;
  static long seq;
  create_table_stmt(prod *p, struct scope *s);
  virtual ~create_table_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct create_type_stmt : prod {
  std::string type_name;
  std::string type_def;
  struct scope myscope;
  static long seq;
  create_type_stmt(prod *p, struct scope *s);
  virtual ~create_type_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct drop_stmt : prod {
  std::string object_type;
  std::string object_name;
  drop_stmt(prod *p, struct scope *s);
  virtual ~drop_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct alter_rename_stmt : prod {
  std::string object_type;
  std::string old_name;
  std::string new_name;
  alter_rename_stmt(prod *p, struct scope *s);
  virtual ~alter_rename_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct grant_revoke_stmt : prod {
  bool is_grant;
  std::string privilege;
  std::string object_type;
  std::string object_name;
  grant_revoke_stmt(prod *p, struct scope *s);
  virtual ~grant_revoke_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct set_stmt : prod {
  std::string param_name;
  std::string param_value;
  set_stmt(prod *p, struct scope *s);
  virtual ~set_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

struct show_stmt : prod {
  std::string target;
  show_stmt(prod *p, struct scope *s);
  virtual ~show_stmt() { }
  virtual void out(std::ostream &out);
  virtual void accept(prod_visitor *v) {
    v->visit(this);
  }
};

#endif
