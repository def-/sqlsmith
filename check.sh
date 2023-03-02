#!/usr/bin/env bash
cat $* |
grep -E "^(Syntax|Error) " |
grep -v "violates not-null constraint" |
grep -v "division by zero" |
grep -v "but expression is of type" | # Should fix, but only happens rarely
grep -v "coalesce could not convert type map" | # Should fix, but only happens rarely
grep -v "operator does not exist: map" | # Should fix, but only happens rarely
grep -v "result exceeds max size of" | # Seems expected with huge queries
grep -v "coalesce could not convert type character" | # https://github.com/MaterializeInc/materialize/issues/17899
grep -v "coalesce could not convert type \"char\"" | # https://github.com/MaterializeInc/materialize/issues/17899
grep -v "invalid selection: operation may only refer to user-defined tables" | # Seems expected
grep -v "cannot reference pseudo type pg_catalog.list" | # https://github.com/MaterializeInc/materialize/issues/17870
grep -v 'operator is not unique: "char" = character' | # https://github.com/MaterializeInc/materialize/issues/17871
grep -v "Invalid data in source, saw retractions" | # https://github.com/MaterializeInc/materialize/issues/17874
grep -v "internal error: unimplemented join" | # https://github.com/MaterializeInc/materialize/issues/17897
sort | uniq -c | sort -n
