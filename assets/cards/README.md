# Root card assets

These PNG files were rendered at 300 DPI from
`assets/Root PNP (Final) Base Cards (Fronts).pdf`.

## Inventory

| Group | Count | Status |
| --- | ---: | --- |
| Shared deck | 54 | Complete base deck |
| Eyrie leaders and viziers | 6 | Complete |
| Vagabond characters | 6 | Complete for this PDF |
| Vagabond quests | 15 | Complete base quest deck |
| Walkthrough cards | 4 | Complete for the four base factions |
| Faction overview cards | 6 | Four base factions plus Lizard Cult and Riverfolk Company |
| Spy cards | 4 | Complete |
| **Total** | **95** | All source pages extracted |

`manifest.json` maps stable card IDs and source page numbers to image paths.

## Still missing for the game

- The web client does not load these card-front images yet.
- Only one generic `assets/card_back.webp` exists. A separate quest-card back,
  and any other deck-specific backs wanted by the UI, are not present.
- Lizard Cult and Riverfolk Company only have overview cards here; their full
  faction boards, pieces, setup data, and rules are not included in this PDF.
- Vagabond portraits already exist for Adventurer, Harrier, and Ronin, but this
  PDF has no matching character cards for them.
- The rules engine still needs Eyrie decree and turmoil, Woodland Alliance
  supporters and outrage, Vagabond items, quests and relations, crafting, and
  dominance before the extracted cards are fully playable.

## Regenerate

```sh
python3 tools/extract_pnp_cards.py
```
