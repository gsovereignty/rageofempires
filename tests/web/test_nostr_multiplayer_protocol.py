#!/usr/bin/env python3

import copy
import unittest

from nostr_multiplayer_protocol_test import (
    Failure,
    private_material_absent,
    validate_identity_lobby,
)


def peer_state(public_key: str, local_slot: int) -> dict[str, object]:
    host_key = "a" * 64
    join_key = "b" * 64
    event_ids = {
        "lobbyEventId": "c" * 64,
        "blueAckEventId": "d" * 64,
        "redAckEventId": "e" * 64,
        "blueReadyEventId": "f" * 64,
        "redReadyEventId": "1" * 64,
    }
    return {
        "publicKey": public_key,
        "matchId": "2" * 64,
        "hostPublicKey": host_key,
        "matchReference": "aoe-nostr:1:reference",
        "relays": ["wss://one.example", "wss://two.example"],
        "relayPoolDigest": "pool",
        "compatibilityDigest": "compatibility",
        "eoseRelays": ["wss://one.example", "wss://two.example"],
        "recentPublications": [{
            "results": [{"ok": True}, {"ok": True}],
        }],
        "game": {
            "protocolVersion": 1,
            "epoch": 1,
            "localSlot": local_slot,
            "hostPublicKey": host_key,
            "bluePublicKey": host_key,
            "redPublicKey": join_key,
            "configDigest": "config",
            "lobbyRevision": 2,
            **event_ids,
            "blueReady": True,
            "redReady": True,
        },
    }


class ProtocolIdentityLobbyTests(unittest.TestCase):
    def test_accepts_distinct_identities_and_exact_roster(self):
        host = peer_state("a" * 64, 0)
        join = peer_state("b" * 64, 1)

        result = validate_identity_lobby(host, join)

        self.assertEqual(result["blueSlotPublicKey"], host["publicKey"])
        self.assertEqual(result["redSlotPublicKey"], join["publicKey"])
        self.assertTrue(result["privateMaterialAbsent"])

    def test_rejects_equal_ephemeral_identities(self):
        host = peer_state("a" * 64, 0)
        join = peer_state("a" * 64, 1)

        with self.assertRaisesRegex(Failure, "identities"):
            validate_identity_lobby(host, join)

    def test_rejects_slot_pubkey_disagreement(self):
        host = peer_state("a" * 64, 0)
        join = peer_state("b" * 64, 1)
        join["game"] = copy.deepcopy(join["game"])
        join["game"]["redPublicKey"] = "3" * 64

        with self.assertRaisesRegex(Failure, "accepted-lobby diagnostics differ"):
            validate_identity_lobby(host, join)

    def test_rejects_missing_exact_ready_event(self):
        host = peer_state("a" * 64, 0)
        join = peer_state("b" * 64, 1)
        host["game"]["redReadyEventId"] = ""
        join["game"]["redReadyEventId"] = ""

        with self.assertRaisesRegex(Failure, "redReadyEventId"):
            validate_identity_lobby(host, join)

    def test_private_material_marker_is_rejected(self):
        self.assertFalse(private_material_absent({"private_key": "hidden"}))
        self.assertFalse(private_material_absent({"value": "nsec1hidden"}))
        self.assertTrue(private_material_absent({"publicKey": "a" * 64}))


if __name__ == "__main__":
    unittest.main()
