# VIC2.5 — 1.15 전략 역량: 권위, 위신, 외교 영향력
**기준:** Victoria 3 1.15.0 게임 데이터 및 위키 · **작업:** `work/politics` · **일자:** 2026-11-09

## 0. 단일 원장 정책
- `Authority`와 `Influence`는 **동시에 유지하는 행위의 공급·사용·잔여량**이다. 현금·재고 같은 누적 계좌가 아니다. 기존 C 재정/POP 및 H 외교 조약 기록에서 파생 계산한다.
- `Prestige`는 국가 국력·지위에서 얻는 **비소비적 지표**이며 국가 순위/외교 영향력에 영향을 준다. 현금 잔액을 GDP로, 군인 수를 전투력으로 변환해 허위 위신을 생성하지 않는다.
- 모든 일간 정치 행동은 기존 C/D/E/G/H 단일 스테이징 트랜잭션으로 실행하며 새 정책의 경제·사회 효과는 **다음 날 Stage0 이후**부터 적용된다.
- 별도의 정치자원·외교자원 회계는 만들지 않는다. `P1Mandate`는 실제 시행 중인 법률상 **행동 기록**이고 `P1StandingEvidence`는 출처·관측일을 가진 **국력 원자료**다.

## 1. 권위 — 국가 통치 역량

**계산**
1. 국가 기본 권위 = **100**.
2. P2 시행 법률의 `country_authority_add` 누계, 실제 국가원수 인기도(출처·당사자 확인)가 확인되면 추가.
3. 법률의 `country_authority_mult` 및 실제 소속 인원과 승인도로 활성화된 이익집단의 해당 효과를 합산해 공급량 산정.
4. C-P1의 실제 활성화된 명령(`P1Mandate`) 비용 합계가 지속 사용량이며 잔여량 = 공급 - 사용.
5. 공급이 사용의 2배 이상일 때 입법 속도 보너스 최대 **+25%**; 사용이 공급의 2배 이상이면 야당 승인도 최대 **-10**, 정치운동 급진화 최대 **+20%**. 해당 효과는 현재 파생 조회이고 별도 P2 입법 확률/POP 실제 지위 변화가 아직 없는 부분은 도메인 미적용이다.

**실제 구현된 명령**
- `decree_road_maintenance`: 1개 지역당 권위 100을 지속 소모. 실제 P0/Phase D 유상 건설의 하루 처리 한도에 **+10%**를 적용하되 기존 자재·현금·고용·미완료 사업량 상한을 유지. 원본의 인프라 인구 기반 효과 +1/상한 +20은 조회 가능하지만 물리적 교통망에 직접 환산하지 않는다.
- `decree_promote_social_mobility`: 지역당 권위 100을 지속 소모, Phase B 실제 POP 교육 접근에 **+25%p**를 연결. 원본의 직업 자격 성장 +25% 효과는 조회 가능하지만 별도 직업 숙련도 성장 루프에서 완전 처리되지는 않는다.
- 두 정책은 `p1_schedule_mandate`로 Stage9에 합법적으로 요청하며 다음날 Stage0에 발효. `p1_schedule_mandate_termination`은 다음날 해제·권위 사용량 복원. 정책·취소 예약을 선택적 세이브 확장에 영구 저장한다.
- 이익집단 지원·억압(각 권위 200), 소비세(기본 권위 100) 등 다른 사용처는 1.15 **비용 출처를 확보**했지만 현재 P3의 실제 모집·소비세 집행과 아직 연결되지 않아 정책 발행을 허용하지 않는다. 수치만 차감하는 가짜 기능은 구현하지 않는다.

**조회:** `IntegratedWorld::p1_authority_capacity(country)`, `p1_authority_consequences(country)`, `p1_local_decree_ppm(country,region,modifier)`.

## 2. 위신 — 국가의 비소비적 국력 지표

**원본 1.15 기반 공식**
- 국가 등급(도시국가/공국/대공국/왕국/제국/패권국) = 0/5/10/15/25/50.
- 자국 GDP **100만당 위신 3**, 종속국 GDP **100만당 위신 1.5**.
- 실제 육군·해군 투사력, 주요 상품 생산 순위, 기념물, 임시 이벤트 위신을 합산한다.
- 마지막에 법률/이익집단의 `country_prestige_mult`를 적용한다.
- 외교 영향력 부족은 최대 **-50%**의 위신 불이익을 유발한다.

**데이터 완전성 계약**
- 기존 C에는 정확한 V3식 GDP, 함선 전력, 공업 생산 지도자 점수, 기념물 데이터가 전국 공통 단일 지표로 모두 존재하지 않는다.
- `P1StandingEvidence`에 위신 산정에 필요한 원시 자료와 관측일·출처 ID를 제공해야 한다. `complete=false`이면 `partial_total`은 확인된 점수의 일부로만 사용하고 최종 위신을 계산·선언하지 않는다.
- 기록된 원자료는 원본 경제·군사 원장의 소유권을 바꾸지 않는다. 별도 자본/POP/군사 자산 생성 금지.
- `confirmed_rank`가 입증된 경우 해당 1.15 순위를 사용한다. 모든 국가의 자료가 완전하고 세계가 100개국 이상일 때만 1.15의 국가별 상대 위신·평균 위신 임계값으로 순위를 자동 계산한다. 100개국 미만의 원본 게임 보정용 가상 국가를 임의로 만들지 않는다.
- 기존 인물이 바뀌면 인기도 보너스는 자동으로 무효가 된다. 시나리오 증거의 인기도와 현 국가원수 ID가 일치해야만 사용한다.

**조회:** `IntegratedWorld::p1_prestige(country)`, `p1_power_rank(country)`; 출처 데이터 갱신은 `p1_record_standing(evidence)`로 시나리오 시작 또는 합법적인 Stage9에만 허용한다.

## 3. 외교 영향력 — 조약 유지 역량

**공급량**
- 기본 100, 확인된 국가 순위에 따라 추가 0/500/600/750/1,000.
- P2 법률과 활성 이익집단의 `country_influence_add/mult`를 적용한다.
- 관측 가능한 국가 순위가 없으면 기본량과 확인된 법률 효과만 계산하고 `complete=false`로 명시한다.

**사용량**
- H가 실제 소유한 활성 보호국 관계는 1.15의 비용 10을 종주국에 산정.
- H가 의무 부담자를 명시한 조약 중 `no_tariffs`(10, 대상국 부담), `military_access`(25, 대상국 부담), `guarantee_independence`(50, 제공국 부담)는 대응하는 조약 조항에서 유지비를 파생한다.
- 조약 상대국의 **국가 순위 비용 배율**(예: 열강 ×2, 주요 강대국 ×1.5, 미승인 지역 강국 ×0.75, 미승인 소국 ×0.5)을 해당 조약에 적용한다. 상대국의 순위를 알 수 없으면 기본 비용만 잠정 표시하고 미산정 표시를 남긴다.
- 구형 양방향 일간 관세 조약 및 국제기구는 기존 데이터에 유지비 부담 방향/지도자 ID가 없을 수 있다. 이를 무상으로 가정하지 않고 `unpriced_commitments`를 증가시켜 계산 완전성을 해제한다.
- 실제 조약은 영향력 적자로 인해 즉시 삭제되지 않으며 공급을 초과한 사용을 허용한다(원본의 부족 불이익 모델).
- 영향력 잉여에 따른 악명 감소 최대 **+25%**, 영향력 부족에 따른 위신 불이익 최대 **-50%**. 현재 H에 완전한 악명 변화 원장이 연결되지 않아 악명 수치 자체를 임의 수정하지 않는다.
- `H::assess_influence(source,target)`는 기존 무역/군사/문화적 **양자 영향력 실측 조회**다. 국가가 사용하는 자원인 `H::diplomatic_capacity(country)`와 혼동하거나 하나의 가짜 외교 점수로 통합하지 않는다.

## 4. 관료제, 영토, 국력 보완 대상
- 관료제도 기본 공급 100을 가진 용량이며 실제 관료 건물/직원·기관 사용과 연결해야 한다.
- 현재 C의 `PublicInstitution`은 처리 슬롯·임금·직원·시설 상한을 제공할 뿐, 원본 V3의 정부청사별 관료제 생산 계수가 동일하게 모델링된 것은 아니다. **진료·행정 슬롯을 임의로 관료제 수치로 환산하지 않는다.**
- GDP·해군력·위신 상품 순위와 악명 원장이 단일 정본으로 연결되면 `P1StandingEvidence`의 관측 원시 자료를 그 정본 조회로 대체하고 불완전 표기를 제거해야 한다.
- 명령 비용과 조약 비용의 국가 순위/기술/이벤트·로비 조건부 배율도 일부 미구현이다. 직접 출처의 숫자가 있다고 해서 관련 전체 실행 경로가 완성되었다고 주장하지 않는다.

## 5. 원본 출처
- [빅토리아 3 법률 위키](https://vic3.paradoxwikis.com/wiki/Laws)
- [빅토리아 3 1.15 법률 추출](https://vic3tabula.com/en/laws)
- [1.15 `country_ranks/00_country_ranks.txt`](https://github.com/jink47787/Historical_MP_Series_Vic3/blob/main/basegame-ver/common/country_ranks/00_country_ranks.txt)
- [1.15 `static_modifiers/00_code_static_modifiers.txt`](https://github.com/jink47787/Historical_MP_Series_Vic3/blob/main/basegame-ver/common/static_modifiers/00_code_static_modifiers.txt)
- [1.15 `defines/00_defines.txt`](https://github.com/jink47787/Historical_MP_Series_Vic3/blob/main/basegame-ver/common/defines/00_defines.txt)
- [1.15 `decrees/00_decree.txt`](https://github.com/jink47787/Historical_MP_Series_Vic3/blob/main/basegame-ver/common/decrees/00_decree.txt)
- [1.15 조약 유지비 항목](https://github.com/jink47787/Historical_MP_Series_Vic3/tree/main/basegame-ver/common/treaty_articles)

## 6. 소스 위치·호환성
- C: `phase_c_p1_capacities.hpp`, `phase_c_p1_capacities_impl.hpp`, `phase_c.hpp`
- 일간 연결: `daily_time.hpp`, `p0_daily.hpp` (실제 건설·교육에 적용)
- H: `phase_gh_political_power.hpp`, `phase_gh.hpp` (조약·종주국 관계에서 유지비 산정)
- 저장: `phase_c_snapshot.hpp` 의 `VIC25_PHASEC_P1_STRATEGIC_CAPACITIES_1` 선택적 확장; 변경 자료 없는 구형 저장 포맷과 분리.
- 기존 C/D/E/G/H 정본·일별 Stage0~11 유지.
- **이번 작업은 사용자의 앞선 검사 생략 요청에 맞춰 테스트·CI 실행 없이 작성했다.** 코드 적용 검증과 역사적 모수 확인은 별도다.
