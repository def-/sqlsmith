#include <typeinfo>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <cassert>

#include "random.hh"
#include "relmodel.hh"
#include "grammar.hh"
#include "schema.hh"
#include "impedance.hh"

using namespace std;

shared_ptr<table_ref> table_ref::factory(prod *p) {
  try {
    if (p->level < 6 + d6()) {
      if (d6() > 3 && p->level < 3 + d6() && g_joins > 0) {
	g_joins--;
	return make_shared<table_subquery>(p);
      }
      if (d6() > 3 && g_joins > 0) {
	g_joins--;
	return make_shared<joined_table>(p);
      }
    }
    if (d20() == 1 && p->level < 4)
      return make_shared<values_expr>(p);
    if (d20() == 1 && p->level < 4)
      return make_shared<table_func_ref>(p);
    return make_shared<table_or_query_name>(p);
  } catch (runtime_error &e) {
    p->retry();
  }
  return factory(p);
}

table_or_query_name::table_or_query_name(prod *p) : table_ref(p) {
  t = random_pick(scope->tables);
  refs.push_back(make_shared<aliased_relation>(scope->stmt_uid("ref"), t));
}

void table_or_query_name::out(std::ostream &out) {
  out << t->ident() << " as " << refs[0]->ident();
}

target_table::target_table(prod *p, table *victim) : table_ref(p)
{
  while (! victim
	 || victim->schema == "pg_catalog"
	 || victim->schema == "mz_catalog"
	 || victim->schema == "mz_internal"
	 || !victim->is_base_table
	 || !victim->columns().size()) {
    struct named_relation *pick = random_pick(scope->tables);
    victim = dynamic_cast<table *>(pick);
    retry();
  }
  victim_ = victim;
  refs.push_back(make_shared<aliased_relation>(scope->stmt_uid("target"), victim));
}

void target_table::out(std::ostream &out) {
  out << victim_->ident() << " as " << refs[0]->ident();
}

table_sample::table_sample(prod *p) : table_ref(p) {
  match();
  retry_limit = 1000; /* retries are cheap here */
  do {
    auto pick = random_pick(scope->schema->base_tables);
    t = dynamic_cast<struct table*>(pick);
    retry();
  } while (!t || !t->is_base_table);
  
  refs.push_back(make_shared<aliased_relation>(scope->stmt_uid("sample"), t));
  percent = 0.1 * d100();
  method = (d6() > 2) ? "system" : "bernoulli";
}

void table_sample::out(std::ostream &out) {
  out << t->ident() <<
    " as " << refs[0]->ident() <<
    " tablesample " << method <<
    " (" << percent << ") ";
}

table_subquery::table_subquery(prod *p, bool lateral)
  : table_ref(p), is_lateral(lateral) {
  query = make_shared<query_spec>(this, scope, lateral);
  string alias = scope->stmt_uid("subq");
  relation *aliased_rel = &query->select_list->derived_table;
  refs.push_back(make_shared<aliased_relation>(alias, aliased_rel));
}

table_subquery::~table_subquery() { }

void table_subquery::accept(prod_visitor *v) {
  query->accept(v);
  v->visit(this);
}

shared_ptr<join_cond> join_cond::factory(prod *p, table_ref &lhs, table_ref &rhs)
{
     try {
	  if (d6() < 6)
	       return make_shared<expr_join_cond>(p, lhs, rhs);
	  else
	       return make_shared<simple_join_cond>(p, lhs, rhs);
     } catch (runtime_error &e) {
	  p->retry();
     }
     return factory(p, lhs, rhs);
}

simple_join_cond::simple_join_cond(prod *p, table_ref &lhs, table_ref &rhs)
     : join_cond(p, lhs, rhs)
{
retry:
  named_relation *left_rel = &*random_pick(lhs.refs);
  
  if (!left_rel->columns().size())
    { retry(); goto retry; }

  named_relation *right_rel = &*random_pick(rhs.refs);

  column &c1 = random_pick(left_rel->columns());

  for (auto c2 : right_rel->columns()) {
    if (c1.type == c2.type) {
      condition +=
	scope->schema->quote_name(left_rel->ident()) + "." + scope->schema->quote_name(c1.name) + " = " + scope->schema->quote_name(right_rel->ident()) + "." + scope->schema->quote_name(c2.name) + " ";
      break;
    }
  }
  if (condition == "") {
    retry(); goto retry;
  }
}

void simple_join_cond::out(std::ostream &out) {
     out << condition;
}

expr_join_cond::expr_join_cond(prod *p, table_ref &lhs, table_ref &rhs)
     : join_cond(p, lhs, rhs), joinscope(p->scope)
{
     scope = &joinscope;
     for (auto ref: lhs.refs)
	  joinscope.refs.push_back(&*ref);
     for (auto ref: rhs.refs)
	  joinscope.refs.push_back(&*ref);
     search = bool_expr::factory(this);
}

void expr_join_cond::out(std::ostream &out) {
     out << *search;
}

joined_table::joined_table(prod *p) : table_ref(p) {
  lhs = table_ref::factory(this);
  rhs = table_ref::factory(this);

  condition = join_cond::factory(this, *lhs, *rhs);

  switch(d6()) {
  case 1:
    type = "inner";
    break;
  case 2:
    type = "left";
    break;
  case 3:
    type = "right";
    break;
  case 4:
    type = "full";
    break;
  default:
    type = "";
    break;
  }

  for (auto ref: lhs->refs)
    refs.push_back(ref);
  for (auto ref: rhs->refs)
    refs.push_back(ref);
}

void joined_table::out(std::ostream &out) {
  out << *lhs;
  indent(out);
  out << type << " join " << *rhs;
  indent(out);
  out << "on (" << *condition << ")";
}

void table_subquery::out(std::ostream &out) {
  if (is_lateral)
    out << "lateral ";
  out << "(" << *query << ") as " << refs[0]->ident();
}

void from_clause::out(std::ostream &out) {
  if (! reflist.size())
    return;
  out << "from ";

  for (auto r = reflist.begin(); r < reflist.end(); r++) {
    indent(out);
    out << **r;
    if (r + 1 != reflist.end())
      out << ",";
  }
}

from_clause::from_clause(prod *p) : prod(p) {
  reflist.push_back(table_ref::factory(this));
  for (auto r : reflist.back()->refs)
    scope->refs.push_back(&*r);

  while (d6() > 5 && g_joins > 0) {
    // add a lateral subquery
    g_joins--;
    if (!impedance::matched(typeid(lateral_subquery)))
      break;
    reflist.push_back(make_shared<lateral_subquery>(this));
    for (auto r : reflist.back()->refs)
      scope->refs.push_back(&*r);
  }
}

select_list::select_list(prod *p) : prod(p)
{
  do {
    shared_ptr<value_expr> e = value_expr::factory(this, nullptr, true);
    value_exprs.push_back(e);
    ostringstream name;
    name << "c" << columns++;
    sqltype *t=e->type;
    if (!t)
      fail("null type in select_list");
    derived_table.columns().push_back(column(name.str(), t));
  } while (d6() > 1);
}

void select_list::out(std::ostream &out)
{
  int i = 0;
  for (auto expr = value_exprs.begin(); expr != value_exprs.end(); expr++) {
    indent(out);
    out << **expr << " as " << derived_table.columns()[i].name;
    i++;
    if (expr+1 != value_exprs.end())
      out << ", ";
  }
}

void query_spec::out(std::ostream &out) {
  out << "select ";
  if (has_distinct_on) {
    out << "distinct on (";
    for (auto it = distinct_on_cols.begin(); it != distinct_on_cols.end(); it++) {
      out << **it;
      if (it + 1 != distinct_on_cols.end())
        out << ", ";
    }
    out << ") ";
  } else if (!set_quantifier.empty()) {
    out << set_quantifier << " ";
  }
  out << *select_list;
  indent(out);
  out << *from_clause;
  indent(out);
  out << "where ";
  out << *search;

  if (has_group_by) {
    indent(out);
    out << "group by ";
    for (auto it = group_by_cols.begin(); it != group_by_cols.end(); it++) {
      out << **it;
      if (it + 1 != group_by_cols.end())
        out << ", ";
    }
    if (having) {
      indent(out);
      out << "having " << *having;
    }
  }

  if (has_order_by) {
    indent(out);
    out << "order by ";
    for (auto it = order_by_cols.begin(); it != order_by_cols.end(); it++) {
      out << **it;
      if (it + 1 != order_by_cols.end())
        out << ", ";
    }
  }

  if (has_limit) {
    indent(out);
    out << "limit " << limit_val;
  }

  if (has_offset) {
    indent(out);
    out << "offset " << offset_val;
  }
}

struct for_update_verify : prod_visitor {
  virtual void visit(prod *p) {
    if (dynamic_cast<window_function*>(p))
      throw("window function");
    joined_table* join = dynamic_cast<joined_table*>(p);
    if (join && join->type != "inner")
      throw("outer join");
    query_spec* subquery = dynamic_cast<query_spec*>(p);
    if (subquery)
      subquery->set_quantifier = "";
    table_or_query_name* tab = dynamic_cast<table_or_query_name*>(p);
    if (tab) {
      table *actual_table = dynamic_cast<table*>(tab->t);
      if (actual_table && actual_table->name.find("pg_") == 0)
	throw("catalog");
    }
    // Syntax: ERROR:  Expected joined table, found dot
    //table_sample* sample = dynamic_cast<table_sample*>(p);
    //if (sample) {
    //  table *actual_table = dynamic_cast<table*>(sample->t);
    //  if (actual_table->name.find("pg_"))
    //    throw("catalog");
    //}
  } ;
};


select_for_update::select_for_update(prod *p, struct scope *s, bool lateral)
  : query_spec(p,s,lateral)
{
  static const char *modes[] = {
    "update",
    "share",
    "no key update",
    "key share",
  };

  try {
    for_update_verify v1;
    this->accept(&v1);

  } catch (const char* reason) {
    lockmode = 0;
    return;
  }
  lockmode = modes[d6()%(sizeof(modes)/sizeof(*modes))];
  set_quantifier = ""; // disallow distinct
}

void select_for_update::out(std::ostream &out) {
  query_spec::out(out);
  if (lockmode) {
    indent(out);
    out << " for " << lockmode;
  }
}

query_spec::query_spec(prod *p, struct scope *s, bool lateral) :
  prod(p), myscope(s)
{
  scope = &myscope;
  scope->tables = s->tables;

  if (lateral)
    scope->refs = s->refs;

  from_clause = make_shared<struct from_clause>(this);
  select_list = make_shared<struct select_list>(this);

  set_quantifier = (d100() == 1) ? "distinct" : "";

  search = bool_expr::factory(this);

  // GROUP BY (~20% of queries)
  if (d6() > 4 && scope->refs.size()) {
    has_group_by = true;
    group_by_cols.push_back(make_shared<column_reference>(this));
    while (d6() > 4)
      group_by_cols.push_back(make_shared<column_reference>(this));

    // HAVING (~50% of GROUP BY queries)
    if (d6() > 3) {
      having = bool_expr::factory(this);
    }
  }

  // ORDER BY (~30% of queries)
  if (d6() > 4 && scope->refs.size()) {
    has_order_by = true;
    order_by_cols.push_back(make_shared<column_reference>(this));
    while (d6() > 4)
      order_by_cols.push_back(make_shared<column_reference>(this));
  }

  // LIMIT (~20% of queries)
  if (d6() > 4) {
    has_limit = true;
    limit_val = 1 + d100();
  }

  // OFFSET (~10% of queries with LIMIT)
  if (has_limit && d6() > 4) {
    has_offset = true;
    offset_val = d100() / 2;
  }

  // DISTINCT ON (~5% of queries, requires ORDER BY)
  if (d20() == 1 && scope->refs.size() && set_quantifier.empty()) {
    has_distinct_on = true;
    set_quantifier = "";  // clear regular DISTINCT
    distinct_on_cols.push_back(make_shared<column_reference>(this));
    while (d6() > 5)
      distinct_on_cols.push_back(make_shared<column_reference>(this));

    // ORDER BY must start with DISTINCT ON columns
    has_order_by = true;
    order_by_cols.clear();
    for (auto &c : distinct_on_cols)
      order_by_cols.push_back(c);
    // Optionally add more ORDER BY columns
    while (d6() > 4)
      order_by_cols.push_back(make_shared<column_reference>(this));
  }
}

long prepare_stmt::seq;

void modifying_stmt::pick_victim()
{
  do {
      struct named_relation *pick = random_pick(scope->tables);
      victim = dynamic_cast<struct table*>(pick);
      retry();
    } while (! victim
	   || victim->schema == "pg_catalog"
	   || victim->schema == "mz_catalog"
	   || victim->schema == "mz_internal"
	   || !victim->is_base_table
	   || !victim->columns().size());
}

modifying_stmt::modifying_stmt(prod *p, struct scope *s, table *victim)
  : prod(p), myscope(s)
{
  scope = &myscope;
  scope->tables = s->tables;

  if (!victim)
    pick_victim();
}


delete_stmt::delete_stmt(prod *p, struct scope *s, table *v)
  : modifying_stmt(p,s,v) {
  scope->refs.push_back(victim);
  search = bool_expr::factory(this);
}

delete_returning::delete_returning(prod *p, struct scope *s, table *victim)
  : delete_stmt(p, s, victim) {
  match();
  select_list = make_shared<struct select_list>(this);
}

comment_stmt::comment_stmt(prod *p, struct scope *s, table *v)
  : modifying_stmt(p, s, v)
{
  match();
  if(d6() < 4) {
    victim_column = make_shared<column>(random_pick(victim->columns()));
  }
}

void comment_stmt::out(std::ostream &out)
{
  out << "comment on ";

  if(victim_column) {
    out << "column " << victim->ident() << "." << victim_column->name;
  } else {
    static const char *obj_types[] = {
      "table", "view", "materialized view", "index", "source", "sink",
      "cluster", "secret"
    };
    // Most of the time use "table" since we know victim is a table
    // Sometimes try other object types which may or may not exist
    if (d6() > 4) {
      out << obj_types[d100() % 8] << " " << victim->ident();
    } else {
      out << "table " << victim->ident();
    }
  }

  string comment = scope->stmt_uid("comment");
  out << " is '" << comment << "'";
}

insert_stmt::insert_stmt(prod *p, struct scope *s, table *v)
  : modifying_stmt(p, s, v)
{
  match();

  for (auto col : victim->columns()) {
    auto expr = value_expr::factory(this, col.type, false);
    if (expr->type != col.type && !col.type->consistent(expr->type))
      fail("type mismatch in insert");
    value_exprs.push_back(expr);
  }
}

void insert_stmt::out(std::ostream &out)
{
  out << "insert into " << victim->ident() << " ";

  if (!value_exprs.size()) {
    out << "default values";
    return;
  }

  out << "values (";
  
  for (auto expr = value_exprs.begin();
       expr != value_exprs.end();
       expr++) {
    indent(out);
    out << **expr;
    if (expr+1 != value_exprs.end())
      out << ", ";
  }
  out << ")";
}

set_list::set_list(prod *p, table *target) : prod(p)
{
  do {
    for (auto col : target->columns()) {
      if (d6() < 4)
	continue;
      auto expr = value_expr::factory(this, col.type, false);
      value_exprs.push_back(expr);
      names.push_back(col.name);
    }
  } while (!names.size());
}

void set_list::out(std::ostream &out)
{
  if (!names.size())
    throw std::runtime_error("empty set_list");
  out << " set ";
  for (size_t i = 0; i < names.size(); i++) {
    indent(out);
    out << names[i] << " = " << *value_exprs[i];
    if (i+1 != names.size())
      out << ", ";
  }
}

update_stmt::update_stmt(prod *p, struct scope *s, table *v)
  : modifying_stmt(p, s, v) {
  scope->refs.push_back(victim);
  search = bool_expr::factory(this);
  set_list = make_shared<struct set_list>(this, victim);
}

void update_stmt::out(std::ostream &out)
{
  out << "update " << victim->ident() << *set_list;
}

update_returning::update_returning(prod *p, struct scope *s, table *v)
  : update_stmt(p, s, v) {
  match();

  select_list = make_shared<struct select_list>(this);
}


upsert_stmt::upsert_stmt(prod *p, struct scope *s, table *v)
  : insert_stmt(p,s,v)
{
  match();

  if (!victim->constraints.size())
    fail("need table w/ constraint for upsert");
    
  set_list = std::make_shared<struct set_list>(this, victim);
  search = bool_expr::factory(this);
  constraint = random_pick(victim->constraints);
}

shared_ptr<prod> statement_factory(struct scope *s, long max_joins, struct prod *parent)
{
  try {
    g_joins = max_joins;
    s->new_stmt();
    // Syntax: ERROR:  Unexpected keyword MERGE at the beginning of a statement
    //if (d42() == 1)
    //  return make_shared<merge_stmt>(parent, s);
    if (d42() < 3)
      return make_shared<comment_stmt>(parent, s);
    else if (d42() < 3)
      return make_shared<insert_stmt>(parent, s);
    else if (d42() < 3)
      return make_shared<delete_returning>(parent, s);
    // no constraints supported currently
    //else if (d42() < 3)
    //  return make_shared<upsert_stmt>(parent, s);
    else if (d42() < 3)
      return make_shared<update_returning>(parent, s);
    else if (d42() == 1)
      return make_shared<set_operation>(parent, s);
    else if (d42() == 1)
      return make_shared<subscribe_stmt>(parent, s);
    else if (d42() == 1)
      return make_shared<recursive_cte>(parent, s);
    else if (d42() == 1)
      return make_shared<create_mat_view_stmt>(parent, s);
    else if (d42() == 1)
      return make_shared<create_view_stmt>(parent, s);
    else if (d42() == 1)
      return make_shared<create_index_stmt>(parent, s);
    else if (d42() == 1)
      return make_shared<create_table_stmt>(parent, s);
    else if (d100() == 1)
      return make_shared<create_type_stmt>(parent, s);
    else if (d100() == 1)
      return make_shared<drop_stmt>(parent, s);
    else if (d100() == 1)
      return make_shared<alter_rename_stmt>(parent, s);
    else if (d42() == 1)
      return make_shared<show_stmt>(parent, s);
    else if (d6() > 4)
      return make_shared<select_for_update>(parent, s);
    else if (d6() > 5)
      return make_shared<common_table_expression>(parent, s);
    return make_shared<query_spec>(parent, s);
  } catch (runtime_error &e) {
    return statement_factory(s);
  }
}

shared_ptr<prod> explain_factory(struct scope *s, long max_joins)
{
  try {
    std::shared_ptr<prod> p;
    g_joins = max_joins;
    s->new_stmt();
    if (d6() > 5)
      p = make_shared<common_table_expression>((struct prod *)0, s);
    else
      p = make_shared<query_spec>((struct prod *)0, s);
    return make_shared<explain_stmt>((struct prod *)0, p);
  } catch (runtime_error &e) {
    return explain_factory(s);
  }
}

void common_table_expression::accept(prod_visitor *v)
{
  v->visit(this);
  for(auto q : with_queries)
    q->accept(v);
  query->accept(v);
}

common_table_expression::common_table_expression(prod *parent, struct scope *s)
  : prod(parent), myscope(s)
{
  scope = &myscope;
  do {
    shared_ptr<query_spec> query = make_shared<query_spec>(this, s);
    with_queries.push_back(query);
    string alias = scope->stmt_uid("jennifer");
    relation *relation = &query->select_list->derived_table;
    auto aliased_rel = make_shared<aliased_relation>(alias, relation);
    refs.push_back(aliased_rel);
    scope->tables.push_back(&*aliased_rel);

  } while (d6() > 2);

 retry:
  do {
    auto pick = random_pick(s->tables);
    scope->tables.push_back(pick);
  } while (d6() > 3);
  try {
    query = make_shared<query_spec>(this, scope);
  } catch (runtime_error &e) {
    retry();
    goto retry;
  }

}

void common_table_expression::out(std::ostream &out)
{
  out << "WITH " ;
  for (size_t i = 0; i < with_queries.size(); i++) {
    indent(out);
    out << refs[i]->ident() << " AS " << "(" << *with_queries[i] << ")";
    if (i+1 != with_queries.size())
      out << ", ";
    indent(out);
  }
  out << *query;
  indent(out);
}

merge_stmt::merge_stmt(prod *p, struct scope *s, table *v)
     : modifying_stmt(p,s,v) {
  match();
  target_table_ = make_shared<target_table>(this, victim);
  data_source = table_ref::factory(this);
//   join_condition = join_cond::factory(this, *target_table_, *data_source);
  join_condition = make_shared<simple_join_cond>(this, *target_table_, *data_source);


  /* Put data_source into scope but not target_table.  Visibility of
     the latter varies depending on kind of when clause. */
//   for (auto r : data_source->refs)
//     scope->refs.push_back(&*r);

  clauselist.push_back(when_clause::factory(this));
  while (d6()>4)
    clauselist.push_back(when_clause::factory(this));
}

void merge_stmt::out(std::ostream &out)
{
     out << "MERGE INTO " << *target_table_;
     indent(out);
     out << "USING " << *data_source;
     indent(out);
     out << "ON " << *join_condition;
     indent(out);
     for (auto p : clauselist) {
       out << *p;
       indent(out);
     }
}

void merge_stmt::accept(prod_visitor *v)
{
  v->visit(this);
  target_table_->accept(v);
  data_source->accept(v);
  join_condition->accept(v);
  for (auto p : clauselist)
    p->accept(v);
    
}

when_clause::when_clause(merge_stmt *p)
  : prod(p)
{
  condition = bool_expr::factory(this);
  matched = d6() > 3;
}

void when_clause::out(std::ostream &out)
{
  out << (matched ? "WHEN MATCHED " : "WHEN NOT MATCHED");
  indent(out);
  out << "AND " << *condition;
  indent(out);
  out << " THEN ";
  out << (matched ? "DELETE" : "DO NOTHING");
}

void when_clause::accept(prod_visitor *v)
{
  v->visit(this);
  condition->accept(v);
}

when_clause_update::when_clause_update(merge_stmt *p)
  : when_clause(p), myscope(p->scope)
{
  myscope.tables = scope->tables;
  myscope.refs = scope->refs;
  scope = &myscope;
  scope->refs.push_back(&*(p->target_table_->refs[0]));
  
  set_list = std::make_shared<struct set_list>(this, p->victim);
}

void when_clause_update::out(std::ostream &out) {
  out << "WHEN MATCHED AND " << *condition;
  indent(out);
  out << " THEN UPDATE " << *set_list;
}

void when_clause_update::accept(prod_visitor *v)
{
  v->visit(this);
  set_list->accept(v);
}


when_clause_insert::when_clause_insert(struct merge_stmt *p)
  : when_clause(p)
{
  for (auto col : p->victim->columns()) {
    auto expr = value_expr::factory(this, col.type, false);
    if (expr->type != col.type && !col.type->consistent(expr->type))
      fail("type mismatch in merge insert");
    exprs.push_back(expr);
  }
}

void when_clause_insert::out(std::ostream &out) {
  out << "WHEN NOT MATCHED AND " << *condition;
  indent(out);
  out << " THEN INSERT VALUES ( ";

  for (auto expr = exprs.begin();
       expr != exprs.end();
       expr++) {
    out << **expr;
    if (expr+1 != exprs.end())
      out << ", ";
  }
  out << ")";

}

void when_clause_insert::accept(prod_visitor *v)
{
  v->visit(this);
  for (auto p : exprs)
    p->accept(v);
}

shared_ptr<when_clause> when_clause::factory(struct merge_stmt *p)
{
  try {
    switch(d6()) {
    case 1:
    case 2:
      return make_shared<when_clause_insert>(p);
    case 3:
    case 4:
      return make_shared<when_clause_update>(p);
    default:
      return make_shared<when_clause>(p);
    }
  } catch (runtime_error &e) {
    p->retry();
  }
  return factory(p);
}


void explain_stmt::out(std::ostream &out) {
  out << "explain ";

  // EXPLAIN FILTER PUSHDOWN (~8%)
  if (d12() == 1) {
    out << "filter pushdown for ";
    out << *q;
    return;
  }

  // EXPLAIN BROKEN (~5% - finds optimizer bugs by checking for broken plans)
  if (d20() == 1) {
    out << "broken ";
    switch (d6()) {
    case 1:
    case 2:
      break;
    case 3:
      out << "raw plan ";
      break;
    case 4:
      out << "decorrelated plan ";
      break;
    case 5:
      out << "optimized plan ";
      break;
    case 6:
      out << "physical plan ";
      break;
    }
    out << "as text for " << *q;
    return;
  }

  bool for_supported = true;
  bool with_supported = true;
  switch(d6()) {
  case 1:
    for_supported = false;
    break;
  case 2:
    out << "raw plan ";
    break;
  case 3:
    out << "decorrelated plan ";
    break;
  case 4:
    out << "optimized plan ";
    break;
  case 5:
    out << "physical plan ";
    break;
  case 6:
    out << "timestamp ";
    with_supported = false;
    break;
  }
  if(with_supported && d6() > 2) {
    out << "with (";
    switch(d6()) {
    case 1:
      out << "arity, join implementations, humanized expressions, filter pushdown";
      break;
    case 2:
      out << "arity, join implementations, keys, types, humanized expressions, no fast path, redacted, raw syntax, filter pushdown";
      break;
    case 3:
      out << "keys, types, humanized expressions";
      break;
    case 4:
      out << "keys, redacted, non negative";
      break;
    case 5:
      out << "join implementations, types";
      break;
    case 6:
      out << "types, no fast path";
      break;
    }
    out << ") ";
  }
  out << "as ";
  switch(d6()) {
  case 1:
  case 2:
  case 3:
    out << "json ";
    break;
  default:
    out << "text ";
  }
  if (for_supported) {
    out << "for ";
  }
  if(with_supported) {
    switch(d6()) {
    case 1:
      out << "create materialized view mv as ";
      break;
    case 2:
      out << "create view vw as ";
      break;
    default:
      break;
    }
  }
  out << *q;
}

values_expr::values_expr(prod *p) : table_ref(p) {
  match();
  num_cols = 1 + d6() / 2;
  num_rows = 1 + d6();
  string alias = scope->stmt_uid("vals");

  // Generate first row to establish types
  vector<sqltype*> col_types;
  vector<shared_ptr<value_expr>> first_row;
  for (int c = 0; c < num_cols; c++) {
    auto e = value_expr::factory(this, nullptr, false);
    first_row.push_back(e);
    col_types.push_back(e->type);
  }
  rows.push_back(first_row);

  // Generate remaining rows with matching types
  for (int r = 1; r < num_rows; r++) {
    vector<shared_ptr<value_expr>> row;
    for (int c = 0; c < num_cols; c++) {
      row.push_back(value_expr::factory(this, col_types[c], false));
    }
    rows.push_back(row);
  }

  // Build derived table
  for (int c = 0; c < num_cols; c++) {
    string colname = "c" + to_string(c);
    derived_table.columns().push_back(column(colname, col_types[c]));
  }
  refs.push_back(make_shared<aliased_relation>(alias, &derived_table));
}

void values_expr::out(std::ostream &out) {
  out << "(VALUES ";
  for (auto rit = rows.begin(); rit != rows.end(); rit++) {
    out << "(";
    for (auto eit = rit->begin(); eit != rit->end(); eit++) {
      out << **eit;
      if (eit + 1 != rit->end())
        out << ", ";
    }
    out << ")";
    if (rit + 1 != rows.end())
      out << ", ";
  }
  out << ") as " << refs[0]->ident() << "(";
  for (int c = 0; c < num_cols; c++) {
    out << "c" << c;
    if (c + 1 < num_cols)
      out << ", ";
  }
  out << ")";
}

table_func_ref::table_func_ref(prod *p) : table_ref(p) {
  match();
  string alias = scope->stmt_uid("tf");

  struct func_def {
    const char *call;
    const char *col;
    const char *col_type;
  };

  static const func_def funcs[] = {
    {"generate_series(1, 10)", "gs", "int4"},
    {"generate_series(1, 100)", "gs", "int4"},
    {"generate_series(1, 5)", "gs", "int4"},
    {"generate_series(0, 2)", "gs", "int4"},
    {"generate_series(1, 10, 2)", "gs", "int4"},
    {"generate_series(TIMESTAMP '2020-01-01', TIMESTAMP '2020-01-10', INTERVAL '1 day')", "gs", "timestamp"},
    {"jsonb_array_elements('[1,2,3]'::jsonb)", "jae", "jsonb"},
    {"jsonb_each('{\"a\":1,\"b\":2}'::jsonb)", "je", "jsonb"},
    {"jsonb_object_keys('{\"a\":1,\"b\":2}'::jsonb)", "jok", "text"},
  };

  int idx = d100() % (sizeof(funcs)/sizeof(funcs[0]));
  func_call = funcs[idx].call;
  col_alias = funcs[idx].col;

  // Build derived relation
  derived_table.columns().push_back(column(funcs[idx].col, sqltype::get(funcs[idx].col_type)));
  refs.push_back(make_shared<aliased_relation>(alias, &derived_table));
}

void table_func_ref::out(std::ostream &out) {
  out << func_call << " as " << refs[0]->ident() << "(" << col_alias << ")";
}

set_operation::set_operation(prod *p, struct scope *s)
  : prod(p), myscope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;

  static const char *ops[] = {
    "UNION", "UNION ALL", "INTERSECT", "INTERSECT ALL",
    "EXCEPT", "EXCEPT ALL"
  };
  op_type = ops[d6() - 1];

  lhs = make_shared<query_spec>(this, scope);
  rhs = make_shared<query_spec>(this, scope);
}

void set_operation::out(std::ostream &out) {
  out << "(" << *lhs << ")";
  indent(out);
  out << op_type;
  indent(out);
  out << "(" << *rhs << ")";
}

subscribe_stmt::subscribe_stmt(prod *p, struct scope *s)
  : prod(p), myscope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;
  query = make_shared<query_spec>(this, scope);
}

void subscribe_stmt::out(std::ostream &out) {
  out << "SUBSCRIBE (" << *query << ") WITH (SNAPSHOT = false) UP TO (mz_now()::text::int8 + 1000)";
}

recursive_cte::recursive_cte(prod *parent, struct scope *s)
  : prod(parent), myscope(s), inner_scope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;

  cte_name = scope->stmt_uid("rcte");

  // Generate base query to establish column types
  base_query = make_shared<query_spec>(this, scope);

  // Build CTE relation from base query's derived table
  for (auto &col : base_query->select_list->derived_table.columns()) {
    cte_relation.columns().push_back(col);
  }

  // Add CTE to inner scope for self-reference
  inner_scope.tables = s->tables;
  cte_ref = make_shared<aliased_relation>(cte_name, &cte_relation);
  inner_scope.tables.push_back(&*cte_ref);

  // Generate final query with CTE available
  scope->tables.push_back(&*cte_ref);
  final_query = make_shared<query_spec>(this, scope);
}

void recursive_cte::out(std::ostream &out) {
  out << "WITH MUTUALLY RECURSIVE ";
  out << "ERROR AT RECURSION LIMIT 100 ";
  indent(out);
  out << cte_name << " (";
  auto &cols = cte_relation.columns();
  for (auto it = cols.begin(); it != cols.end(); it++) {
    out << it->name << " " << it->type->name;
    if (it + 1 != cols.end())
      out << ", ";
  }
  out << ") AS (";
  indent(out);
  out << *base_query;
  out << " UNION ALL ";
  indent(out);
  out << "SELECT ";
  for (auto it = cols.begin(); it != cols.end(); it++) {
    out << cte_name << "." << it->name;
    if (it + 1 != cols.end())
      out << ", ";
  }
  out << " FROM " << cte_name;
  out << " WHERE false";
  out << ")";
  indent(out);
  out << *final_query;
}

void recursive_cte::accept(prod_visitor *v)
{
  v->visit(this);
  base_query->accept(v);
  final_query->accept(v);
}

long create_mat_view_stmt::seq = 0;

create_mat_view_stmt::create_mat_view_stmt(prod *p, struct scope *s)
  : prod(p), myscope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;
  view_name = "sqlsmith_mv_" + to_string(seq++);
  query = make_shared<query_spec>(this, scope);
}

void create_mat_view_stmt::out(std::ostream &out) {
  out << "CREATE MATERIALIZED VIEW IF NOT EXISTS " << view_name << " AS ";
  out << *query;
}

long create_view_stmt::seq = 0;

create_view_stmt::create_view_stmt(prod *p, struct scope *s)
  : prod(p), myscope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;
  view_name = "sqlsmith_v_" + to_string(seq++);
  query = make_shared<query_spec>(this, scope);
}

void create_view_stmt::out(std::ostream &out) {
  out << "CREATE VIEW IF NOT EXISTS " << view_name << " AS ";
  out << *query;
}

long create_index_stmt::seq = 0;

create_index_stmt::create_index_stmt(prod *p, struct scope *s)
  : prod(p), myscope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;
  index_name = "sqlsmith_idx_" + to_string(seq++);

  // Pick a random table to index
  target = random_pick(scope->tables);

  // Pick 1-3 columns to index
  auto &cols = target->columns();
  if (cols.empty())
    fail("need table with columns for index");

  int ncols = 1 + d6() / 3;
  for (int i = 0; i < ncols && i < (int)cols.size(); i++) {
    columns.push_back(scope->schema->quote_name(cols[i].name));
  }
}

void create_index_stmt::out(std::ostream &out) {
  out << "CREATE INDEX IF NOT EXISTS " << index_name;
  out << " ON " << target->ident() << " (";
  for (auto it = columns.begin(); it != columns.end(); it++) {
    out << *it;
    if (it + 1 != columns.end())
      out << ", ";
  }
  out << ")";
}

long create_table_stmt::seq = 0;

create_table_stmt::create_table_stmt(prod *p, struct scope *s)
  : prod(p), myscope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;
  table_name = "sqlsmith_t_" + to_string(seq++);

  static const char *types[] = {
    "integer", "bigint", "smallint", "text", "boolean",
    "real", "double precision", "numeric", "varchar",
    "timestamp", "timestamptz", "date", "time", "interval",
    "jsonb", "bytea", "uuid",
    "uint2", "uint4", "uint8",
    "integer[]", "text[]", "bigint[]",
    "jsonb[]"
  };

  int ncols = 1 + d6();
  for (int i = 0; i < ncols; i++) {
    string colname = "c" + to_string(i);
    string coltype = types[d100() % (sizeof(types)/sizeof(types[0]))];
    col_defs.push_back({colname, coltype});
  }
}

void create_table_stmt::out(std::ostream &out) {
  out << "CREATE TABLE IF NOT EXISTS " << table_name << " (";
  for (auto it = col_defs.begin(); it != col_defs.end(); it++) {
    out << it->first << " " << it->second;
    if (it + 1 != col_defs.end())
      out << ", ";
  }
  out << ")";
}

long create_type_stmt::seq = 0;

create_type_stmt::create_type_stmt(prod *p, struct scope *s)
  : prod(p), myscope(s)
{
  match();
  scope = &myscope;
  scope->tables = s->tables;
  type_name = "sqlsmith_type_" + to_string(seq++);

  static const char *element_types[] = {
    "integer", "text", "boolean", "bigint", "real", "jsonb"
  };
  string elem = element_types[d100() % 6];

  if (d6() > 3) {
    type_def = "AS LIST (ELEMENT TYPE = " + elem + ")";
  } else {
    string val_type = element_types[d100() % 6];
    type_def = "AS MAP (KEY TYPE = text, VALUE TYPE = " + val_type + ")";
  }
}

void create_type_stmt::out(std::ostream &out) {
  out << "CREATE TYPE " << type_name << " " << type_def;
}

drop_stmt::drop_stmt(prod *p, struct scope *s) : prod(p) {
  scope = s;
  match();
  static const char *types[] = {
    "TABLE", "VIEW", "MATERIALIZED VIEW", "INDEX", "TYPE"
  };
  int choice = d100() % 5;
  object_type = types[choice];

  static long seq = 0;
  switch (choice) {
  case 0:
    object_name = "sqlsmith_t_" + to_string(d100());
    break;
  case 1:
    object_name = "sqlsmith_v_" + to_string(d100());
    break;
  case 2:
    object_name = "sqlsmith_mv_" + to_string(d100());
    break;
  case 3:
    object_name = "sqlsmith_idx_" + to_string(d100());
    break;
  case 4:
    object_name = "sqlsmith_type_" + to_string(d100());
    break;
  }
}

void drop_stmt::out(std::ostream &out) {
  out << "DROP " << object_type << " IF EXISTS " << object_name;
  if (object_type == "MATERIALIZED VIEW" || object_type == "VIEW" || object_type == "TABLE") {
    if (d6() > 4)
      out << " CASCADE";
  }
}

alter_rename_stmt::alter_rename_stmt(prod *p, struct scope *s) : prod(p) {
  scope = s;
  match();
  static const char *types[] = {
    "TABLE", "VIEW", "MATERIALIZED VIEW", "INDEX"
  };
  int choice = d100() % 4;
  object_type = types[choice];

  switch (choice) {
  case 0:
    old_name = "sqlsmith_t_" + to_string(d100());
    new_name = "sqlsmith_t_" + to_string(d100());
    break;
  case 1:
    old_name = "sqlsmith_v_" + to_string(d100());
    new_name = "sqlsmith_v_" + to_string(d100());
    break;
  case 2:
    old_name = "sqlsmith_mv_" + to_string(d100());
    new_name = "sqlsmith_mv_" + to_string(d100());
    break;
  case 3:
    old_name = "sqlsmith_idx_" + to_string(d100());
    new_name = "sqlsmith_idx_" + to_string(d100());
    break;
  }
}

void alter_rename_stmt::out(std::ostream &out) {
  out << "ALTER " << object_type << " IF EXISTS " << old_name
      << " RENAME TO " << new_name;
}

grant_revoke_stmt::grant_revoke_stmt(prod *p, struct scope *s) : prod(p) {
  scope = s;
  match();
  is_grant = d6() > 3;

  static const char *privileges[] = {
    "SELECT", "INSERT", "UPDATE", "DELETE", "USAGE", "CREATE"
  };
  privilege = privileges[d6() - 1];

  auto &tables = s->schema->tables;
  if (tables.size() > 0) {
    auto &t = random_pick<>(tables);
    object_type = "TABLE";
    object_name = t.ident();
  } else {
    object_type = "DATABASE";
    object_name = "materialize";
  }
}

void grant_revoke_stmt::out(std::ostream &out) {
  if (is_grant)
    out << "GRANT " << privilege << " ON " << object_type << " " << object_name << " TO PUBLIC";
  else
    out << "REVOKE " << privilege << " ON " << object_type << " " << object_name << " FROM PUBLIC";
}

set_stmt::set_stmt(prod *p, struct scope *s) : prod(p) {
  scope = s;
  match();

  static const char *params[][2] = {
    {"statement_timeout", "'60s'"},
    {"idle_in_transaction_session_timeout", "'120s'"},
    {"transaction_isolation", "'serializable'"},
    {"transaction_isolation", "'strict serializable'"},
    {"extra_float_digits", "3"},
    {"intervalstyle", "'postgres'"},
    {"timezone", "'UTC'"},
    {"timezone", "'America/New_York'"},
    {"search_path", "'public, mz_catalog'"},
    {"auto_route_catalog_queries", "'true'"},
    {"enable_session_rbac_checks", "'false'"},
    {"max_query_result_size", "'1GB'"},
  };
  int idx = d100() % 12;
  param_name = params[idx][0];
  param_value = params[idx][1];
}

void set_stmt::out(std::ostream &out) {
  if (d6() == 1)
    out << "RESET " << param_name;
  else
    out << "SET " << param_name << " = " << param_value;
}

show_stmt::show_stmt(prod *p, struct scope *s) : prod(p) {
  scope = s;
  match();

  static const char *targets[] = {
    "TABLES", "VIEWS", "MATERIALIZED VIEWS",
    "SOURCES", "SINKS", "INDEXES",
    "TYPES", "CLUSTERS", "CLUSTER REPLICAS",
    "DATABASES", "SCHEMAS", "COLUMNS FROM ",
    "CONNECTIONS", "SECRETS", "ROLES"
  };
  int idx = d100() % 15;
  target = targets[idx];

  if (target == "COLUMNS FROM ") {
    auto &tables = s->schema->tables;
    if (tables.size() > 0) {
      target += random_pick<>(tables).ident();
    } else {
      target = "TABLES";
    }
  }
}

void show_stmt::out(std::ostream &out) {
  out << "SHOW " << target;
}
