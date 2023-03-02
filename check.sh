#!/usr/bin/env bash
grep "^Error: " $* |
grep -v "result exceeds max size of" | # Seems expected with huge queries
grep -v "cannot reference pseudo type pg_catalog.list" | # https://github.com/MaterializeInc/materialize/issues/17870
grep -v 'operator is not unique: "char" = character' | # https://github.com/MaterializeInc/materialize/issues/17871
grep -v "Invalid data in source, saw retractions" | # https://github.com/MaterializeInc/materialize/issues/17874
sort | uniq -c | sort -n
