#!/bin/sh
grep "#0" postgres_crashes |
grep -v "#0  GetTupleForTrigger (estate=" | # https://github.com/yugabyte/yugabyte-db/issues/10152
grep -v "#0  0x00000000005b27bc in YbDatumToText (datum=0, data=" | # https://github.com/yugabyte/yugabyte-db/issues/11363
grep -v "#0  operator-> (this=" | # https://github.com/yugabyte/yugabyte-db/issues/11365
grep -v "pfree (pointer=" # https://github.com/yugabyte/yugabyte-db/issues/11366
