# Commercial Multiplayer Adapter ABI

Reconstruction does not redistribute or link Steamworks. Users may opt in to a
licensed platform adapter loaded with `load_commercial_multiplayer_adapter`.
Adapter includes `aoe/commercial_multiplayer_adapter.h`, exports
`aoe_commercial_multiplayer_adapter_v1`, and returns
`AoeCommercialMultiplayerAdapterV1`.

ABI v1 covers identity, authentication tickets, lobby create/join/leave and
metadata mutation, owner transfer, and reliable channel-addressed P2P packets.
Steam-backed adapters must use reliable delivery on channel 0 to match supplied
HD executable evidence. Adapter owns returned context and display-name storage;
`destroy` releases both. Ticket and receive calls use two-pass sizing: null
buffer returns pending byte count, then same call fills exact-sized buffer.

Lobby discovery and snapshot methods intentionally throw in ABI v1: no stable C
shape is published for their variable-sized results yet. This seam therefore
does not by itself establish original-client compatibility. It avoids an SDK
build dependency and lets locally licensed adapters exercise proved platform
callbacks and transport behavior.
