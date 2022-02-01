#!/bin/sh
cat  ~/var/data/yb-data/tserver/logs/postgresql*.log | grep "TRAP: " |
grep -v 'FailedAssertion("!(bms_is_subset(appendrel->lateral_relids, required_outer))", File: "../../../../../../../src/postgres/src/backend/optimizer/util/relnode.c", Line:' | # https://github.com/yugabyte/yugabyte-db/issues/11233
grep -v 'FailedAssertion("!(!((allPgXact\[proc->pgprocno\].xid) != ((TransactionId) 0)))", File: "../../../../../../../src/postgres/src/backend/storage/ipc/procarray.c", Line:' | # https://github.com/yugabyte/yugabyte-db/issues/11235
grep -v 'BadArgument("!(((context) != ((void\*)0) && (((((const Node\*)((context)))->type) == T_AllocSetContext) \|\| ((((const Node\*)((context)))->type) == T_SlabContext) \|\| ((((const Node\*)((context)))->type) == T_GenerationContext))))", File: "../../../../../../../src/postgres/src/' | # https://github.com/yugabyte/yugabyte-db/issues/11250
grep -v "FailedAssertion(\"!(buf\[len - 1\] == '\\\\0')\", File: \"../../../../../src/postgres/src/common/psprintf.c\", Line: " | # https://github.com/yugabyte/yugabyte-db/issues/11251
grep -v 'FailedAssertion("!(IsSearchNull(ybScan->key\[i\].sk_flags))", File: "../../../../../../../src/postgres/src/backend/access/yb_access/yb_scan.c", Line:' | # https://github.com/yugabyte/yugabyte-db/issues/11253
grep -v 'FailedAssertion("!(tuple != ((void\*)0))", File: "../../../../../../src/postgres/src/backend/executor/execTuples.c", Line:' | # https://github.com/yugabyte/yugabyte-db/issues/11300
grep -v 'FailedAssertion("!(((const void\*)(fdw_trigtuple) != ((void\*)0)) ^ ((_Bool) (((const void\*)(tupleid) != ((void\*)0)) && ((tupleid)->ip_posid != 0))))", File: "../../../../../../src/postgres/src/backend/commands/trigger.c", Line:' | # https://github.com/yugabyte/yugabyte-db/issues/11303
sort | uniq -c | sort -n
