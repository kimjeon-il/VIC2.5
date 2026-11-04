# H-V5 — 외교 전 분야 교차 인수시험 72개

> phase-h-integration/1.0 · 2026-11-04 · **SPECIFIED / NOT_RUN**
> 신규 고유 ID HV-T01~HV-T72 (72개). 기존 H-D0-T01~T20과 H-D1~D7 각 12개(84개)는 별도 정본이며 중복 PASS로 집계 금지. **외교 정의 ID 합계 176개** (20+84+72).
> 대상: 실물 경제·정치·군사·영토·POP·무역/조약의 실제 엔진 통합 및 결정론/원장·시간 회귀.

## 0. 기준 및 공통 오라클

- 각 시험은 한 달/여러 달의 동일 world/seed/ruleset과 권한·원장·지리/조약·조직·위기 상태를 가진 fixtures를 준비하고, 명령/사건 목록을 실행한 뒤 실제 C-D0 WorldSnapshot 및 서브원장·조약 이벤트를 검사한다.
- 참조만 있는 조회/Mock으로 성공한 것은 실제 통합 인수가 아니다. E/F/G/C/D/POP 등이 미구현이면 해당 시험은 BLOCKED/Unsupported와 선행조건을 명시.
- Source evidence = 법적 권한/당사자, effective month, 관련 H Term/Organization/Conflict/Crisis, 실제 Money/Inventory/POP/Province/Fiscal/Trade·ServiceAssignment 상태. **예상효과·파생 리스크/영향력은 원본 상태 변화의 근거가 아니다**.
- 정상적인 Reject/협상 실패·전쟁 없이 종료·금융부도도 기대되는 오라클이면 PASS일 수 있다. 음수 물자·가짜 외교 보너스·중복 관세/조약·소급 변경/전역 부분 commit은 결함.
- 실패 시 month/commit_id/ID allocator·C/D/E/G/H 원장 및 제3국 정보뷰/캐시 hash를 precommit 상태와 대조. Worker1/N, save/replay 규약은 H-V6.

## 1. HV-A — 외교권한·조약·시간·국가와 정부(01~12)

| 시험 ID | 교차 시나리오 | 필수 실측 오라클 | 연결 분야 |
|---|---|---|---|
| HV-T01 | C-P 정부교체 후 기존 H 조약 발효/배상·C-E3 채무 | CountryId/TreatyTerm/FinancialDebt 불변, 정부·외교권한만 새 Government으로 적용; 중복 지급 0 | C-P,H-D1/D3,C-E3 |
| HV-T02 | m stage9 다자조약 서명·일부 비준 + m 무역·전투 | m 기존 관세/TradeFill/Conflict 그대로, required ratifications 충족 m+1에만 조항 발효 | H-D2/D3,C-P,G,C-M |
| HV-T03 | 관세면제 조약이 상품1에만 발효, 상품2에는 미발효 | G PolicyProjection 한 번; C-E5 상품1 관세0·상품2 기존세율, 정부 세수 실제 지급 원장과 일치 | H-D3,G-T4,C-E4/E5 |
| HV-T04 | 동맹 조항 유효하지만 의회가 C-P 참전 거부 | H obligation/위반 또는 적법 면책 기록, C-M5 EntryToWar 없음·실제 병력0 추가 | H-D3,C-P,C-M5 |
| HV-T05 | 적법 중재안 중 비밀 단서 전달 지연 | 제3국의 H-D1 정보뷰는 비밀/미수신 정보 없음; 해당 국가 AI는 공개정보만 근거 | H-D1/D2,C-P,AI |
| HV-T06 | 외교사절 2명·급여20, 조직/해외업무가 동일 인원 재요구 | Employment/POP 실효 2명 이하, 실제 급여/기관 비용 20 이하 지급가능액 조건, 중복 노동 0 | H-D1/D2,H-D5,C-E1/E3,POP |
| HV-T07 | 상충 보호국 treaty_capacity와 신규 타국 조약 체결 | H-D4 위임/ C-P 권한으로 Reject, E 영토/ G 통관/금융 경로에 조약 신규효과 0 | H-D3/D4,C-P,E,G |
| HV-T08 | 조약 Term DAG에 사이클 + G TradePolicyProjection 요청 | 법적 조항 Reject, 기존 TradeAgreement/C-E5 원본·기존 통관 원장/시장 변화0 | H-D3,G,C-E5 |
| HV-T09 | 부채 100 보상조약 갱신/종결 시 실제 20 지급 | 발효·만료 범위/잔존 의무 적법, C-E3 실제20/미지급80, 과거 지출 그대로 | H-D3,C-P,C-E3/E4 |
| HV-T10 | 동일 조항을 H 통상·C-E5 TradeAgreement가 함께 참조 | term×customs×shipment unique; 관세채무·지급 한 번, 신규 H 화폐/세금 원장 0 | H-D3,G-T4,C-E5 |
| HV-T11 | 당월 비준과 다음달 왕위승계·국가법인격 연속 | C-P CountryId/국채·조약의 legal participation 보존, 새 권한자 stage0 이후만 집행 | H-D1/D3,C-P,C-E3 |
| HV-T12 | 부도/주권 조건부 조약 패키지가 stage11 원장 유효성 실패 | H/C-P/Financial/E/G 전원장·ID/월/이벤트 해시 rollback, 비준/조건충족만 부분 commit 불가 | H-D3,C-D0,C-P,C-E3,E,G |

## 2. HV-B — 국제협력체·무역·기금·방위·기술(13~24)

| 시험 ID | 교차 시나리오 | 필수 실측 오라클 | 연결 분야 |
|---|---|---|---|
| HV-T13 | A/B/C 공동시장 협력체 발효와 E 다른 주권·G 관세 | 유효 MarketAssignment·CustomsTerritory 조항만 갱신, 세 나라 주권/국고/국적 자동 통합 0 | H-D5,D3,G,C-P,E |
| HV-T14 | 공동기금 3국 각100 약정·각40 송금(실제 조직계좌 존재) | Fiscal 청구/Financial 현금 총120·미수180, 회원권과 금액원장 단일; HV-X2 | H-D5,C-E3/E4,C-P |
| HV-T15 | 기구에 다른 군사동맹 중복가입, 서로 다른 상업회원 의무 | 헌장상 배타조항 없으면 복수가입 합법, 조약상 관세 충돌만 부분 거부 | H-D5,D3,G,C-P |
| HV-T16 | 경제협력체 총회 방위제안 정족수 미충족 | 유효 OrgDecision 없음; C-M5 부대/군수/참전·정부 재정 0 추가 | H-D5,C-P,C-M5,C-E4 |
| HV-T17 | 공동군사기금 현금0인데 군수품10 조달 시도 | Financial/Inventory/Employment reservation 거부, 무상 군수생산/조직세금 원장 0 | H-D5,C-M,C-E3/E4,D,G |
| HV-T18 | 공동연구 협정 발효, 연구원/설비·특허 조건 일부 미충족 | D KnowledgeState/Technique adoption 무근거 활성화 0, 실제 직원/시설·비용만 진행 | H-D5,D-T,D-C,POP,C-E3 |
| HV-T19 | 조직 회비 미납 회원 의결권 정지 헌장 조항 | C-E3 채무 미수 상태와 H-D5 voting rights 연결, 적법 자격정지 전에는 표 자동 제거 금지 | H-D5,C-P,C-E3 |
| HV-T20 | 공동외교성명 후 회원국 B가 영토위기에서 중립 | B C-M5 참전 0·H7 중립유지, 조직 경제협력·상업유효 계약 계속 | H-D5,D7,C-M,G |
| HV-T21 | 회원국 탈퇴 뒤 금융채권·항만 공동시설 소유권 | H membership 종료, C-E3/Ownership/F/기구 채무·시설은 잔존 근거대로 정리, 자동 소멸0 | H-D5,C-E3,Ownership,D-C,E |
| HV-T22 | 조직 의장국 A→B 변경 뒤 E 주권·민간기업 장부 보존 | OrgId/MemberId·시설/구계약 동일, 단지 legal leadership 변경, 영향력 파생 별도 | H-D5,D6,C-P,E,C-E1 |
| HV-T23 | 대외관세 공동조항과 개별 국가 수입쿼터 충돌 | C-P/H 법적 우선·G-T4 적용사유/상품 범위 산출, 불명은 Reject; 중복 통관/상품 공급 없음 | H-D3/D5,C-P,G,C-E5 |
| HV-T24 | 조직 해산/분열 + 기존 재정채권·군사기지·시장 통과권 | 조직계보/기존 조약의 잔존 의무·인수 후 실제 Financial/Ownership/Military/G 사건, 모든 자원 보존 | H-D5,D3,C-E3,C-M,G,E |

## 3. HV-C — 영향력·금융·공급망·POP/정치의 연쇄(25~36)

| 시험 ID | 교차 시나리오 | 필수 실측 오라클 | 연결 분야 |
|---|---|---|---|
| HV-T25 | B 철 필요100, A산60, 다른 기존공급40, 대체추가35 → A 금수 | 월 m+1 부족25·가용75(명시된 실제 이행), H6 influence read-only, 자동 승낙/전쟁0; HV-X3 | H-D6,G,D-I,C-E2,H-D7 |
| HV-T26 | A/B 비대칭 투자·무역 원장과 H-D1 위협 인식 | A→B 경제 노출과 B→A 별도, 신뢰/위협·주권/조직회원 복제·동일점수 강제 금지 | H-D1/D6,C-E3,G,E |
| HV-T27 | A 해외광산 소유·B 차입/물류와 동일 underlying 채권 | Ownership/Financial/Trade exposures 중복 합산하지 않아 차단 예상 손실을 두 번 계상하지 않음 | H-D6,C-E3,Ownership,G,D-I |
| HV-T28 | A 군사원조 약속100 vs 실제 물품납품30 | H 조약/약정100 별도, Inventory Shipment 완료30만 B 군수/안보 노출의 실효 공급으로 반영 | H-D3/D6,C-M,G,C-E5 |
| HV-T29 | B가 A 철강 수입 절반 줄이고 C 대체수입/시설투자 | 실제 G 거래/산업 재고·자산 반영 후 H6 다음 snapshot 평가, 영향력 감소/증가는 결과 조회뿐 | H-D6,G,D-I,D-C,C-E3 |
| HV-T30 | 정치운동 POP 200명 지지와 실제 외교 시위 참가50명 | POP/Employment/정치 C-P5의 참가50, 외교관계/신뢰 파생; 200명 무료 군사/외교포인트 생성0 | H-D1/D6,C-P5,POP,C-E1 |
| HV-T31 | A의 경제압박으로 B 수입가격 상승→B 재정/정치적 저항 | C-E2 가격 m+1과 POP 소비/정치 반응을 실제 원장·법으로 산출; 동일 피해 중복 비용/즉시 폭동 없음 | H-D6,G,C-E2,C-P5,POP |
| HV-T32 | 세력권/영향력 지도 겹침 A 경제/C 군사/D 문화 | 분야별 지도 파생·H5 조직회원 관계 유지, E 주권/법적 보호국/ForeignOffice 자동 변경0 | H-D6,D5,E,C-P |
| HV-T33 | H6 정보캐시 stale, 은행부도·외국인 투자권 변경 | source revisions mismatch → 캐시 invalidate/recompute; 과거 stage1 협상 재실행/예금 변경0 | H-D6,C-E3,C-P,H-D2 |
| HV-T34 | 신용경색으로 A 보증제안→B 거절·C 중재 | H2 실제 장기대출/대체 금융·정치비용 계산, H6 영향력 높아도 자동 수락/금전 발생 없음 | H-D2/D6,C-E3,C-P |
| HV-T35 | 비밀 대외차관·정식 국제협력체 회원권이 서로 정보 수준 다름 | 관측 가능한 인지/실효 사실·상대 국가 AI belief 구별, H6 공개 UI 추정/확정 혼동 금지 | H-D1/D5/D6,C-E3,AI |
| HV-T36 | 월 100 Stage7 실제 투자/수입충격, Stage9 조약 협상 | 동결된 Stage0 근거로 당월 협상 평가, m+1 영향력 갱신; H6→G→H 동월 무한 루프 없음 | H-D2/D6,G,C-E3,C-D0 |

## 4. HV-D — 국제위기·실제 전쟁·평화/경제·군사(37~48)

| 시험 ID | 교차 시나리오 | 필수 실측 오라클 | 연결 분야 |
|---|---|---|---|
| HV-T37 | A/B Province P 영유권 주장→C 소국 중재 | E Claim/H2 교섭 근거로 H7 위기 생성, C는 중립 가능, B 주권·POP/금융 그대로; HV-X4 | H-D7,H-D2,E,C-P |
| HV-T38 | 동맹 X 회원 A/B가 서로 영유권 주장·공동 회의 | 경제회원권 유지·회원국 자동동원0, 실제 H7 참가/의결·C-M5 참전 별도 | H-D5/D7,C-P,C-M,G |
| HV-T39 | 위기 긴장도 최고범위·당사국 최후통첩 만료 | C-P/C-M5 war order 없으면 Conflict/군수/사상자 0, 경제제재는 유효 C-P/G 조건만 적용 | H-D2/D7,C-P,C-M,G |
| HV-T40 | B 방위조약 발동·D의 실제 합법 참전 및 POP 피해 | 군인100 중 전사12·부상8 원본 전이, D 실제 EntryToWar 조건/장비20 중 10 송품; HV-X5 | H-D3/D7,C-M5,POP,C-E3,E |
| HV-T41 | D의 병력 철수 약속, 실제 운송경로 봉쇄/병참 부족 | 의무/위반·철수 미이행 별도, C-M5 병력 이동0 또는 적법 부분 이동, F/E 실제 제약 | H-D3/D7,C-M5,E,C-E5 |
| HV-T42 | 전시 해협 봉쇄가 수입·제철·외채연체에 파급 | 실제 C-M/E 통제→G/F 선적지연→D-I 원료·C-E3 연체, 원장 사건에 한 번씩만 반영 | H-D7,C-M,G,D-I,C-E3 |
| HV-T43 | 군사점령 B→A Province P, 전쟁은 아직 계속 | E military_control=B/sovereignty=A, C-P 시민권·기업 소유권 그대로, C-M5 occupation 비용 실재 | H-D7,C-M5,E,C-P,C-E3 |
| HV-T44 | 평화협상에서 배상100 중 20 납부 + P 공동관리 | 미지급80·E JointAuthority적법·C-M5 참전종료 별도, 전쟁 없었다면 가짜 PeaceTreaty 금지; HV-X4 | H-D2/D3/D7,C-M5,C-E3,E |
| HV-T45 | 위기 일부 쟁점 해결/일부 동결 + 보증의무 잔존 | H CrisisIssue 일부 settled, Claim/Dispute·TreatyObligation ledger 정합; 동일 배상/영토 재처리0 | H-D7/D3,E,C-M,C-E3 |
| HV-T46 | 독립운동의 국제화와 외국 무기 지원 | C-P5 실제 조직/POP·H7 참가·C-M5 재고/군사할당, 독립군·소유권/사상자 무료 창출0 | H-D7,C-P5,POP,C-M,E |
| HV-T47 | 휴전 위반/재개된 적대행위, 미인도 원조 화물 | C-M5 ceasefire/Conflict 실제 원본, H 이행 위반/위기상태, G Shipment 지속/손실 별도 | H-D3/D7,C-M5,G,C-E5 |
| HV-T48 | 국제기구 중재 및 조약 수정이 전투 Stage10 뒤 결정된 경우 | 새 H resolution은 다음 유효월, 이미 발생한 POP/군수 피해 소급취소0; 결정을 복제한 2번째 전쟁 없음 | H-D5/D7,C-M5,POP,C-D0 |

## 5. HV-E — 주권·종속·국가승계·행정·지리(49~60)

| 시험 ID | 교차 시나리오 | 필수 실측 오라클 | 연결 분야 |
|---|---|---|---|
| HV-T49 | 보호국 B의 관세권 A 위임, 실제 시장·세금징수 | H-D4 rights→G/C-E5 유효관세·E fiscal권한/소유주체 일치, B 국가/국적 유지 | H-D4,G,C-E4,E,C-P |
| HV-T50 | 보호관계 종료 후 B의 A 차관·공장 투자 지속 | H 종속권한 종료, C-E3 금융/Ownership/D Site 계속, H6 경제영향력은 실효관계로만 평가 | H-D4/D6,C-E3,Ownership,D-I |
| HV-T51 | 주권 공동관리 P에 2국 법적 권한, 군사 통제 단독 | E JointAuthority의 합법 공동권한 + 다른 military_control 역할 유지, 세금/군정 이중과세 금지 | H-D3/D4,E,C-P,C-E4 |
| HV-T52 | 다국가 종속권한 이중 위임·무효 제3국 조약 | C-P/H-D4 authority cycle/exclusivity Reject, H-D3/ G 정책·E sovereign/Customs 변화0 | H-D4/D3,C-P,G,E |
| HV-T53 | 분리독립으로 Country B 생성, Province P 주권 변경 | E ProvinceId geometry 불변, C-P Country lineage/POP headcount·재산/고용/부채 전체 보존 | H-D4,C-P,E,POP,C-E3 |
| HV-T54 | 국가 합병 후 옛 조약·외채/군수계약 승계 | H-D3 state succession law + Financial/Ownership/C-M5 obligation lineage, 채무/예금/군인 자동 삭제0 | H-D3/D4,C-P,C-E3,C-M,E |
| HV-T55 | E 프로빈스 행정구역 재편·외국 영사관 jurisdiction | 공관의 외교권/시설 위치와 E admin assignment 다른 ID, 해당 군사·외교 관할 유효시점 갱신 | H-D1,E,C-P,C-E1 |
| HV-T56 | 같은 P 영토에 두 위기·두 배상·상이한 조항 | 유효 상충 sovereignty 변경 Reject, 별개 합법 채권은 중복/다른 ID 구분, 영토 단일 정본 | H-D7/D3,E,C-E3 |
| HV-T57 | 보호국 B에게 자치권 회복 조약, 동시에 총독/공무원 변경 | m+1 rights/AuthorityGrant·E civil/POP 고용 변경은 별도 사건, 정부 급여/국세 이중 지급 금지 | H-D4,C-P,E,C-E1/E4,POP |
| HV-T58 | 반란 시 군사 점령/민정·세금/외교적 인식 분리 | C-P5 Rebellion/C-M 군사 통제/E sovereignty/H recognition/ POP actual distinct, 가짜 독립국 없음 | H-D1/D7,C-P5,C-M,E,POP |
| HV-T59 | 통행권 있는 내륙 보호국과 해상관세 동맹 | H 보호/조약 권리 + G/C-E5 적법 수송·CustomsTerritory, 영토·시장/관세구역 동일시 금지 | H-D3/D4/G,C-E5,E,F |
| HV-T60 | 전시 P 군사점령 + 종전 m stage9 양도조약 예약 | m stage10 military_control 유효, m+1 lawful sovereignty, F/G shipping/POP 시민권 과거/미래 이력 일치 | H-D3/D7,C-M5,E,G,POP |

## 6. HV-F — 원자성·정보·결정론·세이브·성능·품질(61~72)

| 시험 ID | 교차 시나리오 | 필수 실측 오라클 | 연결 분야 |
|---|---|---|---|
| HV-T61 | A/B 복합조약 100 즉시지급·P 양도, B 지불가능60 | 필수 동시 이행 Reject, A/B 현금·P 주권변동0, 모든 reservations 해제; HV-X6 | H-D3,C-E3,E,C-D0 |
| HV-T62 | Stage11 고아 Province/의무 참조 오류 주입 | H/Treaty/Org/Crisis·C/POP/D/E/G 원장·ID·월/commit_hash 전부 원상복구; HV-X6 | H-D3/D4,D5/D7,C-D0,E |
| HV-T63 | 중복 조약/지원·국제기구 의결의 메시지 replay | 동일 source event/term/obligation/transaction 2회 입력에도 해당 실제 영향1회, 저널 ID/원장 이중 생성0 | H-D2/D3/D5,C-E3,C-M,G |
| HV-T64 | 월 100 외교/교전 경합, worker1 vs worker4 | 유효 authority·event 순서/국가/군/무역/영토 및 파생 영향력해시 동일 | H,C-P,C-M,E,G,D |
| HV-T65 | 전체 H 상태 세이브→중단→재로드·이후 50월 replay | Treaty/Term effective history, Org membership/votes, Crisis/Influence cache revisions, C ledger 동등 hashes | H,C-D0,POP,C-M,E,G |
| HV-T66 | 외교 contract version 또는 E Province dataset hash 변경 세이브 | 호환된 migration만 성공, 잘못된 버전/고아 ref는 조용한 default 없이 reject/증거 보고 | H,E,C-D0,Persistence |
| HV-T67 | 정보 누설 테스트: 비밀 군사조약으로 AI 중재가 변화? | 정보 미수신 AI 정책은 비밀조건 영향0, 개시된 유효 계약 자체는 보호; 플레이어 UI unknown/assumption 표기 | H-D1/D2/D3,AI,UI |
| HV-T68 | 같은 관세/외교위기 사전 예상툴팁·사후 실제 | forecast/legal immediate vs actual/unknown distinction, C/G Financial/POP price/crisis cause refs와 설명 일치 | H-D6/D7,G,C-E3,POP,UI |
| HV-T69 | 서브틱 미지원 1일 긴급전쟁·외교 즉시효력 요구 | C-D0/C-M5 지원 없으면 Unsupported, 현재월 stage3/7/10/상위 정본 변경0 | H-D2/D7,C-M5,C-D0 |
| HV-T70 | F 물류 미구현·국제기구 독립 FinancialOwner 불가 | 지원안되는 외교원조/공동기금·실물수송 케이스는 BLOCKED, fake stock/money/transport pass 금지 | H-D3/D5,G,F,C-E3 |
| HV-T71 | 1,200개월·다국가/조약/조직/위기·경제/POP 통합 스트레스 | 매월 36 불변식/원장·메모리 성장/CPU P50/P95/P99/사건수 계측, 성능 예산 사전 확정 | H,C,D,E,G,POP |
| HV-T72 | 104 기존 외교 + 71개 교차 케이스 실제 evidence/회귀·마지막 72번째 최종 게이트 | HV-T01..71, 기존 HD0/HD1..7 104, 필수 POP/C-D4/D-V/E-V/G 동등 코드 기준에서 PASS일 때만 HV-COMPLETE | H,CI,Release |

## 7. 72개 시험의 독립 실행 요건

1. HV-T01~T71은 C++ 하니스의 **별도 등록·독립 assert 및 증거**를 요구하며, 한 제한적 스모크 시험을 통과한 것으로 71개를 자동 PASS 처리하지 않는다. HV-T72는 나머지 71개와 상위 104개·필수 회귀/릴리스 증거를 확인하는 집계 **메타게이트**로, 다른 번호를 대리 실행해서는 안 된다.
2. 각 시험은 (a) 정상 성공, (b) 실패·부분이행·거부가 예상되는 경우, (c) 관련 외교/원장 시간계약, (d) 관찰/예측·정보/비밀 범위, (e) 가능한 다른 국가의 역반응/장기 인과를 정량/상태 오라클로 검증한다.
3. 숫자 fixture가 하나의 시험 케이스와 중복되더라도 테스트 ID당 **명시적 엔진 실행/기대값 assert**가 존재해야 한다. H-D0/D1~D7 기존 시험 PASS 기록을 HV 교차시험 PASS로 재사용할 수 없다.
4. HV-T61~72에 소속된 장애·성능/세이브·릴리스 항목은 월간 상태뿐 아니라 파일 체크섬/contract versions, crash/replay/worker/event journal 및 출력 증거를 검사한다.
5. 예측·영향력 평가의 숫자를 외교 정책의 확정 성공확률로 하드코딩하지 않는다. 유효 정보/데이터 기준 시점에 부합하는 전망/불확실성과 실현된 경제·군사 결과의 차이를 설명할 수 있어야 한다.

## 8. 매핑·상위 회귀

- 기준 104 외교 시험: [H-D0 + H-D1~D7 인수명세](../DIPLOMACY_ACCEPTANCE_TESTS.md).
- 신규 36 불변식: [H-V3](H-V3_INVARIANTS.md). 각 시험은 공통 원장/법적 정합성을 검사하며, 아래 H-V6의 재생·성능 게이트와 결합한다.
- 핵심 시나리오 6개: [H-V4](H-V4_SCENARIOS_ORACLES.md); fixture HV-X1~6의 수치 오라클을 해당 여러 독립시험의 cross-domain 근거로 사용.
- 필수 상위 회귀: POP/고용, Phase C-D4 48(독립 코드 기준), Phase D-V 기존 정의 범위와 실제 실행 상태, Phase E-V 60(현 13 BLOCKED), G-D0/G-T1~5 62(현재 NOT_RUN) 및 F/C-E5 물류 어댑터. 상위 각 영역이 실제 지원/통합되었는지를 H-V7에서 증명해야만 통합 인수 가능.
- **상위 회귀의 모든 번호를 외교 총계 176에 합산하지 않는다.** 176은 외교 자체 고유 ID(104+72)이며 POP/C/D/E/G 추가 시험은 별도 요구사항.

상태: **HV-T01..72 명세 완료/실행 0개/PASS 0개**, H-V 완성형 C++ 엔진·전분야 실제 검증은 별개. 문서·예제 작성으로 PASS를 선언하지 않는다.
