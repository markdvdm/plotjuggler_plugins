#!/usr/bin/env python3

import os
import sys

import git

git_repo = git.Repo(os.path.abspath(__file__), search_parent_directories=True)
git_root = git_repo.git.rev_parse("--show-toplevel")
py_msg_dir = os.path.join(git_root, "generated", "py")
sys.path.append(py_msg_dir)

from elroy_common_msg.enums.MessageId import MessageId
from elroy_common_msg.msgs.BusObject import BusObject

# Build a 'HeartBeat' message
msg_dict = {
    "component": {"component": 0, "instance": 0, "location": 0},
    "ecm_version": {
        "major": 1,
        "minor": 2,
        "patch": 3,
        "build": 4,
        "commit_hash": [0] * 32,
        "commit_hash_len": 20,
        "dirty_status": True,
    },
    "messenger_version": {
        "major": 5,
        "minor": 6,
        "patch": 7,
        "build": 8,
        "commit_hash": [1] * 32,
        "commit_hash_len": 20,
        "dirty_status": False,
    },
}
bus_obj_fields = {"id": MessageId.HeartBeat, "write_timestamp_ns": 0, "msg": msg_dict}

msg_bytes = BusObject.build({"fields": {"value": bus_obj_fields}})
msg_parsed = BusObject.parse(msg_bytes)

msg_bytes_hex = " ".join(["{:02x}".format(x) for x in msg_bytes])
print(f"built message:\n\n{msg_bytes_hex}\n")
print("parsed message:\n")
print(msg_parsed)

assert msg_parsed.fields.value.msg["ecm_version"]["major"] == 1
assert msg_parsed.fields.value.msg["ecm_version"]["minor"] == 2
assert msg_parsed.fields.value.msg["ecm_version"]["patch"] == 3
assert msg_parsed.fields.value.msg["ecm_version"]["build"] == 4

assert msg_parsed.fields.value.msg["messenger_version"]["major"] == 5
assert msg_parsed.fields.value.msg["messenger_version"]["minor"] == 6
assert msg_parsed.fields.value.msg["messenger_version"]["patch"] == 7
assert msg_parsed.fields.value.msg["messenger_version"]["build"] == 8
