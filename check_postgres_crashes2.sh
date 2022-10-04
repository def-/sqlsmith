#!/bin/sh
grep "#0" postgres_crashes |
grep -v "#0  GetTupleForTrigger (estate=" | # https://github.com/yugabyte/yugabyte-db/issues/10152
grep -v "#0  0x00000000005b27bc in YbDatumToText (datum=0, data=" | # https://github.com/yugabyte/yugabyte-db/issues/11363
grep -v "#0  operator-> (this=" | # https://github.com/yugabyte/yugabyte-db/issues/11365
grep -v "pfree (pointer=" | # https://github.com/yugabyte/yugabyte-db/issues/11366
#grep -v "0x0000000000000000 in ?? ()" | # https://github.com/yugabyte/yugabyte-db/issues/11366
grep -v "in Delete (value=0x" | # https://github.com/yugabyte/yugabyte-db/issues/11366
grep -v "#0  palloc (size=1024) at" | # https://github.com/yugabyte/yugabyte-db/issues/11366
grep -v "send_message_to_server_log (edata=" | # https://github.com/yugabyte/yugabyte-db/issues/11366
grep -v "in yb::rpc::Proxy::DoAsyncRequest(" | # https://github.com/yugabyte/yugabyte-db/issues/11405
grep -v "in __sigprocmask (how=2, set=" | # https://github.com/yugabyte/yugabyte-db/issues/11404
grep -v "#0  sigusr1_handler (postgres_signal_arg=10)" | # https://github.com/yugabyte/yugabyte-db/issues/11404
cat
# grep "core." postgres_crashes | while read i; do rm $i; done
