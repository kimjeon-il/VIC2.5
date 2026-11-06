# Phase B–H autonomous daily conversion: verified implementation inventory

> **Historical snapshot, superseded 2026-11-06:** Some references below to daily `candidate.advance(month_close_plan)` and monthly market repricing describe the prior baseline only. Current native runtime removed that callback and uses a single staged daily calendar rollover; see [NATIVE_CALENDAR_CUTOVER_2026-11-06.md](NATIVE_CALENDAR_CUTOVER_2026-11-06.md). Remaining F/H and global AI completion gaps still apply.

> 2026-11-06 · Branch `work/daily-full-b-h-conversion` at `c8e6a894896ea4ac47a290281dfa79e45a1195ed`
> Latest main `e1ae021dd4862564557e1206bc6a3784be788421` is already an ancestor of this branch (12 branch-only commits). No redundant merge or forced ref update required.
> **Status: PARTIAL, NOT IMPLEMENTATION COMPLETE.** A passing CI proves existing tests pass; it does not prove all autonomous monthly writers were converted.

## 1. Verified code and tests

| Area | Actual native day path | Remaining month-close or scope limitation |
|---|---|---|
| B population | `daily::World::advance_day` calls `phase_time_auto_demography(day,days_in_month)`, with old monthly hazard suppressed; birth/death/epidemic commands also use POP canonical ledger | POP age-month changes and other non-demographic life-cycle logic still run via `phaseb::World::finish_month()`; no full daily life-course/household migration and disease system |
| C firms and POP | `p0::DailyGame` runs configured staffing, actual manufacture, wage payment, buyer consumption, market pressure, food grievance, political membership and combat | P0 is an **opt-in configured policy layer**; bare `daily::World` does not autonomously derive all firms/households from ledger. `IntegratedWorld::advance()` still invokes stage9 grievances, stage11 `reprice_markets()` at MonthClose |
| D construction, industry, technology | P0 owner-target investment/work/payment/WIP, active site extraction with monthly cumulative flow, funded research; native next-day commissioning | Full general project scheduling, every asset maintenance/retirement, technology diffusion and all D auto decisions not proven day-native; legacy D finalize still at MonthClose |
| E spatial government | Day1 administrative legal effective-month queue, and P0 single-country election calendar | Election registry is not a complete multi-country election engine; other administrative queues/territorial actions need per-day coverage |
| F logistics | Actual G transport graph, cargo day dispatch/arrival/loss, monthly edge capacity prorated to day | No independent full F multi-modal network/traffic/cost/reliability AI; cannot call full Phase F complete |
| G trade | `phasegh::World::advance_daily` invokes `reserve_collateral`, `plan_contracts`, `plan_autonomous_trade`, `execute_autonomous_trade`, insurance daily; routes and real C inventory | Comprehensive endogenous global sourcing, bank/credit decisions, cross-market optimization and full logistics scope unverified |
| H diplomacy | Effective-day tariffs and treaty activation, month1 legal state, P0 tariff response to real price/stock | Autonomous multilateral negotiation, organization voting, crisis resolution and comprehensive diplomatic AI still not day-native |

## 2. Important remaining monthly writers (direct source inspection)

- `include/vic25/daily_time.hpp`: month end still invokes `candidate.advance(complete_month,true)`. This **is not merely a reporting rollup**.
- `include/vic25/phase_c.hpp`: `IntegratedWorld::advance()` still calls `collect_market_grievances()`, `people_.finish_month()`, `reprice_markets()`, and closes resource reservation scopes. Daily P0 uses separate `phase_time_close_daily_markets()` but monthly price path remains.
- `include/vic25/phase_d.hpp`: `World::advance()` still executes `next.finalize(live)` at month end. The native path has real daily construction/manufacture/extraction and separate daily resource finalize, but **all** D writers have not been proven migrated.
- `include/vic25/phase_gh.hpp`: monthly `advance()` has explicit `already_opened_on_calendar_day` safeguards against double G/H trade and legal state; it still forwards to monthly C/D/E settlement.
- `include/vic25/p0_daily.hpp`: `Policies` must be provided by a scenario/controller. An empty policy set does not instantiate full autonomous firm/government/household decisions from every ledger entity.
- Phase F full native C++ system and full H country AI are not present as complete independent engines.

## 3. What is already verified

- 12 commits ahead of current main, preserving latest main ancestry and no geological total-reserves model.
- P0 daily tests cover 12 bounded integration scenarios: production/trade/AI investment, food scarcity and POP political response, price close, combat with actual ammunition/POP injury, extraction, funded research and replay.
- Latest branch GitHub Actions run IDs: `42040148791` consolidated Linux/Windows, `42040148765` P0 daily gameplay, `42040148801` Phase C; all succeeded at `c8e6a894`.
- These are **bounded regressions**, not acceptance evidence for 1,200 months × 1 million POP cells × 30k provinces across complete B–H autonomous domains.

## 4. Completion criteria (must be measured, not asserted)

1. Enumerate each old monthly autonomous state writer with owning Phase and migrate it to a stage-valid day event or prove it is strictly month-end accounting. Remove any duplicate auto writer from `MonthClose`.
2. Bare game day path must activate relevant firm/household/market/government decisions based on actual ledger state; P0 configuration is allowed for preferences but cannot be the only source of all autonomous actions.
3. Implement full F and H domain autonomy, actual logistics/finance/insurance and legal decisions with ownership, money, goods, people, territory and diplomacy conservation.
4. Add targeted cross-month tests proving the same underlying economic/population event never posts both day and MonthClose; enforce rollback, save/reload, deterministic replay, 28/29/30/31 day months, long-run/large-world performance.
5. Re-run Linux/Windows CI and record results **on the exact completion commit**. Only then change this document to COMPLETE.

**Conclusion:** Latest main is integrated by ancestry. Current implementation is substantial but does **not** satisfy the user's full B–H completion criterion. Do not label it complete or merge a purported completion without the missing engine and tests.
