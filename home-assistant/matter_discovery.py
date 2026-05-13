#!/usr/bin/env python3
"""
Matter 设备发现辅助脚本
用于扫描和列出 Home Assistant 中的 Matter 设备

用法:
    python matter_discovery.py --url http://homeassistant.local:8123 --token <TOKEN>
"""

import argparse
import requests
import json


def get_states(url: str, token: str):
    headers = {
        "Authorization": f"Bearer {token}",
        "Content-Type": "application/json",
    }
    resp = requests.get(f"{url}/api/states", headers=headers)
    resp.raise_for_status()
    return resp.json()


def filter_matter_devices(states):
    return [s for s in states if "matter" in s.get("entity_id", "")]


def main():
    parser = argparse.ArgumentParser(description="Discover Matter devices in HA")
    parser.add_argument("--url", default="http://homeassistant.local:8123")
    parser.add_argument("--token", required=True)
    args = parser.parse_args()

    states = get_states(args.url, args.token)
    matter_devices = filter_matter_devices(states)

    print(f"Found {len(matter_devices)} Matter device(s):\n")
    for dev in matter_devices:
        eid = dev["entity_id"]
        state = dev["state"]
        name = dev["attributes"].get("friendly_name", eid)
        print(f"  • {name}")
        print(f"    Entity: {eid}")
        print(f"    State:  {state}")
        print()


if __name__ == "__main__":
    main()
