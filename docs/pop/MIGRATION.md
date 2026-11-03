# 기존 POP v1.0 골격 → POP 설계 계약 1.1 이식 계획

상태: **이식 계획 확정 · 이식 미실행**

## 변경 범위

| 기존 include/vic25 | 새로운 목표 | 이식 지침 |
|---|---|---|
| Pop {id, PopKey, cohorts: Map<CohortKey,People>} | 별도 DemographicRegistry/SocioeconomicRegistry/HouseholdRegistry/PopulationSliceRegistry | 기존 인구 키를 3개 분류 및 하나의 인구 셀로 정규화 |
| PopKey.region | 세 분류의 residence_region_id | 기존 지역 기준 그대로 복제하되 동일지역 참조 검사 |
| PopKey.culture/religion/legal_status | DemographicCohortKey | 키 사전으로 이전 |
| PopKey.profession/livelihood | SocioeconomicPopKey | 유아·부양인구의 직업이 과거 잘못 묶였을 수 있음 → 검증/재배정 |
| PopKey.wealth_band | 재산 분포 초기 추정/데이터 출처 | 신규 POP 키에서 제거; 금융자산을 직접 생성하지 않음 |
| CohortKey.birth_month_index, sex | 인구 코호트 분류 | 원본 월 및 sex 출처를 보존; other_or_unspecified는 unknown으로 맵핑, 원본 정보 메타데이터 보존 |
| CohortKey.health, functional_limitations | HealthProfile | well/temporary_injury/long_term_impairment/illness를 기초 프로필로 이전; 병존 상태가 소실된 원본은 복원했다고 주장하지 않음 |
| CohortKey.military | service_status | prisoner_or_missing은 **legacy_ambiguous**로 보존; 근거 없이 포로 또는 실종 확정 금지 |
| EmploymentLedger (pop_id) | slice_id 및 직업적격 인원 참조 | 인구 총수 아닌 실제 노동 적격 인원에 계약 할당 |
| Financial/OwnershipLedger (pop_id owner) | 경제 POP/가구/자산 소유주 안정적 ID, 이벤트 연결 | 계정/지분 합계 불변; 단순 인원비례 분할 금지 |
| MonthlyPipeline hooks | read-only opening snapshot + staged journal + atomic commit | 기존 단계 이름/순서는 유지, 롤백/검증 추가 |

## 안전한 이식 절차

1. **태그/스냅샷 보존**: 현재 상태와 테스트 실행 결과를 기준선으로 보존. 원래 v0.2 엔진 파일을 확보하지 못했으므로 그것을 복원한 것으로 간주하지 않음.
2. **새 타입 병행 추가**: 기존 public header를 즉시 변경하지 않고 새 POP registry 및 slice API를 별도 네임스페이스/파일에 구현.
3. **한 방향 변환기 작성**: Pop.cohorts의 인구를 대상 PopulationSlice로 이전. 경제적 직업·가구 구성의 모호함은 명시적 unassigned/institutional 풀과 provenance로 표시.
4. **불변조건 검사**: 국가/지역/연령/성별/직업별 이식 전·후 인원 정확히 같음. 인구·자금·재고·지분 보존은 각각 검사.
5. **원장 참조 이전**: 고용·예금·지분의 모호한 배분은 데이터에 근거한 분포 정책/에스크로 계정 요구; 검증할 수 없는 정밀 이식은 수동 중단.
6. **기본 회귀 검사**: 현 테스트의 1,000명 전투손실, 예금 대출 100, 재고 보존, 고용 상한 조회를 새 계약 관점으로 재작성하고 기존 테스트 병행 실행.
7. **실행 엔진 접속**: 월간 staging을 완성한 후에만 새 인구 레지스트리를 실제 월별 엔진의 단일 원본으로 교체.
8. **구현 완료 시**: 오염된 이중 인구수 원본 API를 제거/읽기 전용화하고, 저장형식 마이그레이션 테스트/변경 내역/릴리즈 노트를 작성.

## 주요 격차/우회 금지

- 원래 POP 한 객체에 다양한 연령이 섞여 있으면 **같은 profession을 모든 어린이에게 부여하지 말 것**. 나이·노동 적격성을 기준으로 nonworker 경제 POP로 재배정.
- 레거시 Health enum은 상병 중복 여부를 기록하지 않았으므로 새로운 프로필로 옮겨도 누락 상태가 있을 수 있다. 추측으로 질병/장애를 추가하지 말 것.
- 포로/실종 혼합 상태는 정밀 분할에 필요한 자료가 없으면 ambiguous 범주/검증 표시로 남기고 국가 사망 통계에 더하지 말 것.
- 6개 초기 POP 데이터가 실제 역사적 인구라는 뜻은 아니다. 테스트용 데이터와 역사적 자료 출처를 분리한다.
- 인구/취업/경제/가구 4가지 집계를 동시에 합산해 전체 인구를 4배로 만드는 API는 허용하지 않는다.

## 이식 완료 판정

전량 인구 일치, 원장 참조 무결성, 재현성, 오류 시 롤백, 예시 6인구 fixture, 기존 테스트 통과를 만족한 실제 코드 커밋이 있어야 한다. 이 문서 작성 자체는 이식 완료가 아니다.
