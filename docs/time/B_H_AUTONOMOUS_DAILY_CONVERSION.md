# Phase B–H native daily autonomous simulation — conversion contract

> Status: IMPLEMENTATION IN PROGRESS / NOT COMPLETE
> Branch: `work/daily-autonomous-b-h` (based on main `bbf2954f560673bda73ad40b6460267b0686481a`)
> Date: 2026-11-05
> Scope: all **autonomous** B/C/D/E/F/G/H calculations. This is not a new social-simulation phase or an excuse to add detailed cultural/religious systems.

## 1. Baseline and mandatory compatibility

Historical baseline (2026-11-05): `daily::World::advance_day()` originally called `candidate.advance(month_close_plan)`. **Superseded 2026-11-06** by [NATIVE_CALENDAR_CUTOVER_2026-11-06.md](NATIVE_CALENDAR_CUTOVER_2026-11-06.md): the daily runtime no longer exposes `month_close_plan` and commits calendar rollover at day-stage 10. This removes legacy monthly automatic replay **from the actual daily path** but does not finish all autonomous B–H systems.

- Keep PopulationSlice as the sole person ledger; money/production/stock/ownership/employment/asset and G/H diplomatic ledger have single authoritative owners.
- Keep `J-R1` geological **no-total-reserves** rule; keep I-C1 language as culture metadata and religion effects conditional on law. Neither I nor J is included in this B–H delivery.
- Use one DayIndex for event ordering, with stable command IDs, deterministic per-entity random streams, and an atomic per-day Commit. Daily actions and scheduled events must not double count monthly-close effects.
- Convert monthly budgets (labor, asset capacity, finance, goods, contracts, shipping) to **cumulative day-available ceilings**; never grant the whole monthly budget each day. Do not round away subunit entitlements; carry integer remainder.
- A monthly rollup is allowed for accounting/reporting, interest accrual settlement and period-end reporting, but **must not re-run production, births, deaths, consumption, migration, taxes, construction, investment, AI decisions, trade or diplomatic actions already processed each day**.
- Historical data unknown is not zero; existing scenario and ledger schemas are not replaced by fictional nationwide values.

## 2. Native daily acceptance matrix

| Phase | Autonomous day-by-day work required | Current baseline | Completion gate |
|---|---|---|---|
| B POP | Births, deaths, disease progression, migration, aging eligibility; date-effective demographic transitions | Only explicit birth/death/epidemic; autonomous DemographyRules at MonthClose | Day-by-day hazard/carry with one-person integer conservation and no monthly duplicate |
| C Economy | Firm supply decisions, labor, production, wages, real consumption, market clearing/price updates, fiscal/finance; political POP response & military actions | Explicit production/consumption/payment, monthly automatic stages | Daily autonomous closed production→income→demand→price→political/military decisions; ledger reconciliation |
| D Construction/industry/technology | Autonomous company investment, site scheduling, maintenance, research/tech adoption and commissioning | Explicit construction/manufacture with daily capacity; other D lifecycle monthly | Daily actual resource-limited schedules; site/labor reservation single-use; no double commissioning |
| E Geography/administration | Effective-day ownership/administrative assignment, elections, local-government law effects | Spatial day stage and static registry; administrative queues monthly | Day-effective legal/election queues; stable province/region IDs; no phantom geography |
| F Logistics | Actual routing, transit, delays, congestion, costs, losses, inventory/contract settlement | G actual edge dispatch/arrival, no independent complete F | Deterministic multi-edge route & capacity reservation, ETA, delivered goods, freight & insurance without teleportation |
| G Trade | Firm autonomous sourcing, contracts, shipment planning, financing, customs, insurance and arrivals | Explicit G cargo, monthly automatic trade clearing | Day-level cross-market trade plans with real goods/money/route, customs & delivery settlement |
| H Diplomacy | Autonomous foreign policy and crisis response, treaty negotiations, organizations, obligations and war/peace effects | Explicit daily treaties/tariffs; other diplomatic lifecycle monthly | Day-effective legal decision and state transitions with actual AI decisions and validated obligations |

**Important:** Phase C autonomous firm decisions and Phase H state decisions depend on a minimal country/firm AI; do not substitute a deterministic no-op or random fake decision to claim full implementation.

## 3. Migration sequencing

1. **S0 contracts & baseline tests**: inventory every monthly automatic writer and identify its owner/phase, input snapshot, output ledger and rollback invariants. Explicitly mark unconverted writers.
2. **S1 B/C economic day loop**: demographic hazards and autonomous firm supply/consumption/market/labor. Move each old monthly writer to day loop, leaving month-end aggregation only. Add invariant proving 30-day totals match constrained expected bounds, not blindly 30×.
3. **S2 D/E/F**: asset investment/maintenance/research; day-effective administrative law and election events; real multi-edge freight/capacity/costs. Preserve existing next-day commissioning and actual edge dispatch.
4. **S3 G/H**: daily autonomous contracts, financing/customs/insurance, diplomatic crisis/treaty/organization state and country decisions. No player-only actions passed off as autonomy.
5. **S4 full cutover**: legacy monthly automatic mutators disabled/removed from the daily World path. MonthClose only closes period/rolls accounting and changes month metadata. Save ABI and old save migration explicitly versioned.
6. **S5 validation**: Linux/Windows full CTest, long-run 1,200 months, deterministic replay/worker count, rollback after arbitrary day, 10k/20k/30k Province and large POP load; quantify CPU/RAM.

## 4. Non-negotiable cutover tests

- **No hidden monthly simulation:** record ledger/event hashes before and after month boundary; no duplicated day production, income, deaths, migration, trade or government actions.
- **Conservation:** person counts change only by recorded births/deaths/migration (world migration net zero), goods by real production/consumption/transit/loss, money by valid postings.
- **Time:** day n legal rules/tech/ownership apply only when effective, including leap years, month boundaries and late cargo; no future-date effects.
- **Resource bounds:** labor and site hours, monthly→daily cumulative capacities and actual commodity balances never go negative.
- **Autonomy:** without user-scheduled commands, firms, POPs, markets and countries can produce genuine permitted daily outcomes from initial economic/social/political conditions. A fast idle day remains possible where there is truly no active simulation work.
- **Reproducibility:** same world+seed+commands yields identical world/event hashes across save/reload, worker count and platform.
- **Integration:** B–H cross-domain causes propagate through one authoritative ledger and deterministic event graph; no duplicated parallel domain economy.

## 5. Current explicit blockers

- Phase B `birth_month` and `Engine::advance(World&)` are monthly; daily cohort birth date/hazard rules require a backward-compatible population schema and calibrated conversion.
- Phase C `IntegratedWorld::advance()` calls `open_month`, monthly stage 9 political grievances and stage 10 demography, while `advance_daily()` deliberately skips those. A safe conversion must refactor these writers, not call monthly advance 30 times.
- Phase D `begin_month` handles renewable growth/flow reset and construction lifecycle; Phase E administrative due queues and G/H monthly trade/foreign policy lifecycle still rely on `advance()`.
- Full Phase F C++ logistics engine and autonomous country/firm decision loop are absent; these are real implementation dependencies, not documentation-only items.
- Current day fast path skips all domains when there are no explicit commands/due events. This is incompatible with an always-active autonomous economy and must be replaced with dirty/active region scheduling.

**Definition of done:** All rows in section 2 implemented, all tests in section 4 actually passing on both operating systems, and no monthly autonomous writer remaining on the daily execution path. Until then, status is PARTIAL.
