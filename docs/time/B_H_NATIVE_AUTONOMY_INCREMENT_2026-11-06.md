# B–H native daily runtime — playable strategic-core incremental implementation

> 2026-11-06 · `work/daily-fh-autonomy-integration`
> **Implementation partial.** This report records actual implemented code and bounded integration tests. It is not a claim that all Phase B–H automation, Phase F or H is complete.

## Baseline preserved

- Latest `main` lineage and `work/daily-full-b-h-conversion` behavior are ancestors of this branch. The branch preserves the actual POP, bank, goods, construction, transport, territory and diplomacy registries.
- The old two-plan MonthClose callback was removed. A single `daily::World::advance_day(native_day_plan)` dispatches stages 0–11, including Gregorian last-day POP age classification **in the same atomic commit**. Standalone historical monthly tests and explicit old binary-save decode are separate and are not called by the day runtime.
- No finite geological reserve was reintroduced. Resource sites remain operational-flow and renewable-biomass bounded; no fictitious country-wide resources, debt or political points are created.

## New native C++ functionality

| Phase | Implementation | Authoritative ledger / invariant |
|---|---|---|
| B | Existing daily birth/death/illness/migration risk and month-boundary age update continue in the real Phase B `PopulationSlice` | No extra headcount or second monthly hazard |
| C | New configurable `p0::DebtService`: everyday available cash above a real reserve floor retires actual outstanding bank-loan principal, capped at the daily policy limit | `phasec::IntegratedWorld::outstanding_loan` reads original balanced C bank liability; `repay` posts genuine cash/liability reductions |
| C/G | Credit-line-aware repayment: active G `CreditFacility.drawn` obligations are paid via `Stage::repay_trade_credit` **before** standalone C debt, so actual bank principal and G utilization never diverge | No second credit balance; shared deterministic C/G staged event and checkpoint |
| D | New `phase_time_maintain_facility`: company repairs a damaged production facility on today's date, requiring **existing hired workforce**, daily shared labor slots and physical construction input consumption; optional daily gross payroll booked under effective law | Facility `damaged` immediately decreases; inventory decreases; wage transfer is balanced; pending old scheduled repairs cannot overbook the same physical damage |
| E | Existing first-day territory/legal administrative effects retained, with no month-close replay | One actual sovereign/jurisdiction owner |
| F | Multi-policy autonomous transport planning uses only a **day-local provisional reservation** over G's existing physical transport graph: it accounts for other AI lanes' pledged stock, freight cash and capacity, reroutes via a usable alternative when the cheapest edge is congested | No persistent shadow goods/network ledger; G/C settle freight and ETA/loss in original bank/physical cargo; route preference and reservations deterministic |
| G | Existing day-by-day merchant route/contract execution, credit collateral, delivery and cargo insurance remain; matched daily C/G repayments added | Credit collateral and capacity cannot be spent twice |
| H | New scenario-bounded autonomous country treaty risk decisions: a state can ratify actual multilateral treaty clauses only if available real treasury covers a declared fiscal exposure and any trade, military or territorial commitments are authorized | Uses existing treaty registry, real prime-minister identity, exact month-effective legal state |
| H | New autonomous international-organization council votes: actual member-government authorization, C treasury support threshold, quorum, yes requirement and veto rules determine one recorded H decision | Idempotent real organization decision IDs, no invented approvals and no separate vote ledger |

## Executable regression scenarios

`tests/test_p0_daily.cpp` now exercises 20 bounded scenarios, including all original game-core tests and new:

- a damaged facility's daily repair after a real physical casualty/damage event; material and wages are actually spent, the site becomes available again, and an error at a later stage rolls back the full day;
- a treaty with real fiscal aid terms, an organization decision accepted by quorum, another blocked by a government veto, future-only legal activation and forged government authorization rollback;
- multiple autonomous freight lanes competing for one seller's stock and carrier cash, and fallback to actual alternative G edges under exact day-constrained throughput; checkpoint replay retains every shipment and reservation effect;
- C bank principal amortization respecting a minimum operating liquidity balance, and synchronizing real G trade-credit line utilization rather than silently writing the C ledger only.

Validation target: consolidated Linux/Windows CTests, P0 and Phase C verification on **the same** release commit. Previous green runs on earlier commits do not certify a later untested update. G/H acceptance items tagged BLOCKED remain BLOCKED.

## Explicit gaps (DO NOT mislabel complete)

1. **B**: policy-independent migration choice, entire daily schooling/age sub-month precision, full disease provenance, and national-scale demographic/household autonomy.
2. **C**: economy-wide endogenous firm formation, pricing/supply, realistic debt interest, government tax spending, complete political-party and combat operational decisions without scenario-specific P0 mandates.
3. **D**: full industry/technology diffusion, asset maintenance policy generation without scenario choices, multi-phase permits and long project financing/insurance.
4. **E**: authoritative multi-country electoral result registry (old Phase C implementation still has one global election result), territory/administration effective-day data for historical campaigns.
5. **F**: validated global ground/sea/rail/air intermodal freight, simultaneous manual-vs-autonomous trade scheduling, historically sourced per-edge transit times, probabilistic delay/risk and >30k Province performance.
6. **G/H**: full global sourcing/credit/default/insurance automation and complete multilateral diplomacy, foreign intelligence and war-peace sovereign AI. Legal permissions do not magically fulfil monetary or military obligations.
7. **Scale/data**: 1200-month deterministic full-world load, million POP sparse cohort peak RAM, actual campaign datasets, and active-day copy-on-write performance acceptance.

**Result:** Real game-loop improvements exist in C/D/F/G/H, but implementation status remains PARTIAL until all owners and acceptance gates are verified.
