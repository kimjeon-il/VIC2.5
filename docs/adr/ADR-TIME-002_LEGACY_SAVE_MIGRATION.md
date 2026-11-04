# ADR-TIME-002 — 월별 C/D/E/GH 스냅샷의 안전한 일별 세이브 이전

> 계약: `vic25-daily-save/1.0` · 기준 `vic25-time/2.0` · 모든 변환은 명시적 opt-in

## 지원 목록

| 원본 | 실제 직렬화 코드 | 마이그레이션 방식 |
|---|---|---|
| Phase C | `phasec::SnapshotCodec::decode` raw / `load` file | `phasec::IntegratedWorld → phased::World → phasee::World → phasegh::World` 빈 분야 sidecar만 추가 |
| Phase D | `phased::SnapshotCodec::decode` | D World → E World → GH World |
| Phase E | `phasee::SnapshotCodec::decode` | E World → GH World |
| Phase G/H | `phasegh::SnapshotCodec::decode` | GH World 그대로 wrapping |
| Phase B 단독 | 단독 canonical save codec 없음 | 명시적 unsupported. 인구원본을 임의 재구성하지 않음 |
| Phase F | 완성형 런타임·독립 save codec 없음 | 미지원 |
| Phase I/J | 상세 설계 중심; 독립 C++ save codec 없음 | 미지원 |
| 기존 월중 세이브 | 날짜별 누적 자원·사건 근거 없음 | 자동 이전 **거부** |

## 검증 및 무손실 정책

1. 이전 source의 codec이 실제 payload를 체크섬/계약/원장 무결성으로 검증하고, 구 포맷임을 확인한다.
2. snapshot에 저장된 `month_index`가 1년 1월 이후 Gregorian month ordinal 범위인지 검사. year = floor(month/12), month = month % 12 +1.
3. 신규 날짜는 해당 월 **1일**. 이전 snapshot의 마지막 확정 월이 이미 종료되었다는 해석이므로 같은 달의 이전 일자에 사건을 만들어넣지 않음.
4. 실제 인구·금융·재고·주권·고용·상환/계약/역사 이벤트는 **단일 원본 payload를 그대로 보존**. 빠진 새 도메인은 비활성/빈 초기 sidecar이며 보정된 가상 자금/사람을 생성하지 않음.
5. source payload digest 및 format/contract와 migration report를 신규 snapshot에 기록, replay 시 원본 idempotency set을 보존.
6. 새 하루 커밋 후 이전 양식으로 export하지 않음(데이터 소실 방지). 다른 calendar/ruleset/숫자 정책은 별도 명시적 migration 없으면 거부.
7. version lock을 `phase-gh-cpp/0.2`, `phase-e-cpp/0.4` 등의 **실제 codec**에 맡김. unrecognized older snapshots를 legacy로 자동 다운그레이드하거나 깨진 byte를 복구해 통과시키지 않음.
8. 월간 stage의 내부 기준시점은 매일 호출하지 않으며 **월말에 정확히 한 번**. 월중 일별 ledger transaction이 월간 통계/국민계정에 어떻게 반영될지 구체적인 증빙 없으면 `FULL_DAILY_ACCOUNTING`을 PASS로 선언하지 않음.

## 실제 인수 필요

- 생략 없이 동일 원본 Phase C/D/E/GH snapshot bytes를 migrate 직후 encode해서 비교.
- 2000년 2월 윤년·1900년 비윤년·12월/1월 rollover.
- 손상 payload/version mismatch → 실패하고 원본 경로 불변.
- 신규 snapshot 일별 reload 후 같은 command sequence, 동일 world payload checksum.
- 월말 사건과 당일 사건을 묶은 atomically rollback.
- 게임 날짜/계약 날짜를 임의 배분하지 않음.
