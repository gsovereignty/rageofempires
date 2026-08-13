# Nostr multiplayer gameplay audit

- Run: `20260813T000517Z-0df46074e3b8`
- Started UTC: `2026-08-13T00:06:09.214056+00:00`
- Verdict: **BLOCKED**

## Result

Run stopped before acceptance completed. Infrastructure versus product classification remains unproved pending evidence review.

Primary failure: `MoveTargetOutOfBoundsException: Message: move target out of bounds
  (Session info: chrome=151.0.7922.109)
Stacktrace:
0   chromedriver                        0x000000010474ce84 chromedriver + 3427972
1   chromedriver                        0x000000010444c140 chromedriver + 278848
2   chromedriver                        0x00000001044da370 chromedriver + 861040
3   chromedriver                        0x00000001044d2830 chromedriver + 829488
4   chromedriver                        0x000000010448a39c chromedriver + 533404
5   chromedriver                        0x000000010471d950 chromedriver + 3234128
6   chromedriver                        0x00000001047207b0 chromedriver + 3246000
7   chromedriver                        0x000000010470732c chromedriver + 3142444
8   chromedriver                        0x0000000104721088 chromedriver + 3248264
9   chromedriver                        0x00000001046f9650 chromedriver + 3085904
10  chromedriver                        0x000000010473c580 chromedriver + 3360128
11  chromedriver                        0x000000010473c6e0 chromedriver + 3360480
12  chromedriver                        0x000000010474cb1c chromedriver + 3427100
13  libsystem_pthread.dylib             0x0000000193d03c04 _pthread_start + 132
14  libsystem_pthread.dylib             0x0000000193cfeba4 thread_start + 4
`
