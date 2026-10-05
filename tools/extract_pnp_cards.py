#!/usr/bin/env python3
"""Render the Root PNP card-front PDF into named PNG assets."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path


CARDS = [
    ("shared-deck", "bird-ambush-1", "Bird Ambush"),
    ("shared-deck", "bird-ambush-2", "Bird Ambush"),
    ("shared-deck", "birdy-bindle", "Birdy Bindle"),
    ("shared-deck", "armorers-1", "Armorers"),
    ("shared-deck", "armorers-2", "Armorers"),
    ("shared-deck", "woodland-runners", "Woodland Runners"),
    ("shared-deck", "arms-trader", "Arms Trader"),
    ("shared-deck", "bird-crossbow", "Crossbow"),
    ("shared-deck", "sappers-1", "Sappers"),
    ("shared-deck", "sappers-2", "Sappers"),
    ("shared-deck", "brutal-tactics-1", "Brutal Tactics"),
    ("shared-deck", "brutal-tactics-2", "Brutal Tactics"),
    ("shared-deck", "royal-claim", "Royal Claim"),
    ("shared-deck", "fox-ambush", "Fox Ambush"),
    ("shared-deck", "gently-used-knapsack", "Gently Used Knapsack"),
    ("shared-deck", "fox-root-tea", "Root Tea"),
    ("shared-deck", "fox-travel-gear", "Travel Gear"),
    ("shared-deck", "protection-racket", "Protection Racket"),
    ("shared-deck", "foxfolk-steel", "Foxfolk Steel"),
    ("shared-deck", "anvil", "Anvil"),
    ("shared-deck", "stand-and-deliver-1", "Stand and Deliver!"),
    ("shared-deck", "stand-and-deliver-2", "Stand and Deliver!"),
    ("shared-deck", "tax-collector-1", "Tax Collector"),
    ("shared-deck", "tax-collector-2", "Tax Collector"),
    ("shared-deck", "tax-collector-3", "Tax Collector"),
    ("shared-deck", "favor-of-the-foxes", "Favor of the Foxes"),
    ("shared-deck", "rabbit-ambush", "Rabbit Ambush"),
    ("shared-deck", "smugglers-trail", "Smuggler's Trail"),
    ("shared-deck", "rabbit-root-tea", "Root Tea"),
    ("shared-deck", "a-visit-to-friends", "A Visit to Friends"),
    ("shared-deck", "bake-sale", "Bake Sale"),
    ("shared-deck", "command-warren-1", "Command Warren"),
    ("shared-deck", "command-warren-2", "Command Warren"),
    ("shared-deck", "better-burrow-bank-1", "Better Burrow Bank"),
    ("shared-deck", "better-burrow-bank-2", "Better Burrow Bank"),
    ("shared-deck", "cobbler-1", "Cobbler"),
    ("shared-deck", "cobbler-2", "Cobbler"),
    ("shared-deck", "favor-of-the-rabbits", "Favor of the Rabbits"),
    ("shared-deck", "mouse-ambush", "Mouse Ambush"),
    ("shared-deck", "mouse-in-a-sack", "Mouse-in-a-Sack"),
    ("shared-deck", "mouse-root-tea", "Root Tea"),
    ("shared-deck", "mouse-travel-gear", "Travel Gear"),
    ("shared-deck", "investments", "Investments"),
    ("shared-deck", "sword", "Sword"),
    ("shared-deck", "mouse-crossbow", "Crossbow"),
    ("shared-deck", "scouting-party-1", "Scouting Party"),
    ("shared-deck", "scouting-party-2", "Scouting Party"),
    ("shared-deck", "codebreakers-1", "Codebreakers"),
    ("shared-deck", "codebreakers-2", "Codebreakers"),
    ("shared-deck", "favor-of-the-mice", "Favor of the Mice"),
    ("shared-deck", "rabbit-dominance", "Rabbit Dominance"),
    ("shared-deck", "mouse-dominance", "Mouse Dominance"),
    ("shared-deck", "bird-dominance", "Bird Dominance"),
    ("shared-deck", "fox-dominance", "Fox Dominance"),
    ("eyrie", "leader-commander", "Commander"),
    ("eyrie", "leader-despot", "Despot"),
    ("eyrie", "leader-builder", "Builder"),
    ("eyrie", "leader-charismatic", "Charismatic"),
    ("eyrie", "loyal-vizier-1", "Loyal Vizier"),
    ("eyrie", "loyal-vizier-2", "Loyal Vizier"),
    ("vagabond", "thief", "Thief"),
    ("vagabond", "tinker", "Tinker"),
    ("vagabond", "ranger", "Ranger"),
    ("quests", "fox-fundraising", "Fundraising - Fox"),
    ("quests", "fox-errand", "Errand - Fox"),
    ("quests", "fox-logistics-help", "Logistics Help - Fox"),
    ("quests", "fox-repair-a-shed", "Repair a Shed - Fox"),
    ("quests", "fox-give-a-speech", "Give a Speech - Fox"),
    ("quests", "rabbit-guard-duty", "Guard Duty - Rabbit"),
    ("quests", "rabbit-errand", "Errand - Rabbit"),
    ("quests", "rabbit-give-a-speech", "Give a Speech - Rabbit"),
    ("quests", "rabbit-fend-off-a-bear", "Fend off a Bear - Rabbit"),
    ("quests", "rabbit-expel-bandits", "Expel Bandits - Rabbit"),
    ("quests", "mouse-expel-bandits", "Expel Bandits - Mouse"),
    ("quests", "mouse-guard-duty", "Guard Duty - Mouse"),
    ("quests", "mouse-fend-off-a-bear", "Fend off a Bear - Mouse"),
    ("quests", "mouse-escort", "Escort - Mouse"),
    ("quests", "mouse-logistics-help", "Logistics Help - Mouse"),
    ("walkthrough", "eyrie", "Eyrie Walkthrough"),
    ("walkthrough", "vagabond", "Vagabond Walkthrough"),
    ("walkthrough", "woodland-alliance", "Woodland Alliance Walkthrough"),
    ("walkthrough", "marquise-de-cat", "Marquise de Cat Walkthrough"),
    ("faction-overview", "marquise-de-cat", "Marquise de Cat Overview"),
    ("faction-overview", "woodland-alliance", "Woodland Alliance Overview"),
    ("faction-overview", "eyrie", "Eyrie Dynasties Overview"),
    ("faction-overview", "vagabond", "Vagabond Overview"),
    ("spy", "rabbit", "Rabbit Spy"),
    ("spy", "mouse", "Mouse Spy"),
    ("spy", "bird", "Bird Spy"),
    ("spy", "fox", "Fox Spy"),
    ("vagabond", "arbiter", "Arbiter"),
    ("vagabond", "scoundrel", "Scoundrel"),
    ("vagabond", "vagrant", "Vagrant"),
    ("faction-overview", "lizard-cult", "Lizard Cult Overview"),
    ("faction-overview", "riverfolk-company", "Riverfolk Company Overview"),
]


def render_cards(source: Path, output: Path, dpi: int) -> None:
    if not shutil.which("pdftoppm"):
        raise RuntimeError("pdftoppm is required")
    if len(CARDS) != 95:
        raise RuntimeError(f"Expected 95 card definitions, got {len(CARDS)}")

    manifest = []
    for page, (group, card_id, title) in enumerate(CARDS, start=1):
        group_dir = output / group
        group_dir.mkdir(parents=True, exist_ok=True)
        destination = group_dir / card_id
        subprocess.run(
            [
                "pdftoppm", "-f", str(page), "-l", str(page), "-singlefile",
                "-r", str(dpi), "-png", str(source), str(destination),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
        )
        file_path = destination.with_suffix(".png")
        if not file_path.is_file() or file_path.stat().st_size == 0:
            raise RuntimeError(f"Page {page} did not render")
        manifest.append(
            {
                "id": card_id,
                "title": title,
                "group": group,
                "sourcePage": page,
                "file": file_path.relative_to(output.parent).as_posix(),
            }
        )

    with (output / "manifest.json").open("w", encoding="utf-8") as stream:
        json.dump(
            {
                "source": source.name,
                "dpi": dpi,
                "cardCount": len(manifest),
                "cards": manifest,
            },
            stream,
            ensure_ascii=True,
            indent=2,
        )
        stream.write("\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("assets/Root PNP (Final) Base Cards (Fronts).pdf"),
    )
    parser.add_argument("--output", type=Path, default=Path("assets/cards"))
    parser.add_argument("--dpi", type=int, default=300)
    arguments = parser.parse_args()

    if arguments.dpi < 72 or arguments.dpi > 600:
        parser.error("--dpi must be between 72 and 600")
    if not arguments.source.is_file():
        parser.error(f"source PDF not found: {arguments.source}")

    render_cards(arguments.source, arguments.output, arguments.dpi)
    print(f"Rendered {len(CARDS)} cards to {arguments.output}")


if __name__ == "__main__":
    main()
