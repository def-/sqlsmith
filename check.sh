#!/usr/bin/env bash
cat $* |
grep -E "^(Broken|Syntax|Error) " |
grep -v "violates not-null constraint" |
grep -v "division by zero" |
grep -v "cannot reference pseudo type" | # TODO: Fix, happens often with catalog items: anyrange etc
grep -v "Expected left square bracket, found right parenthesis" | # TODO: Fix, happens often with map/map[] and similar types
# Refinement:
grep -v "array_agg on arrays not yet supported" |
grep -v "binary date_bin is unsupported" |
grep -v "sum(interval) not yet supported" |
grep -v "invalid input syntax for type jsonb" |
grep -v "invalid regular expression" |
grep -v "aggregate functions are not allowed in" |
grep -v "invalid input syntax for type date" |
grep -v "invalid escape string" |
grep -v "invalid hash algorithm" |
grep -v "nested aggregate functions are not allowed" |
grep -v "is defined for numbers greater than or equal to" |

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
grep -v "operator is not unique: character" | # https://github.com/MaterializeInc/materialize/issues/17899
grep -v "operator is not unique: \"char\"" | # https://github.com/MaterializeInc/materialize/issues/17899
grep -v "invalid selection: operation may only refer to user-defined tables" | # Seems expected
grep -v "cannot reference pseudo type pg_catalog.list" | # https://github.com/MaterializeInc/materialize/issues/17870
grep -v 'operator is not unique: "char" = character' | # https://github.com/MaterializeInc/materialize/issues/17871
grep -v 'operator is not unique: character = "char"' | # https://github.com/MaterializeInc/materialize/issues/17871
grep -v "Invalid data in source, saw retractions" | # https://github.com/MaterializeInc/materialize/issues/17874
#grep -v "internal error: unimplemented join" | # https://github.com/MaterializeInc/materialize/issues/17897
grep -v "Evaluation error: internal error: invalid input type" | # https://github.com/MaterializeInc/materialize/issues/17807
sort | uniq -c | sort -n
