# LithoLattice Engine — Standards Review
October 10, 2026. Prepared by Elfie for JD's review.

## Conventions under review

| Rule | Example (correct) | Status |
|------|-------------------|--------|
| Engine namespace `litholattice::` | `litholattice::EntityManager` | ✅ |
| Interfaces use `I` prefix | `IEntityBehavior`, `ITickable`, `ISignalNode` | ✅ |
| Constants use `k` prefix | `kSignalTransmittor`, `kDishSensitivity`, `kEvItemDrop` | ✅ |
| Components use -OR suffix | Overclockor, Pulsor, Memor, Transmittor, Receivor | ✅ (see fix below) |
| Wired medium = Trace | `red_trace`, `blue_trace` | ✅ |
| Wireless medium = Signal | `signal_transmittor`, `signal_receivor` | ✅ |

## Fix applied during this review

**`SimRole::SignalTx` / `SignalRx` → `SignalTransmittor` / `SignalReceivor`.**
The enum values were still abbreviated from before the total -OR lock. Renamed in
`blocks.h`, `blocks.cpp`, `signal.cpp`. All 329 tests pass after the rename.

## Audit notes

- **New interfaces** (`IEntityBehavior`, tick interfaces) follow the `I` prefix. ✅
- **New constants** (`kTransmittorPower`, `kDishSensitivity`, `kEvItemDrop`, `kEntityItem`) follow `k` prefix. ✅
- **New blocks** (`lithokraft:dish`, `lithokraft:signal_transmittor`, `lithokraft:signal_receivor`) use -OR and Trace/Signal terms. ✅
- **Save aliases** preserve old `pulsar`/`memory_cell`/`thresholder`/`red_wire`/`blue_wire` IDs for backward compat. ✅
- **No remaining `Wire` identifiers** in the wireless code path. The `isFlatWire`/`wireSupported` helpers predate the Trace rename and are internal; flag for a future cleanup pass if you want them gone too.

## Architecture diagrams

![Engine modules](sandbox://workspace/user/files/litholattice-uml-2026-10-10/engine-modules.png)

![Entity system](sandbox://workspace/user/files/litholattice-uml-2026-10-10/entity-system.png)

![Wireless signal flow](sandbox://workspace/user/files/litholattice-uml-2026-10-10/signal-flow.png)

## Open roadmap items (in order)

1. Entity rendering (items currently sim-functional, invisible)
2. Inventory integration for pickup
3. Radio registry channel indexing (currently O(n) per query)
4. Cross-dimensional signal registry
5. Multiblock Dish (after in-game disc validation testing)
6. `isFlatWire`/`wireSupported` internal rename (optional cleanup)
