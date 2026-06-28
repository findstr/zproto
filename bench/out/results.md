# zproto benchmark results

## size (bytes)

| message | zproto-nopack | zproto-pack | pb |
|---|---:|---:|---:|
| chat | 150 | 74 | 66 |
| login | 43 | 35 | 32 |
| heartbeat | 18 | 13 | 12 |
| snapshot | 3376 | 1162 | 1114 |
| frame | 46 | 24 | 32 |
| alltypes | 3492 | 1511 | 1955 |

## throughput (ops/sec)

| message | op | cpp-zproto | cpp-pb |
|---|---|---:|---:|
| alltypes | decode | 151792 | 54343 |
| alltypes | encode | 91194 | 176192 |
| alltypes | encode_pack | 60756 | 176192 |
| alltypes | unpack_decode | 54149 | 54343 |
| chat | decode | 2717527 | 1017387 |
| chat | encode | 1784171 | 3098295 |
| chat | encode_pack | 1181371 | 3098295 |
| chat | unpack_decode | 1075561 | 1017387 |
| frame | decode | 12235948 | 9841371 |
| frame | encode | 4624316 | 9908850 |
| frame | encode_pack | 3222027 | 9908850 |
| frame | unpack_decode | 3748092 | 9841371 |
| heartbeat | decode | 14187200 | 11659723 |
| heartbeat | encode | 7822923 | 12515648 |
| heartbeat | encode_pack | 5264982 | 12515648 |
| heartbeat | unpack_decode | 6829643 | 11659723 |
| login | decode | 11073645 | 6531133 |
| login | encode | 5379395 | 8657097 |
| login | encode_pack | 3086045 | 8657097 |
| login | unpack_decode | 3447429 | 6531133 |
| snapshot | decode | 177895 | 78014 |
| snapshot | encode | 97864 | 389596 |
| snapshot | encode_pack | 66863 | 389596 |
| snapshot | unpack_decode | 59432 | 78014 |
