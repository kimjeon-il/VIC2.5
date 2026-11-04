# VIC2.5 Phase D-T1~T5 — 기술 시스템 상세 설계 색인

> **상태: 기술 D-T1~T5 세부 설계 확정 / 실제 C++ 기술 엔진·45개 자동 인수시험 미실행**  
> **계약 버전:** `phase-d-technology/1.0` · 기준일 2026-11-04 · 개발 브랜치 `work/phase-d-design`  
> 상위: `phase-d-common/1.0`, `phase-d-construction/1.0`, `phase-d-industry/1.0`, `pop-design/1.1`, `phase-c-common/1.0` 및 C-E/P/M/D4.

## 기술 상세 정본

| 단계 | 설계 문서 | 확정 구조 |
|---|---|---|
| **D-T1** | [기술/지식 분류·선행 그래프·보유자](D-T1_KNOWLEDGE_TAXONOMY_PREREQUISITES.md) | 복수 분야 기술 그래프/ALL·ANY 선행조건, 실용화와 지식 이해 구분, 기관별 `KnowledgeState`, 특허/설비/숙련 분리 |
| **D-T2** | [연구기관·연구재정·실험/검증](D-T2_RESEARCH_INSTITUTIONS_EXPERIMENTS.md) | 기존 Firm/Institution/POP·자산/금융·연구원/실험재, `ResearchWorkEvent`와 성공/실패 검증·차월 지식 |
| **D-T3** | [발명·지식접촉/확산·기술이전/IP](D-T3_INVENTION_DIFFUSION_IP.md) | 출판·교역·이동·교육/실제 통신·모방·특허/라이선스/영업비밀, 수용 역량/기술 유실 |
| **D-T4** | [실용화·사업장 기술도입·직업훈련](D-T4_ADOPTION_TRAINING_IMPLEMENTATION.md) | `SiteTechniqueAssignment`·설비/레시피/POP 숙련·D-C 실제 개축, 교육/근로 경험과 차월 도입/고용 |
| **D-T5** | [산업·경제·사회·정치·군사 영향](D-T5_CROSS_DOMAIN_IMPACTS.md) | 실제 산업/건설·보건·교육/인구·재정/법·군수/작전·환경피해·정치 반응과 장기 변화 |
| **인수** | [25 불변조건·45개 시험](TECHNOLOGY_ACCEPTANCE_TESTS.md) | `DT-INV-01..25`, `DT-T01..45`; 실제 C++ 실행은 **NOT_RUN** |
| **ADR** | [35개 기술 결정](TECHNOLOGY_ADRS.md) | `DT-ADR-001..035`, 기존 C-E/P/M/D4와 기술자·지식/원장 소유권 충돌 방지 |
| **검증** | [계약/문서·기준 매니페스트](verification_manifest.json) | 정본 파일 해시·ID/정적 fixture/게이트·미구현 기능 |
| **fixture** | [2개월 연구/차월 실용지식](examples/two_month_research.json) | 실제 연구원·시약·실험 서비스/공공재원·연구 실패/검증 시점·지식효력 |
| **fixture** | [세 주체 중 하나에만 기술이전·면허](examples/licensed_diffusion_three_holders.json) | 전파/교육·수용자 관측→이해, 실시권/금전과 지식·사업장 사용 분리, 제3국 자동 확산 방지 |
| **fixture** | [공장 기계도입·훈련·공사](examples/site_retrofit_training.json) | 실사용·현물장비/설치·기성/WIP, POP 10→4 고용전환, 재료/연료/금융·차월 공정 효력 |

## 고정 설계 원칙

1. **KnowledgeRegistry는 지식 보유·연구성과 근거만 보유**한다. 인구는 `PopulationSlice`, 고용 `EmploymentLedger`, 연구비·로열티/특허 장부 `FinancialLedger/OwnershipLedger/FiscalLedger`, 상품 `InventoryLedger`, 시설 `CapitalAsset`, 법 `PoliticalRegistry`를 변경 없이 활용.
2. 지식 `unknown/observed/understood/usable`, 연구 실적, 특허·사용권, 기업별/사업장별 공정 도입, 실제 POP 숙련·교육·현장 생산은 **서로 다른 상태**. 한 단계 완료가 다음 상태/전국 산업·군사·보건의 자동 효과가 아님.
3. 실제 연구·실험·교육·기술확산/장인 활동은 유효 인원/시간·시설/자산·학습/재료·자금/계약/법/통신 접근에 의존. 연구소/대학 설치만으로 국가 연구점수·전세계 기술확산 생성 금지.
4. `stage4` 연구·훈련/실물 생산, `stage8` 회계결산, `stage9a` 검증 및 다음 달 연구/도입 예약, `stage10` POP 자격/기능·전쟁/건강 실제 사건, `stage11` 전 분야 원자 커밋. 신규 지식·도입 효력은 **원칙적으로 m+1 stage0 이후**, POP 교육/자격은 m stage10 처리 후 차월 고용부터.
5. 전력 등 동월 비저장 실시간 `stage4` 생산/소비는 D-I2의 `I2-SAFE`/`I2-DISPATCH` 계약을 준수. **CE/C-D4 변경/마이그레이션·회귀 승인 전 미지원**.
6. 기술이 전쟁/사회·환경·의료/교육에 미치는 긍정·부정 효과는 실제 POP/기업·상품/시설·군수·법/행정 서비스 사건에 의해 실현. 국가 전체 무료 버프·군대 즉시 장비교체·문해율/사망률/정치 지지율 덮어쓰기 금지.

## 연구/혁신 참고 원칙·원작과의 차이

- [Victoria 3 기술 연구 개발자 설명](https://store.steampowered.com/news/posts/?enddate=1639070046&feed=steam_community_announcements): 대학·연구/혁신·기술확산이라는 문제 범위만 참고. **고정 3분야 트리/국가 혁신 점수·공짜 전파, 원작 코드·텍스트/수치 불사용**.
- [OECD Frascati Manual 2015](https://doi.org/10.1787/9789264239012-en): 기초/응용/실험개발 연구와 일반 교육/개선·장비 구입을 구별.
- [OECD Oslo Manual 2018](https://www.oecd.org/en/publications/oslo-manual-2018_9789264304604-en.html): 발명·연구와 실제 제품/공정의 사용자 도입을 구별.
- 역사 기술·교육/보건·군사·확산 모수는 출처/지역/시기·유효 법을 담은 버전형 Ruleset으로 보정. 현대 통계·제도를 모든 시대에 자동 소급하지 않음.

## 릴리스 게이트

| Gate | 완료 기준 | 현재 |
|---|---|---|
| DT-DESIGN | T1~T5 본문·35 ADR/25 불변조건·45 실행 시나리오/3 fixture·계약 manifest | **설계 확정** |
| DT-L0 | D-D0/DC/DI 및 실제 Phase C World/POP·경제/정치·군사 호환 계약/버전/데이터 정본 감사 | NOT_RUN |
| DT-L1~L2 | Knowledge/Research/Transfer/Adoption·POP 교육·C-E 자금/재료/설비·C-P/M 실제 C++ 구현 | NOT_RUN |
| DT-L3 | DT 45·DD0 24·DC 45·DI 45·기존 D4 48, POP/CE/CP/CM 회귀·fault injection/저장·직렬병렬 결정론 | NOT_RUN |
| DT-L4 | 1,200개월/10만·100만 POP 및 다수 기관·연구/기술복합 산업·법/전쟁 부하 계측 | NOT_RUN |
| DT-COMPLETE | 실제 코드·로그·오류/병목·fixture·seed·ruleset/commit·플랫폼 근거까지 PASS | NOT_RUN |

**현재 판정:** `D-T1~T5 DESIGN CONFIRMED` / `TECHNOLOGY_CPP NOT_RUN`. Phase C의 기존 48개 프로토타입 테스트 합격은 이 기술 분야의 검증을 대신하지 못한다.
