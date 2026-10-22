# LithoKraft morning update for Elfie (Friday 2026-10-16, 4:40 to 5:23 AM ET)
Prepared by the Squad for JD to pass along.

## Canon decided this morning
- **Mere Glass + sand = the litho key.** Your keypair brief is now canon. The item is written "Mere Glass" (M-E-R-E) and said like "mere"; JD calls it a homonym. Mere Glass is salvaged from the abandoned fab, and sand is found nearby. To get more, players go back to the Organic in their Organic Armor. Farming it is a later idea.
- **Unlocking lithography:** the litho key leads to the etcher, then chip ore, then computing, then player-built text editors and small computers.
- **Underwafer:** sits directly under the Wafer's crust. You can dig down to it and break the bedrock-style blocks. Below that is a shield barrier that only jump gates get you through.
- **Gates:** on the first trip, you return through the same gate. Once a dimension is unlocked, gates can be placed anywhere and work as fast travel. JD wants the first crossing to be hard, so it feels like a real achievement.
- **The Logbook:** the in-game journal's official name ("the Log" for short).
- **Electro Bunny (new mob):** keeps going and going. Mostly white ("follow the white rabbit"), with rare black and colored ones. White bunnies sometimes lead players to an important place.

## Naming standards
- Every -or ending is lowercase: Etchor, Emitor, Observor, Connector, Selector, Portor, Seedor, and so on.
- In the code, interfaces start with a capital I followed by a capital letter (IVehicle, IGlider), and proper nouns are capitalized.

## Engine (Cortana's morning build; the hash held)
- Saves now use .lat files. The world clock is saved, a day lasts 32 minutes, and the prayer windows are 09:00, 13:00 and 17:00.
- Foliage switches to a cheaper cross-hatch model in the distance, which raised the software renderer from 37 to 61 FPS. Entities now draw on Vulkan at about 750 FPS.
- Up next: the Litho GUI overhaul, IVehicle and IGlider stubs (gliders and ramps), and a naming-standards refactor.

## Fair-play design notes
- Vehicle and glider movement is simulated by the game itself. The client only sends what the player pressed.
- The game records dimension unlocks, and the save gets a checksum so it can't be edited.
- When you unlock a dimension, the Logbook adds an "I made it." entry with a seal glyph. Player-written text can't produce that glyph, so the entry can't be faked.

## Still open
- Does the Wafer have a finite outer edge? The Notch and Edge Exclusion depend on it.
- How should Mere Glass farming work? (Later.)
