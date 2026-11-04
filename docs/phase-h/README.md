# VIC2.5 Phase H — 외교·국제관계

> **H-D0 공통 + H-D1~D7 상세 + H-V 통합 검증 설계 문서 작성 완료 / C++ 엔진·실제 인수시험 미실행**
> 2026-11-04 · 공통 계약 phase-h-common/1.0 · 상세 설계 phase-h-diplomacy/1.0
> 브랜치: work/phase-g-h-d0 (기준: work/phase-e-cpp-integration)

## 상세 설계 정본

| 단계 | 문서 | 주요 책임 |
|---|---|---|
| H-D0 | [외교 공통 계약](H-D0_COMMON_CONTRACT.md) | 외교 객체·권한·조약/국제협력체/영향력/국제위기·12단계/단일 원장, 20개 불변식과 20개 기본 시험 |
| H-D1 | [국가 간 관계·승인·인식](H-D1_RELATIONS_RECOGNITION_INTERESTS.md) | Country/Government/승인 구별, A→B 비대칭 신뢰·위협·정보접근, 국가별 이해관계 |
| H-D2 | [외교활동·협상·중재](H-D2_DIPLOMATIC_ACTIONS_NEGOTIATIONS.md) | 외교관/공관의 실제 비용·정보/통신시차, 다자 협상·반대제안·최후통첩 |
| H-D3 | [조약·복수 조항·비준/이행](H-D3_TREATIES_CLAUSES_ENFORCEMENT.md) | TreatyTerm별 법적 효력/시간, 상대국 의무·분야별 실제 이행, 위반/개정·승계 |
| H-D4 | [종속·보호국·주권/자치](H-D4_SUBORDINATION_SOVEREIGNTY_AUTONOMY.md) | 국가 간 외교권·전쟁권·과세·통상·행정 등 권한 행렬, 독립/국가승계·E 주권 |
| **H-D5** | [**국제협력체**](H-D5_INTERNATIONAL_ORGANIZATIONS.md) | 헌장·회원국 다중가입·의결/지도·공동기금·기구변화·실제 공동정책 |
| **H-D6** | [**국가 간 영향력**](H-D6_INFLUENCE_DERIVED_POWER.md) | 경제·군사·정치·문화의 방향성·다면적 **자동 산출**, 중첩 영향권은 설명용 뷰 |
| **H-D7** | [**국제위기·다자 중재·강화**](H-D7_INTERNATIONAL_CRISES_PEACE.md) | 실제 분쟁 기반 위기·소국/중립/제3국 참여·부분합의/동결·전쟁/평화 연계 |
| 인수 | [104개 상세 인수시험 명세](DIPLOMACY_ACCEPTANCE_TESTS.md) | H-D0 기존 20 + H-D1~D7 각 12 = **총 104개 고유 시험** (NOT_RUN) |
| ADR | [35개 외교 설계 결정](DIPLOMACY_ADRS.md) | 관계/협상/조약/종속·국제협력체/영향력/국제위기 구분·중복원장·월효력 |
| **H-V** | [**외교·전분야 통합 검증 설계**](integration/README.md) | H-V1~V7: 36개 교차 불변식, 72개 신규 교차시험, 30개 ADR, 6개 복합 수치 예제, 6개 수락 게이트; 기존 104 + 신규 72 = **176개 정의 시험** (NOT_RUN) |

## 기계판독 예제 — 설계 fixture, 실제 세이브/실행결과 아님

- [복합조약 서명·비준·조항별 발효](examples/multiterm_treaty_ratification.json): m월 비준, 관세 m+1, 군철수 m+3·실제 군사사건 필요.
- [국제협력체 공동기금 일부 납부](examples/organization_dues_partial.json): 약정 300·실제 120·미납 180.
- [경제 영향력 공급망 노출](examples/influence_supply_exposure.json): 철강 수요100·A산60·대체35 → 공급차질 25(조건부 노출이며 영향력 포인트 아님).
- [국제위기 중립 중재와 평화](examples/crisis_mediation_without_war.json): 영토분쟁/중재·배상 100 중 20 지급/미지급80, 자동전쟁 없음.
- [보호국 외교권 위임](examples/protectorate_delegated_rights.json): 대외권한과 지방 과세/교육권 분리, 무권한 제3국 방위조약 거부.

## 설계의 공통 원칙

1. **국제협력체는 공식 조직이다.** 가입/중복 가입·탈퇴/지도권·공동정책은 실제 헌장·조약·정족수/비준·회계·군사·무역 집행에 따른다. 회원국이라는 사실만으로 무료 산업·군사/통상 보너스나 자동 참전은 없음.
2. **영향력은 자율적으로 파생된다.** A국의 B국 투자·수출입·신용/군사원조·정치/사회문화 관계가 변하면 방향성·분야별 영향력이 자동 변화한다. '영향력 올리기/세력권 편입' 버튼 없음. 비공식 영향권은 분야·기간·신뢰도에 따른 지도용 해석이다.
3. **국제위기는 외교 사건이다.** E Claim/Dispute, C-P/C-M, G 통상갈등·H 조약위반 등 실제 원인에서 시작하며 참가·중립/중재·다자 타협·동결/재개·전쟁 연계가 가능. 긴장도 100에서 자동전쟁 금지.
4. **국제적 합의는 실행과 구분한다.** 서명·비준·발효·실제 지급/인도·병력 이동/주권 이전을 구별. C-P/C-M5 PeaceTreaty, C-E5 TradeAgreement/FX/Customs, E Province/Jurisdiction, G/C-E2/3/4의 돈·재고·시장·통상은 기존 정본의 소유권을 유지한다. **같은 효과 중복 적용 금지**.
5. **실효 자원과 시간.** 공관·외교관/국제협력체 기관/기금, 군사원조·조약 보장, 거래/병력·금융/영토 집행에는 실제 인력·예산/재고·시설·권한이 필요. C-D0 공개 월별 12단계와 m월 stage9 신규 정책의 기본 m+1 stage0 효력을 유지.
6. **내부 정교함/단순한 게임플레이.** 플레이어는 정책·조약·중재·공약/외교적 요구 등 원인이 되는 결정을 한다. 복잡한 파급은 툴팁/설명 패널에서 불확실성·대체가능성·실제 근거 사건과 함께 확인한다. [AGENTS.md](../../AGENTS.md) 준수.

## G/F/C/D/E와의 연계

- [Phase G 무역 상세 계약](../phase-g/README.md): 실제 수출입·통상정책/세관·상품/은행 원장. H의 조약은 유효 TradePolicyProjection만 공급한다.
- Phase F 물류 세부 설계·구현은 본 브랜치에서 정본으로 간주하지 않으며, 현재 C-E5 운송 어댑터/지원 경계를 따른다.
- [C-D0](../phase-c/C-D0_COMMON_CONTRACT.md): World·원장·12단계·Command/Journal/원자 rollback.
- [Phase E](../phase-e/E-D0_COMMON_CONTRACT.md): 고정 Province·주권/민정·과세·군사통제/Claim·행정.
- [C-M5](../phase-c/military/C-M5_WARS_OCCUPATION_PEACE.md): 전쟁·실제 참전/점령·휴전/평화의 군사 원본.
- Phase D 설계 정본은 별도의 work/phase-d-design 브랜치, 실제 제한 구현은 Phase D C++ 통합 브랜치에 있으므로 상태를 혼동하지 않는다.

## 외교 통합 검증과 실제 구현 후속 순서

[H-V — 외교 통합 검증 설계](integration/README.md)에서 조약·국제협력체·국가 간 영향력·국제위기가 POP·경제·정치·군사·건설/산업/기술·지리/주권·무역/물류의 **실제 단일 World/원장**을 통해 연결되는지 검사하는 구조를 확정했다. 신규 HV-T01~72(71개 독립 교차 + 1개 증거 메타게이트)는 기존 H-D0 20·H-D1~D7 84개와 별개이다. 총 176개 외교 시험의 실제 코드는 아직 작성/실행하지 않았다.

실제 구현 순서: HV-L0 계약/소스 원본 감사 → H-D1..D7 C++ 구현·C-D0 StagingWorld 연동(HV-L1/2) → 104+72 및 기존 POP/C-D4/D-V/E-V/G 회귀(HV-L3) → 저장/worker1/N/전원차단 복구(HV-L4) → 1,200개월·국가/조직·무역/군사·POP 대표 부하, AI/툴팁/역사법제 검증(HV-L5).

**현재 검증 상태: 설계 문서·GitHub 저장 확인 / 기존 H 104 + H-V 72 = 176개 정의, 실행 0 / Phase H C++ 엔진 NOT_IMPLEMENTED / HV-COMPLETE NOT_DONE / main 미병합.**
