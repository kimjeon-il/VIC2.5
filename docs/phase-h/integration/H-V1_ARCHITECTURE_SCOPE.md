# H-V1 — 외교 통합 아키텍처·정본 소유·계약 기준선

> 계약 phase-h-integration/1.0 · 2026-11-04 · DESIGN_SPECIFIED / C++ NOT_IMPLEMENTED  
> 상위 H-D0(phase-h-common/1.0), H-D1~D7(phase-h-diplomacy/1.0).  
> 추가 시험: HV-T01~T72, 기존 H-D0 20 + H-D1~D7 84 = 176개 고유 외교 시험.

## 1. 목적과 완료의 뜻

H-V는 국가 간 관계·외교협상·복합조약·종속권한·국제협력체·양방향 영향력·국제위기가 **POP/경제/정치/군사/건설·산업·기술/지리·영토/무역**의 실제 원장과 유효시점에 맞게 작동하는지 검증한다.

- '외교 기능의 자료형만 있다' ≠ '외교 기능을 실제로 계산한다' ≠ '다분야 인수시험을 통과했다'. 시험은 독립 엔진에서 실제 연결된 거래/군수/금융/주권 원장의 전후값과 사건 ID를 증명해야 한다.
- 협상에서 실패·거절·조약위반·국제위기 동결·전쟁 패배·경제 불황은 합법적인 모형 결과가 될 수 있다. 유령 인구/돈/물품, 이중 조약효력·이중 주권, 승인되지 않은 권한, 잘못된 시간순서·정본 충돌은 엔진 결함이다.
- 상호영향 계산이 세밀해도 플레이어는 조약·통상정책·지원·중재·국가 운영 같은 **원인 행동**만 조작한다. 영향력/결속력/위기 긴장도는 원본의 파생값이고, 예상 파급효과는 툴팁의 설명형 조회로 검증한다.
- 검증은 지도 없는 최소 여러 국가/시장·영토 객체로 우선 수행한다. 별도 GPU 지구본 E-V 전체 합격은 H 내부 법/원장 검증의 대체가 아니며, 완성형 통합 릴리스에서는 실제 표시/피킹 회귀도 필요하다.

## 2. 계약 기준선 및 경계 리스크

| 계약/실행 영역 | 사용 정본·버전 / 경계 | 현시점 선행 위험 |
|---|---|---|
| POP-D0~D3 | pop-design/1.1: PopulationSlice.count 단일 인구 원본·인구 사건 | 실제 인구/군인/고용 이중계수 금지 |
| C-D0/C-D4 | phase-c-common/1.0, phase-c-integration/1.0: typed ID, 단일 World/StagingWorld, 12 단계·전체 원장, C-D4 48개 기존 시험 | Phase C의 일부 bounded prototype PASS와 전체 기능 인수는 별개 |
| C-P1~P5 | phase-c-politics/1.0: 국가/정부/헌정기관·비준권/법, 정치운동/선거 | 국가의 외교·전쟁권·비준은 C-P 법적 근거로 검증 |
| C-M1~M5 | phase-c-military/1.0: 동원/보급/전쟁/실제 점령·휴전·PeaceTreaty | H Treaty와 C-M5 PeaceTreaty 두 번째 정본 금지 |
| C-E1~E5 | phase-c-economy/1.0: Firm/Market/Trade/Financial/Fiscal/Inventory/Ownership/Shipment/FX | H 법적 통상조항과 C-E5 TradeAgreement 같은 권리의 이중 원본 금지 |
| D-D0/D-V | phase-d-common/1.0 + phase-d-integration/1.0: 실제 건설/산업/기술·연구·원장 | D-V 설계 정본은 work/phase-d-design에 있으며 전수 279 PASS는 증빙되지 않음 |
| E-D0/E-V | phase-e-common/1.0: 고정 Province, sovereignty/civil/fiscal/military, Claim/Dispute, 행정·선거구 | 현재 Phase E 47 bounded PASS/13 BLOCKED의 부분 통합, 전체 60 PASS 아님 |
| G-D0/G-T1~T5 | phase-g-common/1.0 + phase-g-trade/1.0: 기업간 무역·거래 이행·통관/관세 FX/위험 | G 설계 문서만, Phase G 실제 C++ 구현/62개 시험 미실행 |
| F 교통·물류 | 후속 독립 Phase F, 현재 C-E5 Shipment/Transport/Customs 어댑터 참조 | F 독립 계약/구현 미완료: 지원하지 않는 경로는 Unsupported |
| H-D0/D1~D7 | 외교 관계·교섭·조약·종속·기구·영향력·위기 | H 신규 C++ 엔진/104개 시험 미실행 |
| H-V 신규 | phase-h-integration/1.0, HV-INV01~36, HV-T01~72, HV-ADR001~030 | 문서·정적 검증 이후에도 실제 PASS는 별도 증빙 필요 |

현재 H의 정본 브랜치는 work/phase-g-h-d0; 코드 원천은 work/phase-e-cpp-integration에서 분기했다. 별도 Phase D 설계 브랜치/최신 C/D/E 구현의 전부를 main에 병합한 것으로 간주하지 않는다. **커밋과 계약 해시를 L0에서 잠근 뒤 소스 정본과 실제 호환/세이브 마이그레이션을 감사한다.**

## 3. 단일 소유권 매트릭스

| 자료의 진실값 | 유일 쓰기 원본 | H 통합의 읽기/쓰기 역할 |
|---|---|---|
| 인구·건강·교육·복무 대상 인원 | POP PopulationSlice + PopulationEvent, EmploymentLedger | 인적 교류/전쟁·외교기관 고용 조회; 인구 생성 금지 |
| 국가·정부·외교명령/비준·국내법 | C-P PoliticalRegistry/AuthorityGrant/ConstitutionalOrder | H 외교행위의 국내 적법성; 정부 교체와 국가 존속 분리 |
| 국토/주권·민정·세금권·군사통제/Claim | E/C-D0 JurisdictionRegistry + E Claim/Dispute | TreatyTerm/위기에서 범위 참조; 국가 변경은 E 이벤트 |
| 상품·재고/창고·운송중 물품 | C-E InventoryLedger, C-E5/F Shipment | 원조·배상/무역/군수 실제 인도만 참조 |
| 대금·채무/은행·외환·국고 | C-E3 FinancialLedger, C-E4 FiscalLedger, OwnershipLedger | 조약 미지급 의무와 실제 분개를 대조; H 돈 원장 없음 |
| 상업 거래·시장청산/통관 | C-E2 Order/TradeLedger + C-E5 Customs, G TradeExecution | TreatyTerm → read-only trade policy → 실제 C/G 거래·세금 |
| 건설·기업·설비·연구/기술 | C-E Firm/Asset + D Construction/Knowledge/SiteTechnique | 조약에 따른 투자·연구 협력/기지 시설의 실제 실행만 연결 |
| 군부대·교전·점령·군사 인력/보급 | C-M MilitaryRegistry/ServiceAssignment/Conflict/PeaceTreaty | 동맹 의무와 실제 참전·지휘/종전의 구별 |
| 조약·의무·국제협력체·협상·국제위기 | H Treaty/Term/Obligation/Negotiation/InternationalOrganization/InternationalCrisis | H 자체 법적/조직·절차 상태만 쓰기 |
| 외교 영향력·조직 결속력·위기 위험 | H 읽기전용 평가/캐시(타 분야 사건 파생) | **별도 가변 영향력/결속력/긴장도 원장 금지** |

주의: 기존 C-M5 PeaceTreaty / C-E5 TradeAgreement의 역사적 계약·원본이 존재한다. **H의 Treaty/Term은 이를 이관하기 전에는 매핑/어댑터로만 다루며**, 동일 채무·주권·세금·전쟁정본을 두 번 관리하지 않는다. 완전 이관은 분야별 ADR, schema/contract version 변경, 원본권한 전환, 저장파일 마이그레이션, 원장 회귀가 완료된 뒤 가능.

## 4. 통합 명령/조회 흐름

1. 사용자/AI/C-P/C-M/경제 시스템이 실제 원인 행동(Command)을 제출: 조약 제안·비준, 원조 지급, 관세정책/철도 투자, 보호국 권한 위임, 중재/군사 동원 등.
2. H-D1/D2가 유효 Country/Government/Office/정보·통신·협상권/상대방 접근성에 따라 DiplomaticAct/NegotiationCase 제안.
3. H-D3/D4/D5/D7이 발효 요건·조항 중복/상충·조직 의결·주권/전쟁/위기 적격성을 **기존 C-P/E/C-M/G**와 교차 검증한다.
4. 실제 상품/자금/노동/설비/군수/영토 변경 요청은 해당 소유 시스템이 C-D0 ProposedEvent/JournalBatch로 작성한다. H는 조항 이행상태와 source_event/transaction_id를 연결한다.
5. 모든 변경집합을 하나의 StagingWorld에서 검증하고 월말 한 번 commit. 실패하면 전체 rollback(월번호/ID할당/타 원장/캐시 revision 포함).
6. 다음 유효 snapshot에서 국가간 영향력(H-D6), 기구 결속력(H-D5), 위기 위험(H-D7)을 **기존 증거로부터 조회**. 캐시 수정이 새 거래/전쟁 명령을 직접 발행할 수 없음.
7. 설명툴팁은 같은 증거·규칙 기반으로 예상조건/불확실성과 사후 확정 변화를 구분한다.

## 5. 외교–무역–군사–주권의 핵심 인터페이스

- H TreatyTerm → G ResolveApplicableTradeRules (effective_month/scope/term_id) → C-E5 CustomsAssessment/Fiscal/Financial: 실효 관세·통상조건. 동맹=자동 관세 면제 아님.
- H TreatyTerm/OrganizationDecision → C-P/ C-M5 RequestEntryToWar / TroopWithdrawal / SupplyAgreement: 발동·국내 권한·실제 부대/물자/재원 확인. 방위조약=자동 전투 아님.
- H TreatyTerm/CrisisSettlement → E ProposedJurisdictionChange(ProvinceId/role/effective_month) + C-P constitutional/political succession: 정당한 영토 변경과 인구·국적/재산승계 별도. Claim/Dispute=확정 sovereignty 아님.
- G 실제 거래/금융·C-M 군사 지원/POP 사회 교류 → H AssessInfluence(A→B, source refs): 비대칭 평가. UI 라벨을 종속권한·기구 회원권으로 피드백 금지.
- E/C-P 분쟁·H 조약위반·G 통상차단·C-M 군사적 위협 → H InternationalCrisis/Negotiation → H Treaty와 C-M/E 실제 이행: 고위기 수치=자동 전쟁이 아님.

## 6. 오류·실행상태·검증 단위

- **InvalidCommand**: 무권한 사절/무허가 종속국, 미도달 통지, 쿼터/재정·지급 부족, 조약 충돌 등. 명시적 거부/부분 처리 + 상태 불변.
- **UnsupportedFeature**: F 물류 미연결, 국제협력체 계좌 OwnerRef 미지원, 당월 sub-tick 긴급 정책, 현대 제도 미지원 역사규칙 등. 가짜 성공 금지, 해당 통합시험은 BLOCKED.
- **InvariantViolation**: 동일 영토 이중 주권, 회계·재고/POP 보존 위반, 같은 TreatyTerm 중복 이행, 조약권한 없는 참전, 복수 동시 원본 쓰기. World abort 및 오류 증거.
- **ValidAdverseOutcome**: 조약 거절/위기 동결·금융부도·전쟁 패전·기업 손실/영향력 쇠퇴·기구 분열. 올바른 사건/회계/법적 제약을 만족하면 PASS 가능.

H-V은 새 프로그램 단계나 새 원장·별도 통합 경제/외교 점수를 도입하지 않는다. 단계별 실행과 불변식, 결정론·성능 및 수락 기준은 H-V2~V7에서 확정한다.

**현재 상태: DESIGN_SPECIFIED. 본 문서만으로 H C++ 코드/176개 실제 인수 통과를 주장하지 않는다.**
