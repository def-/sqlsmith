#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <cassert>

#include "random.hh"
#include "relmodel.hh"
#include "grammar.hh"
#include "schema.hh"
#include "impedance.hh"
#include "expr.hh"

using namespace std;
using impedance::matched;

shared_ptr<value_expr> value_expr::factory(prod *p, sqltype *type_constraint, bool can_return_set)
{
  try {
    if (1 == d20() && p->level < d6() && window_function::allowed(p))
      return make_shared<window_function>(p, type_constraint);
    if (1 == d20() && p->level < d6() && dedicated_window_func::allowed(p))
      return make_shared<dedicated_window_func>(p, type_constraint);
    if (1 == d42() && p->level < d6() && type_constraint && type_constraint->name.rfind("list", 0) != 0 && type_constraint->name.rfind("map", 0) != 0 && type_constraint->name.rfind("record", 0) != 0 && type_constraint->name.rfind("any", 0) != 0)
      return make_shared<coalesce>(p, type_constraint);
    else if (1 == d42() && p->level < d6() && type_constraint && type_constraint->name.rfind("list", 0) != 0 && type_constraint->name.rfind("map", 0) != 0 && type_constraint->name.rfind("record", 0) != 0 && type_constraint->name.rfind("any", 0) != 0)
      return make_shared<nullif>(p, type_constraint);
    if (p->level < d6() && d6() < 3)
      return make_shared<funcall>(p, type_constraint, can_return_set);
    else if (p->level < d6() && d6() == 6)
      return make_shared<opcall>(p, type_constraint);
    else if (d12()==1)
      return make_shared<atomic_subselect>(p, type_constraint);
    else if (p->level< d6() && d9()==1)
      return make_shared<case_expr>(p, type_constraint);
    else if (d20() == 1 && p->level < d6())
      return make_shared<cast_expr>(p, type_constraint);
    else if (d20() == 1 && p->level < d6())
      return make_shared<greatest_least>(p, type_constraint);
    else if (d42() == 1 && type_constraint && (type_constraint->name == "jsonb" || type_constraint->name == "text"))
      return make_shared<jsonb_access>(p, type_constraint);
    else if (d42() == 1 && (!type_constraint || type_constraint->name == "text"))
      return make_shared<substring_expr>(p, type_constraint);
    else if (d42() == 1 && (!type_constraint || type_constraint == p->scope->schema->inttype))
      return make_shared<extract_expr>(p, type_constraint);
    else if (d42() == 1 && (!type_constraint || type_constraint == p->scope->schema->inttype))
      return make_shared<position_expr>(p, type_constraint);
    else if (p->scope->refs.size() && d20() > 1)
      return make_shared<column_reference>(p, type_constraint);
    else
      return make_shared<const_expr>(p, type_constraint);
  } catch (runtime_error &e) {
  }
  p->retry();
  return factory(p, type_constraint, can_return_set);
}

case_expr::case_expr(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  condition = bool_expr::factory(this);
  true_expr = value_expr::factory(this, type_constraint, false);
  false_expr = value_expr::factory(this, true_expr->type, false);

  if(false_expr->type != true_expr->type) {
       /* Types are consistent but not identical.  Try to find a more
	  concrete one for a better match. */
       if (true_expr->type->consistent(false_expr->type))
	    true_expr = value_expr::factory(this, false_expr->type, false);
       else
	    false_expr = value_expr::factory(this, true_expr->type, false);
  }
  type = true_expr->type;

  if (d6() > 4) {
    int extra = 1 + d6() / 3;
    for (int i = 0; i < extra; i++) {
      auto cond = bool_expr::factory(this);
      auto val = value_expr::factory(this, type, false);
      extra_when_clauses.push_back({cond, val});
    }
  }
}

void case_expr::out(std::ostream &out)
{
  out << "case when " << *condition;
  out << " then " << *true_expr;
  for (auto &wc : extra_when_clauses) {
    out << " when " << *wc.first;
    out << " then " << *wc.second;
  }
  out << " else " << *false_expr;
  out << " end";
  indent(out);
}

void case_expr::accept(prod_visitor *v)
{
  v->visit(this);
  condition->accept(v);
  true_expr->accept(v);
  false_expr->accept(v);
  for (auto &wc : extra_when_clauses) {
    wc.first->accept(v);
    wc.second->accept(v);
  }
}

column_reference::column_reference(prod *p, sqltype *type_constraint) : value_expr(p)
{
  if (type_constraint) {
    auto pairs = scope->refs_of_type(type_constraint);
    auto picked = random_pick(pairs);
    reference += picked.first->ident()
      + "." + scope->schema->quote_name(picked.second.name);
    type = picked.second.type;
    if (!type_constraint->consistent(type))
      fail("type mismatch in column_reference");
  } else {
    named_relation *r = random_pick(scope->refs);

    reference += r->ident() + ".";
    column &c = random_pick(r->columns());
    type = c.type;
    reference += scope->schema->quote_name(c.name);
  }
}

shared_ptr<bool_expr> bool_expr::factory(prod *p)
{
  try {
       if (p->level > d100())
	    return make_shared<truth_value>(p);
       int choice = d100();
       if (choice <= 25)
	    return make_shared<comparison_op>(p);
       else if (choice <= 40)
	    return make_shared<bool_term>(p);
       else if (choice <= 52)
	    return make_shared<null_predicate>(p);
       else if (choice <= 58 && g_joins > 0) {
	    g_joins--;
	    return make_shared<exists_predicate>(p);
       } else if (choice <= 64)
	    return make_shared<between_expr>(p);
       else if (choice <= 70)
	    return make_shared<like_expr>(p);
       else if (choice <= 76)
	    return make_shared<in_expr>(p);
       else if (choice <= 80)
	    return make_shared<distinct_pred>(p);
       else if (choice <= 84)
	    return make_shared<bool_test>(p);
       else if (choice <= 88)
	    return make_shared<not_expr>(p);
       else if (choice <= 92 && g_joins > 0) {
	    g_joins--;
	    return make_shared<any_all_expr>(p);
       } else if (choice <= 95)
	    return make_shared<temporal_filter>(p);
       else
	    return make_shared<truth_value>(p);
  } catch (runtime_error &e) {
  }
  p->retry();
  return factory(p);

}

exists_predicate::exists_predicate(prod *p) : bool_expr(p)
{
  subquery = make_shared<query_spec>(this, scope);
}

void exists_predicate::accept(prod_visitor *v)
{
  v->visit(this);
  subquery->accept(v);
}

void exists_predicate::out(std::ostream &out)
{
  out << "EXISTS (";
  indent(out);
  out << *subquery << ")";
}

distinct_pred::distinct_pred(prod *p) : bool_binop(p)
{
  lhs = make_shared<column_reference>(this);
  rhs = make_shared<column_reference>(this, lhs->type);
}

comparison_op::comparison_op(prod *p) : bool_binop(p)
{
  auto &idx = p->scope->schema->operators_returning_type;

  auto iters = idx.equal_range(scope->schema->booltype);
  oper = random_pick(random_pick(iters)->second);

  lhs = value_expr::factory(this, oper->left, false);
  rhs = value_expr::factory(this, oper->right, false);

  if (oper->left == oper->right
	 && lhs->type != rhs->type) {

    if (lhs->type->consistent(rhs->type))
      lhs = value_expr::factory(this, rhs->type, false);
    else
      rhs = value_expr::factory(this, lhs->type, false);
  }
}

coalesce::coalesce(prod *p, sqltype *type_constraint, const char *abbrev)
     : value_expr(p), abbrev_(abbrev)
{
  auto first_expr = value_expr::factory(this, type_constraint, false);
  auto second_expr = value_expr::factory(this, first_expr->type, false);

  retry_limit = 20;
  while(first_expr->type != second_expr->type) {
    retry();
    if (first_expr->type->consistent(second_expr->type))
      first_expr = value_expr::factory(this, second_expr->type, false);
    else 
      second_expr = value_expr::factory(this, first_expr->type, false);
  }
  type = second_expr->type;

  value_exprs.push_back(first_expr);
  value_exprs.push_back(second_expr);

  // Sometimes add more arguments to coalesce
  while (d6() > 4) {
    value_exprs.push_back(value_expr::factory(this, type, false));
  }
}
 
void coalesce::out(std::ostream &out)
{
  out << "cast(" << abbrev_ << "(";
  for (auto expr = value_exprs.begin(); expr != value_exprs.end(); expr++) {
    out << **expr;
    if (expr+1 != value_exprs.end())
      out << ",", indent(out);
  }
  out << ")";
  out << " as " << type->name << ")";
}

const_expr::const_expr(prod *p, sqltype *type_constraint)
    : value_expr(p), expr("")
{
  type = type_constraint ? type_constraint : scope->schema->inttype;

  if (type == scope->schema->inttype)
    expr = to_string(d100());
  else if (type == scope->schema->booltype)
    expr += (d6() > 3) ? scope->schema->true_literal : scope->schema->false_literal;
  // Error: ERROR:  column "default" does not exist
  //else if (dynamic_cast<insert_stmt*>(p) && (d6() > 3))
  //  expr += "default";
  // https://github.com/MaterializeInc/materialize/issues/17870
  else if (type->name == "any")
    expr = "null";
  else if (type->name == "anycompatible")
    expr = "null";
  else if (type->name == "anyarray")
    expr = "array[null, null]";
  else if (type->name == "anycompatiblearray")
    expr = "array[null, null]";
  else if (type->name == "list")
    expr = "list[null, null]";
  else if (type->name == "anycompatiblelist")
    expr = "list[null, null]";
  else if (type->name == "anyrange")
    expr = "numrange(0,0)";
  else if (type->name == "anycompatiblerange")
    expr = "numrange(0,0)";
  else if (type->name == "record")
    expr = "row(1)";
  else if (type->name == "map")
    expr = "'{}'::map[text=>text]";
  else if (type->name == "anycompatiblemap")
    expr = "'{}'::map[text=>text]";
  else if (type->name == "text[]" || type->name == "_text")
    expr = "array['a', 'b', null, '']::text[]";
  else if (type->name == "smallint[]" || type->name == "_smallint")
    expr = "array[1, 2, 3, null]::smallint[]";
  else if (type->name == "integer[]" || type->name == "_integer")
    expr = "array[1, 2, 3, null]::integer[]";
  else if (type->name == "jsonb")
    if (d6() == 1)
      expr = "'{}'::jsonb";
    else if (d6() == 1)
      expr = "'[]'::jsonb";
    else if (d6() == 1)
      expr = "'\"foo\"'::jsonb";
    else if (d6() == 1)
      expr = "'null'::jsonb";
    else
      expr = "'{\"1\":2,\"3\":4}'::jsonb";
  else if (type->name == "bytea")
    if (d6() == 1)
      expr = "cast('\\000' as bytea)";
    else if (d6() == 1)
      expr = "cast('\\xFFFFFF' as bytea)";
    else
      expr = "cast('\\xDEADBEEF' as bytea)";
  else if (type->name == "uuid")
    if (d6() == 1)
      expr = "UUID '00000000-0000-0000-0000-000000000000'";
    else if (d6() == 1)
      expr = "UUID 'ffffffff-ffff-ffff-ffff-ffffffffffff'";
    else
      expr = "UUID 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'";
  else if (type->name == "date")
    if (d6() == 1)
      expr = "(DATE '1-11-25' - INTERVAL '4713 YEARS')";
    else if (d6() == 1)
      expr = "(DATE '62143-12-31' + INTERVAL '200000 YEARS')";
    else
      expr = "DATE '2007-02-01'";
  else if (type->name == "time")
    if (d6() == 1)
      expr = "TIME '00:00:00'";
    else if (d6() == 1)
      expr = "TIME '23:59:60'";
    else
      expr = "TIME '01:23:45'";
  else if (type->name == "timestamp")
    if (d6() == 1)
      expr = "(TIMESTAMP '0001-01-01 00:00:00' - INTERVAL '4713 YEARS')";
    else if (d6() == 1)
      expr = "(TIMESTAMP '95143-12-31 23:59:59' + INTERVAL '167 MILLENNIUM')";
    else if (d6() == 1)
      expr = "(TIMESTAMP(0) '0001-01-01 00:00:00' - INTERVAL '4713 YEARS')";
    else if (d6() == 1)
      expr = "(TIMESTAMP(6) '0001-01-01 00:00:00' - INTERVAL '4713 YEARS')";
    else if (d6() == 1)
      expr = "TIMESTAMP(3) '2023-01-01 01:23:45'";
    else
      expr = "TIMESTAMP '2023-01-01 01:23:45'";
  else if (type->name == "timestamptz")
    if (d6() == 1)
      expr = "(TIMESTAMPTZ '0001-01-01 00:00:00+06' - INTERVAL '4713 YEARS')";
    else if (d6() == 1)
      expr = "(TIMESTAMPTZ '95143-12-31 23:59:59+06' + INTERVAL '167 MILLENNIUM')";
    else if (d6() == 1)
      expr = "(TIMESTAMPTZ(0) '0001-01-01 00:00:00+06' - INTERVAL '4713 YEARS')";
    else if (d6() == 1)
      expr = "(TIMESTAMPTZ(6) '95143-12-31 23:59:59+06' + INTERVAL '167 MILLENNIUM')";
    else if (d6() == 1)
      expr = "TIMESTAMPTZ(3) '2023-01-01 01:23:45+06'";
    else
      expr = "TIMESTAMPTZ '2023-01-01 01:23:45+06'";
  else if (type->name == "interval")
    if (d6() == 1)
      expr = "INTERVAL '2147483647 MONTHS'";
    else if (d6() == 1)
      expr = "INTERVAL '-2147483648 MONTHS'";
    else
      expr = "INTERVAL '1' MINUTE";
  else if (type->name == "aclitem")
    expr = "cast(null as aclitem)";
  else if (type->name == "mz_aclitem")
    expr = "cast(null as mz_aclitem)";
  else if (type->name == "oid[]")
    expr = "array[null, 12]::oid[]";
  else if (type->name == "mz_aclitem[]" || type->name == "_mz_aclitem")
    expr = "array[null, null]::mz_aclitem[]";
  else if (type->name == "aclitem[]" || type->name == "_aclitem")
    expr = "array[null, null]::aclitem[]";
  else if (type->name.rfind("[]") == type->name.size() - 3)
    expr = "array[null, null]::" + type->name.substr(type->name.rfind("[]")) + "[]";
  else if (type->name[0] == '_')
    expr = "array[null, null]::" + type->name.substr(1, type->name.size()) + "[]";
  else
    if (d6() < 4) {
      if (type->name == "int2")
        expr = "-32768::int2";
      else if (type->name == "int4")
        expr = "-2147483648::int4";
      else if (type->name == "int8")
        expr = "-9223372036854775808::int8";
      else if (type->name == "uint2")
        expr = "0::uint2";
      else if (type->name == "uint4")
        expr = "0::uint4";
      else if (type->name == "uint8")
        expr = "0::uint8";
      else if (type->name == "float4")
        expr = "'inf'::float4";
      else if (type->name == "float8")
        expr = "'inf'::float8";
      else
        expr = "cast(0 as " + type->name + ")";
    } else if (d6() < 4) {
      if (type->name == "int2")
        expr = "32767::int2";
      else if (type->name == "int4")
        expr = "2147483647::int4";
      else if (type->name == "int8")
        expr = "9223372036854775807::int8";
      else if (type->name == "uint2")
        expr = "65535::uint2";
      else if (type->name == "uint4")
        expr = "4294967295::uint4";
      else if (type->name == "uint8")
        expr = "18446744073709551615::uint8";
      else if (type->name == "float4")
        expr = "'nan'::float4";
      else if (type->name == "float8")
        expr = "'nan'::float8";
      else
        expr = "1::" + type->name;
    } else if (d6() == 1) {
      expr = "2::" + type->name;
    } else if (d6() == 1) {
      expr = "10::" + type->name;
    } else {
      expr = "null::" + type->name;
    }
}

funcall::funcall(prod *p, sqltype *type_constraint, bool can_return_set, bool agg)
  : value_expr(p), is_aggregate(agg)
{
  //if (type_constraint == scope->schema->internaltype)
  //  fail("cannot call functions involving internal type");

  // TODO: pick can_return_set here
  auto &idx = agg ? p->scope->schema->aggregates_returning_type
    : !can_return_set ?
      (4 < d6()) ? p->scope->schema->routines_returning_type_without_returns_set
      : p->scope->schema->parameterless_routines_returning_type_without_returns_set
    : (4 < d6()) ?
    p->scope->schema->routines_returning_type
    : p->scope->schema->parameterless_routines_returning_type;

 retry:
  
  if (!type_constraint) {
    proc = random_pick(random_pick(idx.begin(), idx.end())->second);
  } else {
    auto iters = idx.equal_range(type_constraint);
    proc = random_pick(random_pick(iters)->second);
    if (proc && !type_constraint->consistent(proc->restype)) {
      retry();
      goto retry;
    }
  }

  if (!proc) {
    retry();
    goto retry;
  }

  if (type_constraint)
    type = type_constraint;
  else
    type = proc->restype;

  //if (type == scope->schema->internaltype) {
  //  retry();
  //  goto retry;
  //}

  //for (auto type : proc->argtypes)
  //  if (type == scope->schema->internaltype
  //      || type == scope->schema->arraytype) {
  //    retry();
  //    goto retry;
  //  }
  
  for (auto argtype : proc->argtypes) {
    if (!argtype)
      fail("null argtype in funcall");
    auto expr = value_expr::factory(this, argtype, false);
    parms.push_back(expr);
  }

  if (is_aggregate && d6() > 4) {
    filter = bool_expr::factory(this);
  }

  if (is_aggregate && d6() > 4 && parms.size() > 0) {
    agg_order_by.push_back(make_shared<column_reference>(this));
    while (d6() > 5)
      agg_order_by.push_back(make_shared<column_reference>(this));
  }
}

void funcall::accept(prod_visitor *v) {
  v->visit(this);
  for (auto p : parms)
    p->accept(v);
  if (filter) filter->accept(v);
  for (auto &r : agg_order_by) r->accept(v);
}

void funcall::out(std::ostream &out)
{
  out << proc->ident() << "(";
  for (auto expr = parms.begin(); expr != parms.end(); expr++) {
    indent(out);
    // https://github.com/MaterializeInc/materialize/issues/17870
    if ((*expr)->type->name.rfind("list", 0) != 0 && (*expr)->type->name.rfind("map", 0) != 0 && (*expr)->type->name.rfind("record", 0) != 0 && (*expr)->type->name.rfind("any", 0) != 0)
      out << "CAST(" << **expr << " as " << (*expr)->type->name << ")";
    else
      out << **expr;
    if (expr+1 != parms.end())
      out << ",";
  }

  if (is_aggregate && (parms.begin() == parms.end()))
    out << "*";

  if (!agg_order_by.empty()) {
    out << " ORDER BY ";
    for (auto ref = agg_order_by.begin(); ref != agg_order_by.end(); ref++) {
      out << **ref;
      if (ref + 1 != agg_order_by.end()) out << ", ";
    }
  }

  out << ")";

  if (filter) {
    out << " FILTER (WHERE " << *filter << ")";
  }
}

opcall::opcall(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  auto &idx = p->scope->schema->operators_returning_type;

  if (!type_constraint) {
    oper = random_pick(random_pick(idx.begin(), idx.end())->second);
  } else {
    auto iters = idx.equal_range(type_constraint);
    oper = random_pick(random_pick(iters)->second);
  }

  if (!oper->right || !oper->result)
    fail("null operand type in opcall");

  if (type_constraint)
    type = type_constraint;
  else
    type = oper->result;

  rhs = value_expr::factory(this, oper->right, false);
  if (oper->left) {
    lhs = value_expr::factory(this, oper->left, false);
  }
}

atomic_subselect::atomic_subselect(prod *p, sqltype *type_constraint)
  : value_expr(p), offset((d6() == 6) ? d100() : d6())
{
  match();
  if (d6() < 3) {
    if (type_constraint) {
      auto idx = scope->schema->aggregates_returning_type;
      auto iters = idx.equal_range(type_constraint);
      agg = random_pick(random_pick(iters)->second);
    } else {
      agg = &random_pick<>(scope->schema->aggregates);
    }
    if (agg->argtypes.size() != 1)
      agg = 0;
    else
      type_constraint = agg->argtypes[0];
  } else {
    agg = 0;
  }

  if (type_constraint) {
    auto idx = scope->schema->tables_with_columns_of_type;
    col = 0;
    auto iters = idx.equal_range(type_constraint);
    tab = random_pick(random_pick(iters)->second);

    for (auto &cand : tab->columns()) {
      if (type_constraint->consistent(cand.type)) {
	col = &cand;
	break;
      }
    }
    if (!col)
      fail("no matching column for atomic_subselect");
  } else {
    tab = &random_pick<>(scope->schema->tables);
    col = &random_pick<>(tab->columns());
  }

  type = agg ? agg->restype : col->type;
}

void atomic_subselect::out(std::ostream &out)
{
  out << "(select ";

  if (agg)
    out << agg->ident() << "(" << scope->schema->quote_name(col->name) << ")";
  else
    out << scope->schema->quote_name(col->name);
  
  out << " from " << tab->ident();

  if (!agg)
    out << " limit 1 offset " << offset;

  out << ")";
  indent(out);
}

void window_function::out(std::ostream &out)
{
  indent(out);
  out << *aggregate << " over (partition by ";

  for (auto ref = partition_by.begin(); ref != partition_by.end(); ref++) {
    out << **ref;
    if (ref+1 != partition_by.end())
      out << ",";
  }

  out << " order by ";

  for (auto ref = order_by.begin(); ref != order_by.end(); ref++) {
    out << **ref;
    if (ref+1 != order_by.end())
      out << ",";
  }

  if (!frame_clause.empty())
    out << " " << frame_clause;

  out << ")";
}

window_function::window_function(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  //bool agg = d6() > 1;
  bool agg = true;
  aggregate = make_shared<funcall>(this, type_constraint, true, agg);
  type = aggregate->type;
  partition_by.push_back(make_shared<column_reference>(this));
  while(d6() > 4)
    partition_by.push_back(make_shared<column_reference>(this));

  order_by.push_back(make_shared<column_reference>(this));
  while(d6() > 4)
    order_by.push_back(make_shared<column_reference>(this));

  if (d6() > 4) {
    static const char *frames[] = {
      "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW",
      "ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING",
      "ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING",
      "ROWS BETWEEN 3 PRECEDING AND CURRENT ROW",
      "ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING",
      "RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW",
    };
    frame_clause = frames[d6() - 1];
  }
}

bool window_function::allowed(prod *p)
{
  if (dynamic_cast<select_list *>(p))
    return dynamic_cast<query_spec *>(p->pprod) ? true : false;
  if (dynamic_cast<window_function *>(p))
    return false;
  if (dynamic_cast<dedicated_window_func *>(p))
    return false;
  if (dynamic_cast<value_expr *>(p))
    return allowed(p->pprod);
  return false;
}

between_expr::between_expr(prod *p) : bool_expr(p)
{
  match();
  negated = d6() > 4;
  expr = value_expr::factory(this, nullptr, false);
  lo = value_expr::factory(this, expr->type, false);
  hi = value_expr::factory(this, expr->type, false);
}

void between_expr::out(std::ostream &out)
{
  out << "(" << *expr << (negated ? " NOT BETWEEN " : " BETWEEN ")
      << *lo << " AND " << *hi << ")";
}

like_expr::like_expr(prod *p) : bool_expr(p)
{
  match();
  is_ilike = d6() > 3;
  expr = value_expr::factory(this, sqltype::get("text"), false);
  static const char *patterns[] = {
    "'%'", "'_%'", "'%_'", "'__'", "'%a%'", "'a%'", "'%a'", "'_a_'"
  };
  pattern = patterns[d100() % 8];
}

void like_expr::out(std::ostream &out)
{
  out << "(" << *expr << (is_ilike ? " ILIKE " : " LIKE ") << pattern << ")";
}

in_expr::in_expr(prod *p) : bool_expr(p)
{
  match();
  negated = d6() > 4;
  use_subquery = d6() > 4 && g_joins > 0;

  expr = value_expr::factory(this, nullptr, false);

  if (use_subquery) {
    g_joins--;
    subquery = make_shared<query_spec>(this, scope);
  } else {
    int count = 1 + d6();
    for (int i = 0; i < count; i++) {
      value_list.push_back(value_expr::factory(this, expr->type, false));
    }
  }
}

void in_expr::out(std::ostream &out)
{
  out << "(" << *expr << (negated ? " NOT IN (" : " IN (");
  if (use_subquery) {
    out << *subquery;
  } else {
    for (auto it = value_list.begin(); it != value_list.end(); it++) {
      out << **it;
      if (it + 1 != value_list.end())
        out << ", ";
    }
  }
  out << "))";
}

void in_expr::accept(prod_visitor *v)
{
  v->visit(this);
  expr->accept(v);
  if (use_subquery)
    subquery->accept(v);
  else
    for (auto &e : value_list) e->accept(v);
}

cast_expr::cast_expr(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  if (type_constraint) {
    inner = value_expr::factory(this, nullptr, false);
    target_type = type_constraint->name;
    type = type_constraint;
  } else {
    inner = value_expr::factory(this, nullptr, false);
    static const char *cast_targets[] = {
      "integer", "bigint", "text", "boolean", "numeric", "real",
      "float8", "smallint", "varchar"
    };
    target_type = cast_targets[d100() % 9];
    type = sqltype::get(target_type);
  }
}

void cast_expr::out(std::ostream &out)
{
  out << "CAST(" << *inner << " AS " << target_type << ")";
}

temporal_filter::temporal_filter(prod *p) : bool_expr(p)
{
  match();
  col_ref = make_shared<column_reference>(this, sqltype::get("timestamptz"));
  static const char *intervals[] = {
    "'1 second'", "'1 minute'", "'1 hour'", "'1 day'", "'1 week'",
    "'30 seconds'", "'5 minutes'", "'10 minutes'"
  };
  interval_str = intervals[d100() % 8];
}

void temporal_filter::out(std::ostream &out)
{
  out << "mz_now() <= " << *col_ref << " + INTERVAL " << interval_str;
}

dedicated_window_func::dedicated_window_func(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  static const char *funcs_no_args[] = {
    "row_number", "rank", "dense_rank"
  };
  static const char *funcs_with_args[] = {
    "lag", "lead", "first_value", "last_value"
  };

  if (d6() > 3) {
    func_name = funcs_no_args[d100() % 3];
    type = sqltype::get("int8");
  } else {
    func_name = funcs_with_args[d100() % 4];
    auto arg = make_shared<column_reference>(this);
    args.push_back(arg);
    type = arg->type;

    if ((func_name == "lag" || func_name == "lead") && d6() > 3) {
      args.push_back(value_expr::factory(this, scope->schema->inttype, false));
    }
  }

  partition_by.push_back(make_shared<column_reference>(this));
  while (d6() > 4)
    partition_by.push_back(make_shared<column_reference>(this));

  order_by.push_back(make_shared<column_reference>(this));
  while (d6() > 4)
    order_by.push_back(make_shared<column_reference>(this));
}

bool dedicated_window_func::allowed(prod *p)
{
  return window_function::allowed(p);
}

void dedicated_window_func::out(std::ostream &out)
{
  indent(out);
  out << func_name << "(";
  for (auto it = args.begin(); it != args.end(); it++) {
    out << **it;
    if (it + 1 != args.end()) out << ", ";
  }
  out << ") over (partition by ";
  for (auto ref = partition_by.begin(); ref != partition_by.end(); ref++) {
    out << **ref;
    if (ref + 1 != partition_by.end()) out << ", ";
  }
  out << " order by ";
  for (auto ref = order_by.begin(); ref != order_by.end(); ref++) {
    out << **ref;
    if (ref + 1 != order_by.end()) out << ", ";
  }
  out << ")";
}

bool_test::bool_test(prod *p) : bool_expr(p)
{
  match();
  expr = value_expr::factory(this, scope->schema->booltype, false);
  static const char *tests[] = {
    "IS TRUE", "IS FALSE", "IS UNKNOWN",
    "IS NOT TRUE", "IS NOT FALSE", "IS NOT UNKNOWN"
  };
  test_type = tests[d6() - 1];
}

void bool_test::out(std::ostream &out)
{
  out << "(" << *expr << " " << test_type << ")";
}

not_expr::not_expr(prod *p) : bool_expr(p)
{
  match();
  inner = bool_expr::factory(this);
}

any_all_expr::any_all_expr(prod *p) : bool_expr(p)
{
  match();
  lhs = value_expr::factory(this, nullptr, false);

  static const char *cmp_ops[] = { "=", "<>", "<", ">", "<=", ">=" };
  cmp_op = cmp_ops[d6() - 1];

  quantifier = (d6() > 3) ? "ANY" : "ALL";

  subquery = make_shared<query_spec>(this, scope);
}

void any_all_expr::out(std::ostream &out)
{
  out << "(" << *lhs << " " << cmp_op << " " << quantifier << " (";
  indent(out);
  out << *subquery << "))";
}

void any_all_expr::accept(prod_visitor *v)
{
  v->visit(this);
  lhs->accept(v);
  subquery->accept(v);
}

greatest_least::greatest_least(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  func_name = (d6() > 3) ? "greatest" : "least";
  int count = 2 + d6() / 2;
  auto first = value_expr::factory(this, type_constraint, false);
  type = first->type;
  args.push_back(first);
  for (int i = 1; i < count; i++) {
    args.push_back(value_expr::factory(this, type, false));
  }
}

void greatest_least::out(std::ostream &out)
{
  out << func_name << "(";
  for (auto it = args.begin(); it != args.end(); it++) {
    out << **it;
    if (it + 1 != args.end()) out << ", ";
  }
  out << ")";
}

row_constructor::row_constructor(prod *p)
  : value_expr(p)
{
  match();
  type = sqltype::get("record");
  int count = 2 + d6() / 2;
  for (int i = 0; i < count; i++) {
    elems.push_back(value_expr::factory(this, nullptr, false));
  }
}

void row_constructor::out(std::ostream &out)
{
  out << "ROW(";
  for (auto it = elems.begin(); it != elems.end(); it++) {
    out << **it;
    if (it + 1 != elems.end()) out << ", ";
  }
  out << ")";
}

array_subscript::array_subscript(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  // Generate an array expression and subscript into it
  string arr_type;
  if (type_constraint)
    arr_type = type_constraint->name + "[]";
  else
    arr_type = "integer[]";

  arr = value_expr::factory(this, sqltype::get(arr_type), false);
  index_val = 1 + d6();
  type = type_constraint ? type_constraint : scope->schema->inttype;
}

void array_subscript::out(std::ostream &out)
{
  out << "(" << *arr << ")[" << index_val << "]";
}

jsonb_access::jsonb_access(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  obj = value_expr::factory(this, sqltype::get("jsonb"), false);

  // ->> returns text, -> returns jsonb
  returns_text = (type_constraint && type_constraint->name == "text") || d6() > 3;

  if (d6() > 3) {
    // String key access
    static const char *keys[] = { "'a'", "'b'", "'key'", "'1'", "'name'", "'value'" };
    accessor = keys[d6() - 1];
  } else {
    // Integer index access
    accessor = to_string(d6() - 1);
  }

  type = returns_text ? sqltype::get("text") : sqltype::get("jsonb");
}

void jsonb_access::out(std::ostream &out)
{
  out << "(" << *obj << (returns_text ? " ->> " : " -> ") << accessor << ")";
}

position_expr::position_expr(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  type = scope->schema->inttype;
  substring_expr = value_expr::factory(this, sqltype::get("text"), false);
  string_expr = value_expr::factory(this, sqltype::get("text"), false);
}

void position_expr::out(std::ostream &out)
{
  out << "position(" << *substring_expr << " IN " << *string_expr << ")";
}

substring_expr::substring_expr(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  type = sqltype::get("text");
  string_expr = value_expr::factory(this, sqltype::get("text"), false);
  from_pos = 1 + d6();
  for_len = d6();
}

void substring_expr::out(std::ostream &out)
{
  out << "substring(" << *string_expr << " FROM " << from_pos
      << " FOR " << for_len << ")";
}

extract_expr::extract_expr(prod *p, sqltype *type_constraint)
  : value_expr(p)
{
  match();
  type = scope->schema->inttype;

  static const char *ts_fields[] = {
    "year", "month", "day", "hour", "minute", "second",
    "epoch", "dow", "doy", "quarter", "week",
    "millennium", "century", "decade",
    "microseconds", "milliseconds"
  };
  field = ts_fields[d100() % 15];

  // Pick a timestamp/timestamptz/interval/date/time source
  static const char *source_types[] = {
    "timestamp", "timestamptz", "interval", "date", "time"
  };
  string src_type = source_types[d100() % 5];
  source = value_expr::factory(this, sqltype::get(src_type), false);
}

void extract_expr::out(std::ostream &out)
{
  out << "extract(" << field << " FROM " << *source << ")";
}
