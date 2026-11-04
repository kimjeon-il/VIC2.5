# Resource flow migration — Phase D legacy resource stock → Phase J-R1

> status: C++ bounded runtime replacement committed, full J-R1 engine NOT IMPLEMENTED
> supersedes resource reserve-depletion clauses of the historical [Phase D design branch](https://github.com/kimjeon-il/VIC2.5/tree/work/phase-d-design/docs/phase-d); details: [J-D0](J-D0_COMMON_CONTRACT.md) / [J-R1](J-R1_RESOURCE_EXPLORATION_FLOW.md)

## 1. Existing code baseline and required change

Previously `include/vic25/phase_d.hpp` modeled every resource as `ResourceDeposit` with `remaining`, the stage4 `extract_resource` required `remaining>=units`, and Phase D stage0 regenerated only `renewable` sources. The Phase D snapshot encoded a `remaining` field in each deposit.

**New contract is breaking**:
- Remove underground `remaining`, `estimated_quantity`, `confirmed_extractable_stock`, `known_stock_end`, fake geological total. No hidden randomly generated quantity for minerals/oil/coal/gas.
- Keep verified *production history* `cumulative_output` and *current operational flow* `monthly_capacity` with within-month `produced_this_month` for nonrenewables; cumulative output is not a reserve and has **no constraint imposed by an initial total**.
- For renewables only, keep `standing_biomass`, `renewal_per_month`, `carrying_capacity` and harvest conservation. There is no separate renewable economic subsystem.
- The existing C Financial/Inventory/Production, D facility/labor reservations remain sole real money/goods/workforce owners. Extraction requires owned/located active facility and actual labor/site services in stage4.
- `decline_ppm_active_month` and fixed-point `decline_remainder` yield deterministic active-month throughput change; parameters are production-flow model metadata, not mineral reserves. Site capacity may fall to 0 without a preordained last barrel.
- The current bounded C++ implementation does not yet calculate exploration, recovery technology improvements, market-driven reopenings, asset-level geological costs, GIS province prospectivity or all J-N1/P1/D1 interactions. These remain scoped follow-on tasks rather than silently claiming completion.

## 2. Runtime and API compatibility

| Historical Phase D | New active contract | Rule |
|---|---|---|
| `ResourceDeposit`, `DomainState.deposits` | `ResourceSite`, `DomainState.resource_sites` | no underground total/stock schema |
| `seed_resource_deposit` | `seed_resource_site` | nonrenewable site initial quantity is monthly **flow**, not original reserve |
| `World.resource(id).remaining` | `World.resource_site(id).monthly_capacity/cumulative_output` | no 'remaining oil' query |
| stage4 `remaining -= units` | `produced_this_month += units` + `cumulative_output += units` | production validated by actual site capacity/labor/inventory |
| renewable `remaining` | `standing_biomass` | actual living stock; regrows via verifiable rules |
| `VIC25_PHASED_SNAPSHOT_5` / `phase-d-cpp/0.5` | `_6` / `phase-d-cpp/0.6` + `phase-j-resource-flow/1.0` | no backwards decode; old save must be rejected |

Former `ResourceDeposit.remaining=100` on a nonrenewable site **must NOT be mapped to** `monthly_capacity=100`: units and meaning differ. Conversion requires independently sourced or calibrated site throughput (e.g. historically plausible barrel/month, facility scale); an old save without that information is incompatible and must be rejected. The runtime deliberately rejects `phase-d-cpp/0.5` snapshots rather than silently misreading the obsolete data.

Phase E and G/H snapshots embed Phase D codec and therefore inherit this breaking version requirement; new saves re-encode the updated source state. Full parent contract version integration remains future J engine work.

## 3. Earlier Phase D design clashes and resolution

The older D-D0 and D-I2 design lives on `work/phase-d-design` (not merged into the current runtime branch). The following **specific** assumptions no longer apply:
- D-D0 I2 resource ownership row's in-situ underground stock; replace with J ResourceSite/exploration status and actual physical site rights, not an independent stored stock.
- D-D0 `DD0-INV-08` still mandates actual extraction/harvest provenance, but NOT underground `remaining` quantity, reserve depletion, or conserved unextracted geological total.
- D-I2 `ResourceDeposit.estimated_quantity?` / `confirmed_extractable_stock` / `known_stock_end = known_stock_open + verified_discovery - physical_extraction` are abolished for nonrenewables.
- D-I2 `HarvestPool` regeneration, real renewable biomass accounting, industry co-products/waste, asset/labor/material/finance verification, actual output `InventoryLedger` remain valid.
- Historic measurement of *past realized extraction output* remains valid. Re-labelling it as 'hidden possible recoverable total' is prohibited.

When the original D design docs are folded into a consolidated branch, replace their affected table rows/equations and bump associated design ABI/ADR. **This document plus J-D0/J-R1 is the superseding contract on this branch**, not an assertion that the separate historical branch was rewritten.

## 4. Verification status and gates

Implemented (code changes):
- Legacy resource type, total-depletion check, serializer fields and seed/query APIs removed.
- Active-site monthly throughput bound, activity-based decline and real renewable biomass bound added.
- Version-bumped Phase D snapshot with strict rejection of old contract.
- Industry regression case now extracts more in two months than the old fictitious initial `remaining` while respecting monthly site capacity, tests decline, renewable growth, stage4 rollback, snapshot roundtrip.
- Static contract audit script checks absence of the obsolete resource model in the active source/serializer.

Required further verification:
- Compile and execute focused Phase D/Phase E/Phase G-H CTest on Linux and Windows in this branch's CI.
- Add complete J-R1 geological exploration, technology/cost/closing/reopening, duplicate discovery, environmental coupling and historical calibration acceptance.
- Run 30k Province/100만 POP/1,200개월 integration and records/rollback/replay; no unmeasured performance PASS.

**Never reinterpret a historical resource-stock test/seed as a verified new flow simulation without converting its meaning and testing it.**
