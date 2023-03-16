#!/usr/bin/env bash
cat $* |
grep -a -E "^(Broken|Syntax|Error) " |
# Expected AFTER a crash, the query before this is interesting, not the ones after
grep -v "Broken 08000: no connection to the server" |
grep -v "failed: Connection refused" |

grep -v "violates not-null constraint" |
grep -v "division by zero" |
grep -v "operator does not exist" | # For list types
# Refinement:
grep -v "value too long for type" |
grep -v "list_agg on char not yet supported" |
grep -v "does not allow subqueries" |
grep -v "range constructor flags argument must not be null" |
grep -v "function pg_catalog.array_remove(" |
grep -v "function pg_catalog.array_cat(" |
grep -v "function mz_catalog.list_append(" |
grep -v "function mz_catalog.list_prepend(" |
grep -v "does not support implicitly casting from" |
grep -v "aggregate functions that refer exclusively to outer columns not yet supported" | # https://github.com/MaterializeInc/materialize/issues/3720
grep -v "range lower bound must be less than or equal to range upper bound" |
grep -v "length must be nonnegative" |
grep -v "is only defined for finite arguments" |
grep -v "more than one record produced in subquery" |
grep -v "invalid range bound flags" |
grep -v "invalid input syntax for type jsonb" |
grep -v "invalid regular expression" |
grep -v "invalid input syntax for type date" |
grep -v "invalid escape string" |
grep -v "invalid hash algorithm" |
grep -v "is defined for numbers greater than or equal to" |
grep -v "is not defined for zero" |
grep -v "is not defined for negative numbers" |
grep -v "requested character too large for encoding" |
grep -v "internal error: unrecognized configuration parameter" |
grep -v "invalid encoding name" |
grep -v "invalid time zone" |
grep -v "value out of range: overflow" |
grep -v "LIKE pattern exceeds maximum length" |
grep -v "negative substring length not allowed" |
grep -v "cannot take square root of a negative number" |
grep -v "timestamp units not yet supported" |
grep -v "step size cannot equal zero" |
grep -v "stride must be greater than zero" |
grep -v "timestamp out of range" |
grep -v "unterminated escape sequence in LIKE" |
grep -v "null character not permitted" |
grep -v "is defined for numbers between" |
grep -v "field position must be greater than zero" |
grep -v "must appear in the GROUP BY clause or be used in an aggregate function" |

grep -v "Expected joined table, found" | # Should fix for multi table join
grep -v "Expected ON, or USING after JOIN, found" | # Should fix for multi table join
grep -v "but expression is of type" | # Should fix, but only happens rarely
grep -v "coalesce could not convert type map" | # Should fix, but only happens rarely
grep -v "operator does not exist: map" | # Should fix, but only happens rarely
grep -v "result exceeds max size of" | # Seems expected with huge queries
grep -v "expected expression, but found reserved keyword" | # Should fix, but only happens rarely with subqueries
grep -v "Expected right parenthesis, found left parenthesis" | # Should fix, but only happens rarely with cast+coalesce
grep -v "coalesce could not convert type character" | # https://github.com/MaterializeInc/materialize/issues/17899
grep -v "coalesce could not convert type \"char\"" | # https://github.com/MaterializeInc/materialize/issues/17899
grep -v "invalid selection: operation may only refer to user-defined tables" | # Seems expected
#grep -v "cannot reference pseudo type" | # https://github.com/MaterializeInc/materialize/issues/17870
grep -v "Invalid data in source, saw retractions" | # https://github.com/MaterializeInc/materialize/issues/17874
#grep -v "internal error: unimplemented join" | # https://github.com/MaterializeInc/materialize/issues/17897
grep -v "Evaluation error: internal error: invalid input type" | # https://github.com/MaterializeInc/materialize/issues/17807
grep -v "' not recognized" | # https://github.com/MaterializeInc/materialize/issues/17981
grep -v "internal error: Invalid data in source, saw negative accumulation for key" | # https://github.com/MaterializeInc/materialize/issues/17509
grep -v "internal transform error: scalar types do not match" | # https://github.com/MaterializeInc/materialize/issues/18023
grep -v "array_agg on arrays not yet supported" | # https://github.com/MaterializeInc/materialize/issues/18044
grep -v "Unsupported temporal predicate." | # https://github.com/MaterializeInc/materialize/issues/18048
grep -v "OneShot plan has temporal constraints" | # https://github.com/MaterializeInc/materialize/issues/18048
grep -v "unexpected ScalarExpr in uncorrelated plan" | # https://github.com/MaterializeInc/materialize/issues/18188
grep -v "internal error: cannot evaluate unmaterializable function" | # https://github.com/MaterializeInc/materialize/issues/14290
sort | uniq -c | sort -n
