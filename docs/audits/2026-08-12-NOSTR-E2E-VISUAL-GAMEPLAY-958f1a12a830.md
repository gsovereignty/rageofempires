# Nostr multiplayer gameplay audit

- Run: `20260812T212410Z-958f1a12a830`
- Started UTC: `2026-08-12T21:39:42.461455+00:00`
- Verdict: **BLOCKED**

## Result

Run stopped before acceptance completed. Infrastructure versus product classification remains unproved pending evidence review.

Primary failure: `MoveTargetOutOfBoundsException: Message: move target out of bounds
  (Session info: chrome=151.0.7922.109)
Stacktrace:
0   chromedriver                        0x000000010518ce84 chromedriver + 3427972
1   chromedriver                        0x0000000104e8c140 chromedriver + 278848
2   chromedriver                        0x0000000104f1a370 chromedriver + 861040
3   chromedriver                        0x0000000104f12830 chromedriver + 829488
4   chromedriver                        0x0000000104eca39c chromedriver + 533404
5   chromedriver                        0x000000010515d950 chromedriver + 3234128
6   chromedriver                        0x00000001051607b0 chromedriver + 3246000
7   chromedriver                        0x000000010514732c chromedriver + 3142444
8   chromedriver                        0x0000000105161088 chromedriver + 3248264
9   chromedriver                        0x0000000105139650 chromedriver + 3085904
10  chromedriver                        0x000000010517c580 chromedriver + 3360128
11  chromedriver                        0x000000010517c6e0 chromedriver + 3360480
12  chromedriver                        0x000000010518cb1c chromedriver + 3427100
13  libsystem_pthread.dylib             0x0000000193d03c04 _pthread_start + 132
14  libsystem_pthread.dylib             0x0000000193cfeba4 thread_start + 4
`
