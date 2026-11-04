# J-P1 — 오염·정화·환경 외부효과 상세 설계

> phase-j-pollution/1.0 · SPECIFIED / NOT IMPLEMENTED · 상위: J-D0/J-N1, C-E/C-P, D-I2, E-D0

## P1-1. 오염은 실제 활동과 물리적 근거에서 발생
가변 국가 환경오염 포인트나 정책 보너스로 만들지 않는다. 구체적인 산업/가구/교통/광업 활동에서 발생한 대기·수계·토양 오염물질이 지역에 축적·이동·감쇠되는 경로를 추적하고 실제 노출/토양 피해를 후속 분야에 전달한다. 모든 산업별 배출량은 실제 ProductionEvent/TransportEvent/DisposalEvent의 부산물·환경계수에 연결한다.

## P1-2. 상태와 기록
| 객체 | 필드 | 원칙 |
|---|---|---|
| PollutantDefinition | pollutant_kind, medium(air/water/soil), emitted_unit, persistence/decay_model, toxicity/soil_response_rules, numeric_policy_version | 오염물 종류·단위·변환계수 명시 |
| PollutionCell | province_id or hydrologic_cell, pollutant_kind, environment_medium, load_or_concentration, mixing_volume_basis, exposure_proxy, revision | 오염 질량과 농도를 혼용하지 않음; 가능하면 질량 기반 |
| EnvironmentalImpactEvent | id, source_production_event, source_region, pollutant_kind, mass, unit, released_month, medium, source_factors, provenance | 실제 방출 이력; 동일 생산이벤트의 중복 등록 금지 |
| PollutionTransferEvent | origin, destination, pollutant, amount, period, transport_rule, source_event_ids | E 인접/하천 연결·기상 제약 사용, 질량 균형 |
| RemediationEvent | authorized_operator, site/region, actual_labor/material/energy/cash_refs, pollutant_removed, safe_disposal_refs, effective_month | 실제 작업이 없는 무상 정화 금지 |

수질 오염은 지형 인접만으로 강물 흐름을 가정하지 않는다. E의 하천 방향/해양 연결이 실제 있는 지역에서만 방향성 확산/월간 이동을 적용한다. 기체 확산은 대기·풍향·시기 근사 계수를 사용하되 고비용 유체역학을 구현하지 않는다.

## P1-3. 월별 질량 균형·노출
- 한 오염물/단위/환경 매질에 대해 월말 load = 월초 load + 실제 배출 + 타 지역 유입 − 외부 유출 − 검증된 자연 감쇠/분해 − 실제 정화/안전 폐기. 모든 항은 정수/고정소수점·단위 환산 및 overflow 검증을 거친다.
- 단위 변환이 없는 대기 CO2 톤, 수중 부유물 mg/L, 토양 중금속 kg을 하나의 오염 점수로 직접 더하지 않는다. 표시용 종합 위험 등급은 독립 원본이 아닌 사유 있는 파생 지표.
- 피해는 지역 내 노출 인구/농업용수/작물/생활환경이 실제 존재할 때 POP 건강/생산성·산업시설 제약에 전달한다. 문화/직업 집단별 차이가 있더라도 실제 직업 노출·거주지·법적 보호·접근 서비스에 근거한다.
- '오염 발생→같은 달 전국 사망 N명'을 I/J가 직접 기록하지 않는다. 보건/농업 시스템의 위험·손실 사건과 POP-D2의 인구 보존/출생·사망 규약을 따른다.
- 환경세/과태료/배출권/복구책임은 C-P 법적 의무 및 C Fiscal/Financial 실제 분개를 통해 집행한다. 배출행위가 없거나 벌금 집행 불가능하면 국고에 돈 자동 생성 금지.

## P1-4. 단계·API·설명
- stage0: 환경부하·확산 인접/규제·효력 확정, 사업장 규제 적격성의 고정 기준선.
- stage1~3: 실제 허용 배출 조건/정화사업 계획·비용/근로자/설비/재료. stage4: 생산/폐기/정화의 확정 사건과 배출량 생성. stage5~8: 실제 환경세·보상/정화비용 지급과 거래 청산.
- stage9: 신규 규제/복구투자 등 차월 예약. stage10: 오염부하 전이/감쇠·정화 확정, 후속 POP 건강·토양/수문 위험 기록. stage11: 중복 배출/질량 수지·재정/보건/원장 정합.
- API: GetPollutionLoad(province, pollutant, month, snapshot), RecordEmission(source_event, quantity, units, stage4), SimulateTransport(candidate_cells, snapshot), CommitRemediation(realized_resources, stage10), ExplainExposureHealthImpact(scope, period, snapshot). 읽기/쓰기 책임 분리.

## P1-5. 연산 성능·정밀도
프로빈스×오염물 완전 밀집행렬을 피하고 실제 배출/축적 지역의 희소 오염셀만 저장. 오염 확산은 변화한 셀과 실제 인접 간선을 우선 평가하고 일정 안전 범위 이하 값은 명시적 물질 처리(침적/분해) 없는 조용한 삭제 금지. 대량 자료 정확도가 낮으면 시계열/근사임을 공개.

## P1-6. 필수 시험 J-P1-T01~T10
T01 산업이 가동하지 않으면 해당 생산 배출 없음; T02 매체/단위별 질량 수지; T03 상류→하류 이동과 무권한 역류 거부; T04 동일 ProductionEvent 이중 배출 금지; T05 정화에 돈/노동/재료 실제 사용; T06 법·과태료 선행효력/재무 일치; T07 오염 노출 없는 POP 건강 일괄 감소 금지; T08 Stage10 환경 피해로 과거 생산 소급수정 금지; T09 다중 지역 확산 worker/seed 재현; T10 저장/복구·희소 30k Province 부하.

**상태:** 상세 설계 확정, 오염 물리상태/건강 통합 실행 엔진 미구현.
