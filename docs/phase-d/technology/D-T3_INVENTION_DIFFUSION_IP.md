# VIC2.5 D-T3 — 발명·기술확산·기술이전·지식재산/모방

> **설계 확정 / C++ 구현·시험 미실행** · `phase-d-technology/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [D-T1](D-T1_KNOWLEDGE_TAXONOMY_PREREQUISITES.md), [D-T2](D-T2_RESEARCH_INSTITUTIONS_EXPERIMENTS.md), C-E3/E5, C-P, POP/교육, D-I4. [색인](README.md)

## T3-1. 발견·발명·도입의 구분

- **독창적 발명(`invention`)**: 유효 탐색·실험/실용화가 낳은 새로운 지식/장치·공정의 기술적 내용. 발명 ≠ 상용화/전국 채택.
- **독립/병행 발명(`independent_discovery`)**: 서로 다른 연구 주체가 같은 기술에 정당한 실험/관측 근거를 가질 수 있다. 최초 한 건 외에는 지식 자체가 사라지거나 연구 원본을 덮어쓰지 않음.
- **기존 지식 습득(`acquisition`)**: 출판·거래·교육·면허/기술 협약·교류·재현 실험 등을 통해 수용자가 `unknown→observed→understood/usable`로 변경. 연구개발이 없는 도입도 가능.
- **기업/사업장 혁신(`implemented_innovation`)**: 기술 또는 공정·제품을 실제 사용자에게 제공하거나 사업장에서 사용한 사건. OECD Oslo Manual에 따라 발명과 실시/활용을 구별하되, 게임 안에서는 수량·가격·원가·설비/고용/사용을 실제 부문 원장으로 검증.

지식 보유와 권리·교육 숙련도는 별도다. 공개 출판으로 발신 주체의 지식이 감소하지 않으며, 수신자는 접근가능성·수용 역량·검증/현장도입 상태를 갖춰야 한다.

## T3-2. 지식접촉·확산 그래프

신규 `KnowledgeContactEdge` 관계(정본 사건/증거에 기초): `from_holder_ref, to_holder_ref, channel_kind, geographic_or_network_route_refs[], communication_service_refs[], effective_period, access_scope, bandwidth_or_capacity_rule, confidentiality_or_law_refs, provenance`.

접촉 경로 예시:
- 기업·장인·군인의 이동/교육·유학·노동 전직: 실제 POP/Employment·C-E5 접근·체류/이주관계(인구 원본 재생성 금지).
- 상업·기술 라이선스/합작·설비 구입: 실제 계약/자금·산업재/Shipment, 판매자가 기술 전수 조항을 가질 수도 없을 수도 있음.
- 학술 출판·강의·공공 문헌/도서관·실험 발표: 실제 공공/민간 출판·교육/연구서비스·매체 이용 가능성/해독·언어 접근. **정보 자체는 반드시 물건 Shipment일 필요는 없음**; 원거리 소통은 시대별 유효 우편/교통/전신/통신 서비스와 합법적 접근 경로로 표현.
- 동맹·정부기관·외교·군사교육 교류: 유효 정부·법/안보·계약, 교관/훈련/자료·접촉 기록.
- 장치 관찰·역공학: 실제 접근 가능한 장치/자료와 시험·숙련/분석 비용·법적 위험. 기계 수입은 지식 보유 자동 획득이 아님.

`PotentialExposure`(접촉가능성)는 예상 조회지표, `KnowledgeTransferProposal`은 학습의향, `KnowledgeTransferEvent`은 실제 유효 접촉/학습·실험 근거다. **송신·광고·소문·물건 소유와 수신자의 지식 이해를 동일 사건으로 간주하지 않는다**.

## T3-3. 수신자의 흡수 역량과 전파 계산

`assimilation_feasible`은 최소한 유효 접촉/언어·기초지식·학습인력·문해/실습역량·교육/훈련 서비스·공개/권리 조건·실제 시간/비용을 확인한다. 기술 난도/매체 종류에 따라 문해가 필요 없는 장인 전수, 교육받지 못한 집단의 실습 학습도 허용할 수 있으며, **문해율을 국가 전체 지식 접근의 단일 스위치로 사용하지 않는다**.

~~~text
eligible_transfer_work = min(
  effective_contact_and_access_service,
  qualified_recipient_learning_service,
  required_instruction_or_experiment_material,
  funding_or_lawful_obligation_capacity,
  valid_knowledge_and_method_prerequisites
)
new_knowledge_evidence =
  verified(eligible_transfer_work, transfer_mode, rule_version, RNG)
~~~

- `m` 접촉·학습을 기록한 경우 단계9a 검증이 성공한 지식 상태는 원칙적으로 `m+1 stage0`부터 유효. 같은 달에 자신이 아직 이해하지 못한 기술을 외국 사업장 전부에 역전파하는 사슬 금지.
- `DiffusionObservations`(어떤 기관이 어떤 기술을 언제 접하고 습득했는지)는 `KnowledgeRegistry`/실제 접촉·학습 사건에서 파생, **국가별 무료 `tech_spread_points` 원본 없음**.
- 정책·학교/출판 자유/검열/통신·언어장벽/영업비밀, 이동/전쟁·봉쇄·통상관계는 실효 접촉의 원인을 바꾸지만 법률·경제/인구 원본은 따로 유지. 무조건 모든 국가에 자동 확산 금지.
- 지식이 공개/복제 가능해도 학습에는 실제 수행 시간/능력이 필요하다. 이후 같은 지식의 여러 보유자에게 복제되더라도 **재고 보존처럼 발신자의 지식을 차감하는 것은 금지**.

## T3-4. 특허·비밀/면허·계약

| 층위 | 원본 및 처리 |
|---|---|
| 지식의 사실/이해 | `KnowledgeRegistry`; 특정 주체가 아는/모르는 상태, 전파 증거 |
| 특허·독점/공개·보호기간 | C-P `Law/PoliticalRegistry` + `OwnershipLedger`의 유효 특허/노하우 권리·관할 |
| 실시권·사용계약 | `LicenseAgreement`(참조 관계): `license_id, right_id, licensor, licensee, territorial_scope, field_scope, term, sublicensing_rule, fees, effective_month, source_event_id` |
| 실시료·선급/로열티 | `FinancialLedger` 실제 채권/채무·지급 및 E4 유효 조세/세수 |
| 실제 기술운용 | D-T4 `SiteTechniqueAssignment`와 C-E1 실제 시설·노동/공정·설비 |

- 법적 특허 없는 역사적 경제도 자연스럽다. `PatentRight`는 자동으로 모든 발견에 생성되지 않는다. 비밀공방/국가기밀, 공개문서/장인 지식과 특허권을 혼동하지 않음.
- 특허를 소유해도 기술 지식·설비·인력이 저절로 생기지 않고, 공개 기술을 이해해도 유효 법의 보호 범위 안에서 상업적 실시가 제한될 수 있다.
- `Licensor`/`Licensee`·특허 유효지역/만료월/면허 수량/분야는 적법한 법률/계약에 의해 결정, 국제 분쟁·외환 통제가 있을 경우 실제 E5/Financial 지급 제약. 모든 국가의 특허법을 현대 표준으로 강제하지 않음.
- 모방·역공학/비합법 사용은 해당 시대의 법률·실제 접근·연구/실험·집행과 위험을 검사. 정책 위반의 경우 **제재·배상채권/실제 세금·벌금·몰수**를 별도의 Fiscal/Financial/Ownership 사건으로 처리. '특허 없으면 기술보유 삭제' 같은 상태조작 금지.
- 권리 양도/면허 지급은 지식 자체의 물리적 독점소유와 다름. 같은 발명 하나에 다수 지식 보유자가 존재할 수 있고, 특정 사용권자가 파산해도 다른 보유자의 지식이 소멸하지 않음.

## T3-5. 기술격차·지역/기업 간 확산·유실

- 국가 기술격차는 '국가 기술 수준 점수'가 아니라 분야별 보유·접근 가능 기술과 실적용 사업장/인적 숙련의 **비중/시간/지역별 관측**으로 표현한다. 주권·국적 변화가 주민/기업의 기억·시설 상태·권리를 자동 바꾸지 않는다.
- 기술 도입에 필요한 기능적 노하우가 소수 인원·비공개 장부·한 시설에만 남고 연구자/기술자가 이탈·사망하거나 전쟁으로 기록/연구시설이 파괴되면 **해당 기관의 재현 가능 역량**이 떨어질 수 있다. 다만 이미 다른 기관이 확보한 공개 문헌·권리·연구기록까지 세계에서 삭제하지 않는다.
- `KnowledgeAccessLostEvent`는 유효 인적/문서/시설 출처를 참고하여 `usable→understood/observed` 같은 상태 악화 가능성을 기록, **과거 성과를 무효/소급 삭제하지 않고 이후 월의 효력 변경**. 기술 재발견/재학습 경로는 열어둔다.
- 다국가 전쟁·금수조치가 기술정보 교류를 제한할 수 있어도 이미 습득한 방법/설비/인력을 그대로 지우지 않으며 C-M·C-E5의 실제 사건을 근거로 한다.
- 초기 역사 자료의 미상/관측 불완전성과 기술 미발견 상태는 구분. '기술 보급 데이터 없음'을 자동 `unknown`으로 강제하지 않는다.

## T3-6. 수락 조건

1. 실제 유효 교류·학습 없이 국내 A의 기술이 외국 B로 자동 전파되지 않음.
2. 공개 출판·실제 교류는 다른 보유자에게 근거 있는 관측/습득을 남길 수 있지만 송신자의 지식이 사라지지 않음.
3. 장치 수입만으로 완전한 기술 `usable`·면허/훈련·설비가 즉시 생기지 않음.
4. 동일 기술의 특허권자·실시권자·연구기관·사업장 사용자를 구분, 권리·실제 금전/기술 사용 중복 없음.
5. 소속 국가/기업이 변경·소멸해도 명시적 계승/이전·실제 접근 손실을 기록하고 과거 지식 사건 유지.

**세부 시험:** `DT-T19..T27`. [인수시험](TECHNOLOGY_ACCEPTANCE_TESTS.md).

## 참고

- [OECD Oslo Manual 2018: 지식과 혁신](https://www.oecd.org/en/publications/oslo-manual-2018_9789264304604-en/full-report/component-7.html) — 정보의 전달과 실제 지식의 수용·학습 구별.
- [OECD Oslo Manual 2018](https://www.oecd.org/en/publications/oslo-manual-2018_9789264304604-en.html) — 발명과 기업/제품/공정에서 실제 활용되는 혁신의 구별.
