# 일별 네이티브 사건 — Phase B/C/D/G/H 통합 구현 상태

> 2026-11-05 · 브랜치 `work/daily-native-b-h` · 시간 계약 `vic25-time/2.1-native-slice`
> **IMPLEMENTATION_PARTIAL**: 실제 일별 사건/저널이 구현된 것과 전체 분야 자동 연산의 일별 전환을 구분한다.

## 실행 경로 및 정본

- B POP: `Phase C::phase_time_birth/death`가 Phase B 단일 `PopulationSlice` 원본을 사건으로 실제 변경. 날짜는 `daily::World::journal`에 저장. `birth_month`은 기존 월별 분류로 남아 있고 일별 자동 확률·정확한 `birth_day`는 아직 전면 구현되지 않음.
- C 생산: `Kind::production`에서 Phase C 고용·원자재·상품 원장의 실제 투입·산출 실행. 공유 일별 노동 예약으로 같은 작업자의 중복 이용을 방지.
- D 산업: `Kind::manufacture`에서 D의 실제 `IndustryRecipe`, 시설·재고·노동·부산물을 사용하고 **월간 설비용량을 그날까지의 누적 가용량으로 환산**하여 일별 단위 반복으로 증식하지 않도록 제한. 수치는 정수식 `floor(monthly_capacity * elapsed_day / days_in_month)` 기반 월간 누적 예산임.
- D 건설: `Kind::construction` 실제 계획→시공→대금 지급→WIP 자본화→다음날 자산 준공. 실물 재고·고용·현금·시설에 원자 적용. 복잡한 Package curing/inspection/기술 도입은 추후.
- G/F 물류: `Kind::cargo_dispatch`에서 G가 등록한 실제 운송 간선의 `edge_usage[month,edge]` 공유 월간 용량을 사용하며 C의 실제 `TRANSIT:DAY:<id>` 재고로 운송. 실물 자산 소유권과 운송계약은 임의 변경하지 않음. 특정 날짜의 자동 도착/분실은 C Inventory Ledger의 실제 정산. F 완성형 독립 C++ 엔진은 아직 없음.
- H 외교: `Kind::treaty_offer/ratify`는 H `DiplomacyState.daily_treaties`의 유효·법적 정부 권한을 검증하고 지정한 일자에 자동 발효. 해당 쌍·상품의 G 관세 면제 판정에 실제 반영. 전체 H 다자 조약/국제기구/위기·전쟁 개입의 일별화는 미구현.
- B~H 기존 월간 자동 계산은 **월말에 1회**만 실행. 동일한 월별 예산·인구/원자재 수량을 하루마다 반복하지 않음.
- 지리 프로빈스 10k~20k (설계 상한 30k)와 렌더링 원본은 변경하지 않으며, 매일 모든 프로빈스/POP을 순회하지 않는 idle tick 경로 유지.

## 네이티브 단계

Stage 0: 당일 운송 도착/조약 발효/대기 준공
Stage 1: 건설 자금·설비 예약
Stage 4: 생산/건설 실노동·재료 사용
Stage 5: 실제 지급
Stage 7: 지리 간선 검증 및 물리 선적
Stage 8: 건설 WIP 자본화
Stage 9: 적법한 외교 승인
Stage 10: 인구 출생·사망
Stage 11: 전원장·날짜·예약 검증

**일별 사건의 장애 시** 해당 DayIndex의 POP/자금/실물 재고/시설/외교/예약/월말 정산 전체 롤백.

## 버전 호환성

- `vic25-daily-save/2.0` Command 추가 필드 및 Cargo ETA/손실·수송 간선; 기존 1.0 세이브는 명시적 이전 코덱으로 읽고 새 포맷으로 재저장.
- `phase-gh-cpp/0.3`: 일자별 조약 원장이 있을 때만 사용하며, 조약이 없는 저장은 기존 `0.2` 형식을 그대로 재인코딩 가능. 구형 월별 유효기간은 억지로 일별 날짜로 해석하지 않음.
- Phase C/D/E 원본 원장은 기존 권위와 분개 구조 유지. **새 일별 사건의 세이브 이전**과 기존 구월별 세이브 자동 변환은 별개의 경로이며 각각 시험 필요.

## 독립 실행 증거

- [GitHub Actions 네이티브 CI](https://github.com/kimjeon-il/VIC2.5/actions/workflows/daily-tick-integration.yml): Linux/Windows 분리. `tests/test_daily_native.cpp`는 실제 POP/산업/건설/물류/외교/손실·세이브·월경계·원자 롤백을 검사.
- `tests/test_daily_time.cpp` 이전 Gregorian/기본 일별 원장·월말/세이브 마이그레이션 회귀.
- 기존 Phase B/C/D/E/G/H 대표 CTest도 동일 브랜치에서 별도로 실행. **이 테스트만으로 전체 B~J native-day 전환 완료를 선언하지 않음.**

## 미완료와 다음 단계

1. 자동 POP 출생·사망·질병·이주율을 실제 `day` 기반 **하루별 hazard/carry와 정확한 출생일 코호트**로 변경. 월말 B `DemographyRules`에 중복 적용되지 않게 원장 마이그레이션.
2. C/E/G/H의 일별 수요·가격·생산 결정, 조세·금융·외교 위기/모든 조약 조항 전체와 AI의 실제 일별 행동.
3. D 산업 모든 현장/시설 단위의 정확한 일일 노동시간·생산량 제한, 공사 인허가·검수·기술 채택·교육/연구 흐름의 전면 일별화.
4. F 물류의 다중 간선 최적경로·지연·혼잡·운임/보험·세계 30k Province 정확한 지리·저사양 GPU 성능.
5. I/J 실제 C++ 분야 런타임·일별 정기 및 사건 전이. J는 지하 전체 자원매장량을 쓰지 않는 설계 유지.
6. 액티브 데이 `World` 깊은 복사를 30k Province/100만 POP에서 copy-on-write 또는 변경집합 journal로 최적화; CPU/RAM, 월별/일별 1,200개월 장기 성능과 결정론 검증.
