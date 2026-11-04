# I-S1 — 사회구조·불평등·사회이동 상세 설계

> phase-i-social/1.0 · SPECIFIED / NOT IMPLEMENTED · 상위: I-D0, POP-D0~D3, C-E/C-P, D-T

## S1-1. 역할과 금지사항
- 경제적 계층·불평등·빈곤·사회이동은 **기존 POP의 실제 분포, 원장, 사건에서 파생**한다. 독립적인 가변 계층점수, 빈곤포인트, 급진화 포인트를 만들지 않는다.
- 분석 대상인 계층(class position)은 직업(profession), 자산보유(ownership), 법적 신분(legal status), 고용상태(employment), 정치적 권리와 별개다. 지주이면서 공무원이거나 실업자인 자산 소유자도 가능하다.
- 국가 GDP·평균임금만으로 POP의 부·생활수준 분포를 복원하지 않는다. 원본 분포가 없는 역사 자료는 정확한 불평등 지표를 표시하지 않는다.
- C-P3의 정치적 태도·조직화·사회불안은 C-P가 소유한다. I-S1은 사회환경을 분석/노출하는 입력만 제공하며 동일한 원인을 정치에 2번 더하지 않는다.

## S1-2. 분포 데이터와 분석 단위
| 입력 | 소유자/규약 | I-S1 산출 |
|---|---|---|
| PopulationSlice.count, 거주·연령·학력·문화·법적 지위 | POP | 인원 가중 분모; 사망·이주 중복 집계 금지 |
| EmploymentLedger·실제 지급 임금 | C-E1/C-E3 | 노동 소득·고용 비율 |
| HouseholdPool 공동소비·실제 거래 | POP/C-E3 | 가구 규모와 필요를 반영한 생활수준/구매력 |
| Financial/Ownership 및 근거 있는 분포 분할 | C-E3 | 자산·부채·자본 소유 구간/격차 |
| 세금·이전 지급/공공서비스 | C-E4/I-E1 | 정책 변경 전후 가처분 여건 |
| 법적 지위·교육 적격성·정치적 권리 | C-P/I-E1 | 제도에 따른 기회 접근 차이 |

SocioeconomicGroupView: (scope_id, snapshot_id, month_range, cohort_filter, denominator_people, employment, actual_income_by_source, assets_debts_distribution, education_distribution, legal_rights, uncertainty, source_revision_hash). 읽기 전용 캐시.
SocialMobilityObservation: (cohort_lineage_refs, start_month, end_month, category_definition_version, transition_counts, censoring_flags, provenance). 개별 인구가 아닌 POP 집계의 관측 가능 범위를 추적한다.

## S1-3. 계산 규칙
1. **소득/자산/생활수준의 구분.** 총수입=실제 임금·사업소득·지급된 배당·이전소득. 부동산 등 자산의 원본은 소유권/재무 원장에서 조회한다. 원장 없는 가구의 평균 자산을 추정 확정값으로 만들지 않는다.
2. **상대적 경제 위치.** 분석 범위/연도·명목/실질 가격 기준을 명시한 인원가중 분위, 중앙값, 상·하위 비율과 빈곤 통계로 조회한다. 비교 가능한 가격·생계조건이 없는 두 시장을 무조건 동일 소득 숫자로 정렬하지 않는다.
3. **불평등 지표.** 실제 개인/가구 분포가 충분하면 인구가중 로렌츠/지니 등을 계산. 셀 내부에 잔고 분포가 없으면 평균값만으로 가짜 '정밀 지니'를 내지 않고 범위/결측/근사 방식 표시. 서로 다른 가구원수는 필요에 따라 등가 가구 규모 기준 사용.
4. **사회이동.** 시점 t→t+n의 교육·직업·소득 분위 전이를 PopulationEvent lineage 및 실제 고용/자산 변화로 추적. 출생·사망·국제 이주에 따른 관측집단 변경과 동일 개인의 지위 변경을 구별한다. 수명 전체 부모-자식 고정 연결이 없으므로 '부모 소득 대비 자녀 이동'은 미지원 또는 통계적 추정으로 별도 표시한다.
5. **차별·제도.** 법률상의 고용/교육/거주 차별과 실제 집행을 고용 적격성, 학교 접근/직업 기회에 입력. 계층이나 문화에 선천적 능력/의지 패널티 금지.
6. **장기 효과.** 산업화→직업·임금 변화→가처분 소득→교육/이동/정치적 반응은 기존 각 단계의 실제 원장 변화를 경유. '산업화량+사회이동률' 직결 보정과 C-P 사회불안 중복 적용 금지.

## S1-4. 월별 연산·성능
- stage0: 전월 커밋 기준 계산에 필요한 분포/revision 고정. stage4~8: C가 실제 지급·생산·소비·고용 실적 기록. stage10: I-E1·POP의 교육/직업/지역 전이 반영. stage11: 월말 확정 이후 지표 캐시 갱신/무효화.
- 정치 반응은 C-P의 유효 단계에 **실제 원본 분포**를 제공하고, stage10 신규 변화를 과거 C-P 평가에 소급 반영하지 않는다.
- 캐시 키는 (scope, snapshot, period, population_filters, income_basis, deflator/version, source_revisions). 실제 변화한 셀/지역만 증분 갱신하되 분포 재정렬/분모 변경 시 관련 집계 재계산.
- 최대 100만 희소 POP 셀에서 집계 시간/메모리 실측. 분포 원본 없는 POP 전체 세분화로 오히려 셀 수를 폭증시키지 않는다.

## S1-5. 공개 API
- GetStratification(scope, period, basis, snapshot) → 층위별 실제 사람 수·근거.
- GetInequality(scope, period, metric, basis, snapshot) → value_or_interval, denominator, precision, source_refs.
- GetMobility(scope, t0, t1, cohort_filter, snapshot) → 전이 행렬/유입·이탈·검열 사유.
- ExplainSocialDifference(group_a, group_b, snapshot) → 고용·임금·자산·법률·교육·가격 수준에 관한 관찰된 요인. 원인을 통계적 상관으로만 관측한 경우 인과라고 확정하지 않음.

## S1-6. 필수 검증 (I-S1-T01~T10)
T01 6인 POP의 자산·임금·교육 인원 가중합; T02 부유한 실업자와 임금근로자의 직업/재산 분리; T03 직업변경 시 인구 불변; T04 소득 증가·물가 증가에 따른 실질 생활수준 차이; T05 가구 합산의 중복 소득 금지; T06 POP 셀 분할/합병 후 분위 집계 동치; T07 분포 모호성 시 가짜 정밀 지니 거부; T08 이주/사망을 사회이동자로 이중 집계 금지; T09 정치 급진화 이중계수 금지; T10 저장·복구/결정론/100만 셀 성능 계측.

**수락 경계:** 알고리즘·API/반례 규약을 확정했다. 실행 결과 PASS는 아직 아니다.
