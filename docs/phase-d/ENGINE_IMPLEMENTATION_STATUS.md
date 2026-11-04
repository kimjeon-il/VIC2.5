# Phase D C++ engine — bounded integration vertical slice implementation status

> **Status: PARTIAL_IMPLEMENTATION; design contracts are not fully implemented or accepted.**  
> 2026-11-04 · branch: `work/phase-d-cpp-integration` (based on `work/phase-c-engine-integration`).  
> Upstream D-D0/DC/DI/DT/D-V design authority: [work/phase-d-design](https://github.com/kimjeon-il/VIC2.5/tree/work/phase-d-design/docs/phase-d).

## What the patch actually adds

1. [`include/vic25/phase_d.hpp`](../../include/vic25/phase_d.hpp): Phase D domain state and stage context run inside **Phase C's existing 12-stage `IntegratedWorld::advance`**, using a staged copy of both Phase C and D state. A failure of any C/D stage drops all D state, and Phase C's underlying atomic advance drops all its ledger/POP changes.
2. [`include/vic25/phase_c.hpp`](../../include/vic25/phase_c.hpp): additive stage-restricted bridge methods:
   - Actual material consumption from `InventoryLedger` at stage4;
   - Checked multi-input/output production from `InventoryLedger` and actual Phase B employment;
   - Cash-settled paid expenses reclassified to a `FinancialLedger` WIP account at stage8;
   - Next-month stage0 `ConstructionProject` commissioning, WIP→fixed-asset bookkeeping, C-E1 prototype facility registration and `OwnershipLedger` title issue;
   - Read-only actual employed workforce and WIP balance.
3. [`tests/test_phase_d.cpp`](../../tests/test_phase_d.cpp): 10 bounded C++ integration test cases (not mapped to all formal D-V test IDs):
   - Two-month construction, payment→WIP and next-month facility commissioning.
   - Funded research with actual reagents/labor and next-month knowledge activation.
   - Industry recipe input/output and capacity bounds.
   - Same-firm technology knowledge vs independently scheduled site adoption.
   - Stage4 manufacturing→stage7 real material delivery→following-month construction.
   - Factory production vs research double-use of the same POP workers, with full rollback.
   - Research without actual reagents rejected with no changed world.
   - Late-stage fault aborts preceding materials/wages/WIP/employment and returns to same month.
   - Unsupported retrofit recipe is explicitly rejected, not silently manufactured.
   - Repeatable small industrial production and event-journal order across independent runs.
4. `CMakeLists.txt` registers `vic25_phase_d_tests` as an additional CTest.

**Check status:** The tests are only declared until actual GCC/MSVC CI evidence is recorded; check the PR checks before assigning PASS. The Phase C baseline 48 synthetic tests are not evidence for D-V 72/279 acceptance.

## Explicitly unfinished or simplified

- C-E1 prototype facility is a simple `ProductionFacility` (capacity/damage), **not the complete historic Firm/Site/CapitalAsset/ProductionRecipe accounting model**. The new bridge uses its existing actor and ledger identities but needs migration once CE models become full.
- Construction pilot requires an actual **paid** unit contract and material + labor; it does **not** implement cost-plus/unpaid progress invoices, variable quality, contractors/land claims, demolition, or all D-C45 cases. WIP reclasses paid expense; outstanding claims/household self-build require future E3/E4 API.
- Industry recipe supports actual delivered inputs/outputs and plant capacity with staffing, not resource stock/renewability, non-storable real-time electricity dispatch, local clustering, multiple currency full settlement, or macroeconomic equilibrium.
- Research is explicitly funded/paid and reagent/worker/lab-limited but uses a deterministic evidence threshold for its test rule. No full uncertainty engine, technology contact graph, patents/IP, public schooling/POP skill transition, or C-M technology training is implemented yet.
- Adoption only permits existing compatible facilities; equipment-heavy `needs_retrofit` recipes are **explicitly rejected** until D-C commissioned retrofits and POP training integration exist. No nationwide free productivity bonus.
- Stage1's Phase D budget reservation is currently a **domain-specific staged reservation**, checked against Phase C spendable cash at Stage1 and again at actual Stage5 payment. It is NOT yet the single Phase C reservation ledger for all C/E/Politics/Military tasks, so L1-DONE is not claimed.
- Phase D state is currently held in the wrapper and staged atomically at runtime. It is **not yet represented in Phase C snapshot codec**; cross-process Phase D save/load/replay is pending.
- Phase C's older `produce` path has no shared Phase D resource budget table; external callers mixing legacy commands and new Stage methods must be migrated to the single resource reservation contract.
- `I2-DISPATCH` real-time nonstorable energy production/consumption remains unsupported and requires the previously identified C-E1/C-D4 change.
- No claims of 1,200-month full Phase D simulation, 100k/1m active POP with industrial/project/research loads, cross-platform byte identical saves, historical calibration or full Phase D/D-V acceptance.

## Gate mapping

| Gate | Actual state |
|---|---|
| **DV-L0** Phase C C++ base selected/branched, design-contract comparison | **PARTIAL** — C source selected, design branch remains separate; full migration/source-of-truth audit not accepted |
| **DV-L1** single World/typed identifiers/all domain ledger owners | **PARTIAL** — atomic Phase C-backed wrapper & prototype bridge, but ledger reservation and snapshot integration incomplete |
| **DV-L2** all C/I/T features | **PARTIAL** — initial bounded real production/construction/research/adoption loop |
| **DV-L3** 72 new + 207 upstream + POP/CE/CP/CM | **NOT_DONE** — 10 new bounded cases added, full 279 suite not implemented/executed |
| **DV-L4** save/replay, multi-worker determinism | **NOT_RUN** |
| **DV-L5** 1200 months/scale calibration | **NOT_RUN** |
| **DV-COMPLETE** | **NOT_DONE** |

The implementation should remain on a draft PR until the necessary gates are actually implemented and validated. Do not merge with `main` or call this a complete industrial grand-strategy game.
