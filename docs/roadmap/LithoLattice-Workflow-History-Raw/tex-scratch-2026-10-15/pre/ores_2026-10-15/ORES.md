# LithoKraft ores, 2026-10-15 (CloudAtlas)
Searched every .md under /workspace/alpha-docs for ore, mineral, vein, deposit, ingot and metal names. 14 ores in total (copper from r2 plus 13 new).
Style: v3b. Substrate ores sit on the v3b substrate (with faint glints); deep ores sit on a darker, warmer "deep" substrate (no glints).
Every block and icon here is **not registered** in the block or item registry yet. Generator: `scripts/refresh128_ores.py`.
"Inferred" means the docs don't give that detail, so I chose it from the name or real-world chemistry.

Wafer layers (LithoLattice-Dimension-Vertical-Architecture-2026-10-10.md:17): Substrate y120–199 holds "Ores".
Underwafer (Master-Builder-Brief:46): Contact, Kiln, Forge, Sear.

| ore (block / raw icon) | source doc : line | where found | colour / shape | inferred? |
|---|---|---|---|---|
| copper_ore / (copper icon) | LithoKraft-Game-Systems-2026-10-10.md:16 (smelt ores to copper and silver ingots); Overclockor tier trim, Master-Builder-Brief:53; JD 2026-10-15 decision | Wafer Substrate (JD: copper is an ore you dig down to) | copper #6A3A22 to #E8A84C (Procedural-Texture-Generator:105), nuggets | shape inferred. Already drawn in r2 |
| silver_ore / raw_silver | Game-Systems:16; Master-Builder-Brief:53 (Overclockor copper→silver→violet) | Wafer Substrate | light silver, nuggets | location + look inferred |
| titanium_ore / raw_titanium | Voxel-Compute-Elfie-Response-v1.2b-2026-10-09.md:18 (128-TPS tier: tungsten + titanium) | Wafer Substrate | pale blue-gray, vein streaks | location + look inferred |
| tungsten_ore / raw_tungsten | Elfie-Response-v1.2b:18 | Deep Wafer (y40–119) | dark steel, vein streaks | location + look inferred |
| gold_ore / raw_gold | Elfie-Response-v1.2b:19 (256-TPS tier: gold + hafnia); white-gold tier trim, Voxel-Compute-Core-Loop:53 | Deep Wafer | gold #6A4A0E to #FFE58A, nuggets | location + look inferred |
| hafnia_ore / raw_hafnia | Elfie-Response-v1.2b:19 | Deep Wafer or Underwafer | white with a lattice tint (hafnium oxide), crystals | location + look inferred |
| startin_ore / raw_startin | Voxel-Compute-Lore-Bible-v0.1.md:14 ("deep ores (including startin, the EUV key)"); Core-Loop:17 | Underwafer, deep | EUV ultraviolet violet with a white star core, star clusters | look inferred from "EUV" and "star" |
| hypervisor_ore / raw_hypervisor | Master-Builder-Brief:46, :66; Lore-Glossary-2026-10-11.md:55; Lore-Brief-for-Grok:34 | Underwafer, the Sear (deepest) | deep violet to lattice, hex cells ("isolation" and shielding) | look inferred |
| salt_deposit / raw_salt | Lithokraft-Cortana-Catchup-Brief:21 ("chemical baths/salts"); Lore-Bible-v0.1:165 | Underwafer | white cubic crystals | look inferred |
| coal_ore / raw_coal | Game-Systems:17 (furnace fuel: coal) | Wafer Substrate | black with a blue-gray sheen, nuggets | location + look inferred. Docs only name coal as a fuel, not as an ore |
| aluminum_ore / raw_aluminum | Elfie's material icons aluminum and aluminum_block (no ore line in the docs) | Wafer Substrate | aluminum ramp, nuggets | fully inferred |
| gallium_ore / raw_gallium | Elfie's icons gallium and silicon_gallium (no ore line in the docs) | Wafer Substrate | pale blue-silver, droplets (gallium melts) | fully inferred |
| quartz_ore / raw_quartz | Core-Loop:46 and Elfie-Response-2026-10-09:25 ("optics-grade quartz", an Organic World material); Elfie-Response-v1.2b:17 (64-TPS tier: copper + quartz) | Organic World (not in the alpha). Drawn on the Wafer substrate host for now | pale lattice-white crystals | host inferred. Flag: Organic World |
| amber_ore / raw_amber | Lore-Bible-v0.1:212 ("Amber (rare, fossilized)": optics) | Organic World sap line (not in the alpha). Drawn on the substrate host for now | amber #6A3206 to #FFD27A, droplets | host inferred. Flag: Organic World, and fossil resin rather than a true ore |

## Not drawn as ores (noted)
- **carbide, nitride, dioxide, titanium_oxide, photoresist, resist:** compounds or processed materials, not mined ores. Nitride comes from phosphoric acid (First-Day-On-The-Wafer:114).
- **iron and the PBR "gold":** they appear only in the generic PBR material defaults (PBR-Implementation-Report:17), not in the lore.
- **The "Ore Veins" biome** (LithoKraft-Biomes-Proposal:17): a biome, not a mineral.
- **copper:** no raw_copper icon was saved, because the copper ingot icon already exists. The sheet shows a raw copper preview only.
