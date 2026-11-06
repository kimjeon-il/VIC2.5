# VIC2.5 – Native calendar rollover without legacy MonthClose adapter

> 2026-11-06 · work/daily-fh-autonomy-integration · Scope: runtime time orchestration, NOT full Phase B–H feature certification

## Decision

Native game time advances one Gregorian day at a time, using **one** `daily::World::advance_day(const phasegh::World::Plan& native_day_plan={})` call. Every supplied phase callback runs on that exact date. The earlier `advance_day(month_close_plan, native_day_plan)` interface and its exceptional `candidate.advance(legacy,true)` fallback are deleted, not silently ignored or retained as an undocumented option.

The true last day of the Gregorian month does not start another 12-stage autonomous simulation. In a **single** staged `phasegh::World::advance_daily` transaction, the Phase C stage-10 operation updates the calendar-month cursor and the age-dependent employment eligibility after the day's real POP births/deaths, battle and healthcare events. Stage 11 then validates the C/D/E/G/H ledgers. One day commits exactly once to the authoritative C and `daily::World` commit counters. Failure anywhere discards **both** that day's domain changes and its calendar rollover; the day, queued commands, cargo status and journal remain unchanged.

Propagation is explicit:
- `daily::World::advance_day(...)` → `phasegh::World::advance_daily(tasks,day,...,final_calendar_day)`
- `phasee::World::advance_daily(...,first_calendar_day,final_calendar_day)`
- `phased::World::advance_daily(...,first_calendar_day,final_calendar_day)`
- `phasec::IntegratedWorld::advance_daily(...,final_calendar_day)`

`p0::DailyGame::advance_day(additional_today)` accepts only today-specific actions and combines them with current game-state-driven P0 policies before entering that same transaction; there is no separate month-end plan.

## Removed compatibility mechanisms

- No `month_close_plan` argument, callback whitelist, or deferred execution at month end.
- No `candidate.advance(legacy,true)` from `daily::World`.
- No second `close_native_calendar_month()` pass or second month-end C++ commit.
- No `already_opened_on_calendar_day` argument in the independent Phase C/D/E/G/H month-only simulation API; its special double-booking conditional branches are removed.
- No transient `suppress_demography_this_close_` workaround for preventing duplicated Phase B monthly demography.

The independent legacy `phasec/phased/phasee/phasegh::World::advance(plan)` **still exists for old standalone monthly regression fixtures and engine compatibility**, but it cannot be invoked through the daily game coordinator. Existing explicit migration of previous binary save formats remains supported; reading a save is not executing the old monthly simulation.

## Tests and invariants

- Compile-time `std::is_invocable_v` check rejects restoration of the two-plan `daily::World::advance_day` signature.
- `test_daily_time`: January 31 failing real-world payment rolls back the entire day; successful retry advances exactly one C commit and one calendar day; February 1 native callback takes effect immediately.
- `test_daily_native`: daily demography, actual construction/manufacture/cargo/treaty, leap-day/month boundary, snapshot determinism.
- `test_p0_daily`: autonomous production/market and POP politics never re-execute for a full month at calendar rollover.
- Full CTest Linux/Windows and separate Phase C and P0 CI required on exact final head; report skipped G/H acceptance IDs as BLOCKED, never PASS.

## Remaining limitations

This change removes the **legacy runtime execution compatibility branch**, not the entire feature backlog. Bare `daily::World` is not an autonomous country/firm controller without scenario policies; phase F real-world multimodal congestion/risk/time-data coverage, full international-organization/war/crisis H AI and large-world active-day CPU/RAM acceptance remain separate objectives. Do not label Phase B–H fully complete on the basis of this cutover alone.
