# VIC2.5 2026-11-19: outstanding branch integration

## Scope and merge graph

This report records actual Git commits and manual conflict resolutions, not a claim of full-game acceptance.

- Starting `main`: `2e39a4589cf4b4f244de3526f0b9373df011613f`, including the recent politics/content autonomous-cycle work.
- Integrated to `main` at `311ecb9197caba97b3b2fd495f07928add644351`: `work/vic3-115-industry-buildings-20261115` and all commits in `work/economy-logistics-merchant-credit-20261115`, including its ancestor `work/economy-daily-p0-p3-20261115`.
- Law source: `work/law-effects-115-20261110`, containing ancestor `work/p0-law-catalog-115-20261110`. Its distinct 148-law/71-amendment/1,117-row reference data, specific legal projection adapters, and compatible financial/real-asset actions are integrated into the latest canonical P2/C/D engine.
- Earlier political alternative branches `work/legislation-separation-20261109`, `work/politics-merge-20261110`, and `work/politics-ai-revolution-content-20261110` were developed against older, mutually incompatible political runtime versions. The current `main` already implements constitutional government, enactment/veto, politics AI, crisis resolution, and typed content commands. The newer P1/P2/P3/P4/P5/content runtime is the chosen authoritative implementation. Historic branches are preserved by merge ancestry, **not** by restoring duplicate obsolete implementations and their divergent serialized enums.

## Resolutions

- Existing law and politics single source of truth is `phase_c.hpp` / `phase_c_p2_impl.hpp`, not a competing law ledger. Imported 1.15 direct modifiers override older wiki-derived modifiers *within the same adopted law and key*, rather than summing them twice; amendment source effects read the same adopted law registry. Unsupported sponsor/conditional scripts are NOT marked executed.
- The main P1 numeric enum ordering and existing election/constitution P2 law decisions remain authoritative; alternative 1.15 spellings are explicit aliases, not renumbered save values.
- Main political POP/education/migration daily adjustments and current political legitimacy formula are kept, rather than layering an incompatible second demographic pass or second legitimacy calculation.
- Phase C cash/asset/ownership records support real paid/unpaid state acquisition and actual shareholder-funded recapitalization. Stage7/9 law decisions invoke these original C/D ledgers, without minting stock or money.
- Phase D exposes 1.15 research and production-yield calculation helpers; historical industry type is always explicit. The already-established current throughput path remains the only production output transformation; a separate second throughput factor is NOT applied.
- Phase G retains existing capacity-law callback and the newly integrated documented-service-only transport-route filter and merchant AI.
- Economic decisions from November 15, historical transport observations and building crosswalk remain preserved.

## Verification and release boundary

Focused Linux headless C++ tests passed on the economic branch before merging and the post-merge economy branch; law branch focused checks compile/execute the standalone 1.15 catalog, required source modifier rows, effective P2 law projection, and prior economy smoke. No new native/toolchain dependencies were installed.

The complete CMake/CTest, Windows compilation, 30k Province/1m POP performance/longrun, comprehensive historical industrial world dataset and all 1.15 scripted or conditional effects are not accepted by these focused checks. Historic branches can be archived without removing their Git history; duplicate legacy execution paths should not be restored.

## Remaining deliberate limitations

- Only observed or explicitly sourced transport services may dispatch goods; worldwide service capacity/fare/duration tables are not populated.
- Multiple existing legal modifiers still lack runtime consumers. Loading all source rows is not equivalent to 1.15 gameplay parity.
- Historic political alternative code not carried into the current runtime is superseded, not silently claimed fully operational.
