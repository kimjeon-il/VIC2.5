# P0 — 일간 대전략 최소 실행 순환

> Branch: work/p0-strategic-gameplay-loop. Based on work/daily-full-b-h-conversion at d0c0331d32c13e9cad92ed897df8b2eef839574f. Status: bounded C++ implementation; not full release acceptance.

## 실행 경로

`p0::DailyGame::advance_day()` 는 `daily::World::advance_day(month_close_plan,native_day_plan)` 에 Phase G/H의 실제 `Stage` 명령을 전달합니다. C/D/E/G/H의 전원장 원자성 및 일별 POP 동태를 재사용하며 **P0 계산을 월말에 밀어두지 않습니다**.

- Stage0 국가 AI: 실제 가격·재고 부족/과잉 판단 → 합법적인 수입관세 변경 (기존 H 관세 원장)
- Stage1,4,5,8 민간투자: 투자 여력·노동·실물자재·시장가격 조건 → 시공/지급/WIP/다음날 준공 (기존 D/C)
- Stage2 직업에 맞는 실제 POP 노동자의 자율 채용
- Stage4 실제 투입재/설비/노동·가격을 기반으로 자율 제조; 급여는 Stage5 실물 금융에서 지급
- Stage7 POP 기반 유효수요와 가계 현금/공급 재고의 실제 거래
- Stage9 부족품으로부터 실제 POP 사회운동·시위, 설정한 선거일의 실인구 기반 지지 및 득표
- Stage10 실병력·실탄·장비/명시적 지형 계수로 계산한 양측 전투 피해, 실제 Phase B 사상자 반영
- Stage11 일간 가격 반응·식량 부족 기록, 즉시 일일 수급 계수 초기화하여 기존 월말 이중조정 예방

## 제한과 보존 기준

- 기존 MonthClose 호환 규칙과 G/H 월별 외국무역 자동 채결 및 일부 분야의 월말 처리는 아직 존재합니다. P0 경로는 **일간** C 현물시장·D 제조·건설·정치·전투·H 관세 정책의 한정적 통합입니다.
- 전국적 금융/건설·유전 데이터의 역사적 보정, 전투 지휘/이동/점령/평화회담 전체, 국별 분리된 선거 원장, 비선형 투표·정당별 상세 선호, 모든 다국간 무역의 하루별 완전 청산은 포함하지 않습니다.
- 일반 가정 수요 및 정부 AI/기업 우선순위는 `p0::Policies` 의 정적 시나리오/캠페인 데이터입니다. 이 설정은 기존 daily snapshot에 숨겨서 저장하지 않으므로 저장/재개 시 동일 설정을 재공급해야 합니다.
- **합성 P0 테스트**는 일간 생산·실제 결제·자율 투자 및 준공, 일간 식량부족·시위, 양측 실탄·실인구 전투와 저장복구 후 결정론적 재생을 검증합니다. 이는 게임 전체를 완성한 것이 아닙니다.
