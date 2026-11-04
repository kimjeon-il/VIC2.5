# VIC2.5 D-T1~T5 — 기술 시스템 인수시험·불변조건

> **설계 확정 / 실제 C++ 구현·자동시험 미실행** · `phase-d-technology/1.0` · 2026-11-04  
> 25개 기술 불변조건 `DT-INV-01..25`, 45개 개별 수락시험 `DT-T01..45`.  
> 상위 DD0 (14/24), D-C (25/45), D-I (25/45), C-D4(30/48), POP/CE/CP/CM 회귀를 삭제·대체하지 않는다.

## 1. 시험환경·실행 규약

- 가상 국가 A/B와 3개 `Region`, 공공 연구소/대학, 민간 발명 공방, 제조·건설·의료/교육 기관·병참 연구부서, POP 연구원·교원·정비/군인, 금융·재고/설비/소유권·법률/기술 실시권, 운송/통신·실험자료/재료의 실제 원장 관계.
- 모든 예시 숫자는 `fictional_test_value`. 연구 작업단위/원가·확률/문해·기간/로트는 실제 역사 관측값이나 Victoria 3 계수가 아님.
- 시작 월 WorldSnapshot + 동일 seed/ruleset/typed ID/명령, stage0~11(9a~9e/10a~10f), single StagingWorld staging/commit/abort. 실제 성공 시 Knowledge/ResearchWork/Financial/Employment/Inventory/Ownership/POP/Political/Military 도메인 사건과 단일 월 커밋을 대조.
- 예상 `DT_ERR_*` 조건은 예외 발생만으로 통과가 아니라 **정확한 오류 코드와 유효 원장 상태·ID 발행/예약/월 인덱스까지 전부 원복**해야 PASS. 자연스러운 연구 실패, 기업 도입 거절·비용 상승, 군사 패전·인적 지식 유실은 법/원장 규칙이 정합하면 정상 결과.

## 2. 불변조건

| ID | 보존 조건 |
|---|---|
| DT-INV-01 | TechnologyDefinition/TechniqueDefinition/KnowledgeHolder/ResearchProject/SiteTechniqueAssignment와 Firm·POP/CapitalAsset/법률·소유·돈/재고 정본 분리 |
| DT-INV-02 | 기술 ID stable typed ID/유효기간·계보/규칙 버전, 기술 선행 조건 DAG 무순환·참조 일치 |
| DT-INV-03 | 지식 상태 `unknown/observed/understood/usable`는 holder×tech×effective period별 유효한 증빙/사건을 가짐 |
| DT-INV-04 | 연구 작업/실험진척은 실제 `ResearchWorkEvent` 집계/검증이며 별도 가변 `research_points`/국가 혁신점수 원본 금지 |
| DT-INV-05 | 연구기관/프로젝트·회사/정부·군사 단위의 인원수는 오직 `PopulationSlice.count`에서 조회; 교육·숙련 POP 정본만 수정 |
| DT-INV-06 | 연구/교육/산업·군사/건설은 동일 인원·장비서비스·재료/계좌를 중복 점유/예약하지 않음 |
| DT-INV-07 | 실제 연구·훈련/실험은 유효 연구원·시설/실험재·권한/금융 및 검증된 방법의 상한 내에서만 실행 |
| DT-INV-08 | 연구 계획·예산승인·대출한도·기술목록 존재·대학 건설만으로 무료 KnowledgeEvent 생성 불가 |
| DT-INV-09 | 실험 성공/실패·partial result·우발 발명은 유효 사건/확률/수치정책과 근거 있는 검증/동률/결정론 |
| DT-INV-10 | 월 m stage4 연구실적·stage9a 결과는 같은 월 완료된 생산/전투/노동·정치 결정을 소급 바꾸지 않음 |
| DT-INV-11 | 신규 연구·전파 결과의 KnowledgeState 적용은 기본적으로 `m+1 stage0`, 같은 9a에 연쇄 즉시 사용 불가 |
| DT-INV-12 | 지식 전달은 송·수신자/경로·기록·수용 역량·합법 권리를 검증, 전세계 자동 무조건 기술확산 금지 |
| DT-INV-13 | KnowledgeTransfer는 비경합 지식의 복제/학습이므로 발신자 지식을 Inventory처럼 자동 차감하지 않음 |
| DT-INV-14 | 지식/공개/문헌 접근, 연구성과, 특허권/사용 면허/사용료, 개별 사업장 실제 도입은 각각 다른 정본 |
| DT-INV-15 | 권리 이전/로열티·보조금/연구비는 유효 법·Ownership/Financial/Fiscal에 대응, 화폐/예금 자동 생성 금지 |
| DT-INV-16 | 공정 도입 `active`는 실제 기술 이해/접근·법·레시피·실제 설치설비·적격 노동·차월 효력 요건 충족 |
| DT-INV-17 | 기술을 알고 있는 국가/기업이 다른 현장·인구/군부대의 기술·숙련/장비·가동상태를 자동 덮어쓰지 않음 |
| DT-INV-18 | 신규 기계/설비는 D-C 실제 공사/설치·WIP와 C-E1/Inventory/Ownership을 보존, 보유 기술만으로 공장 능력 증가 금지 |
| DT-INV-19 | POP 직업훈련·경험의 자격변경은 실제 훈련/직업전이·교육/고용/보건과 stage10 인구 사건 이후 차월 근로에 적용 |
| DT-INV-20 | 기술별 효과는 실제 생산 레시피·건설/군수/보건/교육/행정 서비스·POP/재정·군사 원장 사건에서만 발생 |
| DT-INV-21 | 기술/고용·보건/정치/군사 수준 변화가 생산성·사망률·전투력/지지율을 근거 없이 자동 수정하지 않음 |
| DT-INV-22 | 문서/기술자·시설 손실로 특정 보유자 접근·사용 가능성이 감소할 수 있으나 과거 사건/타 보유자 지식을 소급 삭제하지 않음 |
| DT-INV-23 | 미지원 I2-DISPATCH 동월 비저장 실시간 발전·공장소비를 새 기술로 우회해 허용하지 않음 |
| DT-INV-24 | 전시·연구 파산/지역 관할·법/특허기간 변경도 유효 인구·돈/부채·재고·설비/권리·역사 지식을 사건 없이 소멸·중복하지 않음 |
| DT-INV-25 | 원자적 월 1회 commit/전체 Abort·source_event_id 멱등성, fixed-point/checked arithmetic·seed·worker 1/N·save/replay 버전 정합 |

## 3. 수락시험 행렬

### D-T1 — 지식분류·선행조건·보유자 (01~09)

| ID | 시나리오 | 합격 조건 |
|---|---|---|
| **DT-T01** | 과학·농업·군수 등 복수 태그, 1기술→여러 공정 연결 | 1개의 `TechId`와 복수 `RecipeId` 관계, 기술 1개로 공장/인구 자동 증가 없음 |
| **DT-T02** | 선행 A→B→C→A 순환 | `DT_ERR_TECH_DAG`, 잘못된 규칙 세트/월 상태 적용 거부 |
| **DT-T03** | 선행 `ALL(A,B)` 미충족, `ANY(A,B)` 충족 비교 | 정확한 논리식 평가, 기술 가용/검증 상태는 요건 충족 범위에서만 |
| **DT-T04** | 연구기관 usable, 같은 국가 외부 제철소 unknown | 국가 통계 공유만으로 제철소 KnowledgeState/active 자동 상승 없음 |
| **DT-T05** | 기술 observed/understood/usable 3상태 다른 기관 | 동일 기술·서로 다른 주체의 유효 증빙·시점 보존, 임의 일괄 전환 없음 |
| **DT-T06** | 기술명 개정/동일 발명 분류 재편 | 역사 사건/TechId 유지, 새 버전 crosswalk만 변경 |
| **DT-T07** | 특허권자 A는 `usable`, 사용권자 B는 `observed` | 특허/실시권이 B의 실용지식·설비/인력 자동 생성하지 않음 |
| **DT-T08** | 과거 기술·현대 기술이 다른 지역/시대에서 발견되며 초기자료 일부 미상 | 0/unknown/estimated/provenance 구분, 역사 연대 강제 트리 없음 |
| **DT-T09** | 같은 KnowledgeEvent 재송신·효력월 역전 | `DT_ERR_EVENT_REPLAY`/`DT_ERR_TIME_TRAVEL`, staging rollback |

### D-T2 — 연구자원·연구재정·실험 (10~18)

| ID | 시나리오 | 합격 조건 |
|---|---|---|
| **DT-T10** | 연구 예산 100만 승인, 실제 국고/대출·현물자원 0 | 연구원 급여·실험실적/KnowledgeEvent 무료 생성 금지 |
| **DT-T11** | 연구계획8, 유효 연구원5/실험실3/시약4 단위 | 연구 실적 최대3, 그에 대응하는 실제 연구/노동·재료/설비 원장 |
| **DT-T12** | 공장 3 연구원·군수 연구소 3, 같은 실제 적격자 4명 | 고용 인구 합 <=4, 동일 POP 분할/군복무·직무 이중배정 없음 |
| **DT-T13** | 실험재 원료 5가 2개월 운송 중이고 연구자는 출근 | 실입고 전 사용 0, 연구실적은 다른 가용활동에 한정 |
| **DT-T14** | 연구 실적 임계치 충족했으나 검증 실패/불확실성 결과 | `KnowledgeState` 실용화 자동 성공 금지; 실패 원가/연구실적 보존 |
| **DT-T15** | 연구 3·3→검증된 총6, 월 m 9a에 완료 | 월 m 과거 생산 불변, `m+1 stage0`부터 승인 지식/후속 연구에 사용 |
| **DT-T16** | 연구재정 100/연구비 실제 지급60·미지급40 | 연구/금융 발생·지급·권리 분리, 공짜 현금/성과 40 없음 |
| **DT-T17** | 학교/공방·국립연구소·개인 장인이 실험, 현대 은행/특허법 없음 | 적법한 가계·현물/노동/실험지원이면 연구 가능, 현대 제도 강제 금지 |
| **DT-T18** | 공동 연구의 연구 수행자 B, 후원 A, 결과 특허권자 C | 연구원/원가·채권·권리/정보 접근 관계를 실제 계약 근거로 분리 |

### D-T3 — 발명·확산·권리 (19~27)

| ID | 시나리오 | 합격 조건 |
|---|---|---|
| **DT-T19** | 국가 A에 공표된 기술이 있으나 B 접촉·교육·교역/기록 없음 | `B.unknown` 유지, 무료 글로벌 확산 없음 |
| **DT-T20** | 출판/실제 교관 교류·수용 B가 학습 후 verified observed | B `observed`(차월)·A `usable` 보존, 지식 Inventory 차감 없음 |
| **DT-T21** | 회사 B가 기계상품 수입했으나 공법자료/교육·권리 없음 | 상품재고 확보는 가능, `KnowledgeState.usable` 및 설치/가동 보장 안 됨 |
| **DT-T22** | 회사 B가 A 기술에 라이선스12 지급, B는 아직 이해 부족 | 실제 A/B 금융분개·법/Ownership 실시권은 등록, B 지식·공장 자동 active 아님 |
| **DT-T23** | B가 유효 채널·학습력·재료 확보 후 `observed→understood` | actual contact/transfer, `m+1` 효력·기존 지식 보유자 상태 유지 |
| **DT-T24** | 장인 2집단 독립으로 동일 발명 확인 | 연구/KnowledgeEvent lineage 2개·TechId 공유 가능, 중복 세계기술/설비/인구 생성 없음 |
| **DT-T25** | 기술자의 해외 이주·문서/강사 상실·다른 기관 공개 문서 존재 | 해당 기관의 실효 기술 접근만 차월 변화, 다른 holder 지식/역사 사건 보존 |
| **DT-T26** | 특허 만료·입법/국가 점령, 이미 유효한 B 라이선스와 부채 | 권리·미지급 사용료/소유/주권 구별, 법 효력월/계약 따라 갱신 |
| **DT-T27** | 동일 수신자에게 같은 전파 사건 두 번 적용, 불법 특허 사용료 징수 | 정확한 `DT_ERR_EVENT_REPLAY`/`DT_ERR_IP_AUTHORITY`, 금전/지식 중복 원복 |

### D-T4 — 사업장 도입·실용화·숙련 (28~36)

| ID | 시나리오 | 합격 조건 |
|---|---|---|
| **DT-T28** | 국가 기술 usable, 기업 지식 unknown·설비 없음 | 기업 설비/생산/공정/고용 변화 0 |
| **DT-T29** | 기업 usable+특허/레시피 유효, 설비 상품만 창고에 존재 | 설치·실제 D-C 준공 전에 `active` 거부, 기계 재고/자산 이중계상 없음 |
| **DT-T30** | 공장 m stage11 개축 ready, m+1 stage0 기술효력 | m 기존 레시피/생산 불변, m+1부터 실제 가동조건 충족 시 전환 |
| **DT-T31** | 월 m stage4 현장교육 3, stage10 자격인증2 | m stage2 인력/급료 소급 변경 없음; m+1 stage2에 신규 적격2 적용 |
| **DT-T32** | 수공업 10 노동자→기계화4, 동일 POP 10 | 4 고용·나머지6 재배치/실업 명시, 인구10·기업 소유권/현금 자동변화 0 |
| **DT-T33** | 두 사업장/생산라인이 같은 설치기계 서비스5를 각각4 사용 요청 | 실사용 합 <=5, 실제 Capacity/Employment/Inventory 예약 정합 |
| **DT-T34** | 선진공정은 에너지/특허료가 높고 구공법은 원료/노무만 필요 | 예측·실제 비용에 따라 기존 공법 선택 가능, '기술 최신=무조건 상위' 금지 |
| **DT-T35** | 현장 active 기술이나 원료·자금/고용 부족 | actual_output=0 가능, 기술 보유 상태는 합법 유지, 공짜 매출/재고 생성 금지 |
| **DT-T36** | 새 기술로 동월 stage4 비저장 전력 생산→다른 공장 즉시 사용 요청 | I2-SAFE에서 `DT_ERR_UNSUPPORTED_DISPATCH`, 상위 CE/D4 계약 변경 전 금지 |

### D-T5 — 다분야 효과·장기/오류주입 (37~45)

| ID | 시나리오 | 합격 조건 |
|---|---|---|
| **DT-T37** | 의학 usable, 약품·병원·의료인/서비스 0 | 주민 사망/회복 즉시 개선 0, 실제 의료자원 필요 |
| **DT-T38** | 새로운 교육기법 usable, 학교·교사/현장교육 0 | POP 문해율/자격 자동 증가 0 |
| **DT-T39** | 실제 보건·교육 서비스 제공, 월 m stage10 건강/자격 전이 | 실제 POP/재고·금융/교사·의료인 원장, m+1 노동/인구 상태 적용 |
| **DT-T40** | 군사기관에서 신기술 지식 확보, 총기·장비/탄약·교관·군수 공급 없음 | 부대 장비·탄약/전투력 자동 증가 없음 |
| **DT-T41** | 군사기술의 실제 장비제작·배속·훈련/보급, stage10 작전 | 기존 C-M/Inventory/POP/Financial과 연결한 적법 효과·피해, 원장 중복 없음 |
| **DT-T42** | 산업 자동화로 고용·임금/소득 변화, 정치 집단의 반응 | 실제 E/POP/금융 변동 후 `stage9b` 정치 갱신, 기술 확보 자체로 정당 지지 무작위 덮어쓰기 없음 |
| **DT-T43** | 월 m stage9d 공공연구지원/특허·학교 법률 제정 | 기존 m 지출/기술확산·선거/군수효력 소급 0, 차월 실제 재원/법에 한정 |
| **DT-T44** | 오염 유발 신공정·산업재해, 방산 연구시설 전쟁 피해 | 실제 배출/재고·부상/시설손상·보험/권리·기술접근 변화, 기술 발명 전체/타 기관 지식은 보존 |
| **DT-T45** | 기술발견·도입/실패·파산·전쟁·교육/산업화가 섞인 1,200개월, save/replay·worker 1/N·규칙버전 거부/에러 주입 | DT/DI/DC/DD0/POP/CE/CP/CM/D4 실수락·모든 보존/결정론/월원자성, 실제 로그/성능·원장 검증 PASS |

## 4. 오류 코드와 C-D4 대응

| 기술 오류 | 의미·대응 |
|---|---|
| `DT_ERR_TECH_DAG` | 순환/미존재 기술·선행조건, 규칙무효 → `D4_ERR_RULESET_COMPAT` |
| `DT_ERR_KNOWLEDGE_PROVENANCE` | 출처·검증 없는 지식/미상 상태 확정 → `D4_ERR_COMMIT_VALIDATION` |
| `DT_ERR_RESEARCH_RESOURCE` | 연구 시설·연구원·시약/장비/권한·실재 자원 부족 → `D4_ERR_REVISION_CONFLICT`/`D4_ERR_INVENTORY_OVERDRAWN` |
| `DT_ERR_POP_OVERALLOC` | 연구/교육/공장/군사 동일 인원 중복 → `D4_ERR_POP_OVERALLOC` |
| `DT_ERR_CAPACITY_OVERALLOC` | 실험실/훈련·사업장 기계서비스 중복 → `D4_ERR_REVISION_CONFLICT` |
| `DT_ERR_FUNDING` | 연구비/특허·교육비·계좌 이중예약/대차오류 → `D4_ERR_FINANCIAL_UNBALANCED` |
| `DT_ERR_TRANSFER_ACCESS` | 유효 전파/교육/수신 근거 없음 → `D4_ERR_AUTHORITY`/`D4_ERR_COMMIT_VALIDATION` |
| `DT_ERR_IP_AUTHORITY` | 허가·면허/특허 법적 범위/기간·정부 권한 위반 → `D4_ERR_AUTHORITY` |
| `DT_ERR_TECH_ADOPTION` | 지식·설비·자격/레시피/권한 없는 시설의 활성 요구 → `D4_ERR_TIMING`/`D4_ERR_AUTHORITY` |
| `DT_ERR_TIME_TRAVEL` | m 성과/교육을 이미 끝난 m 생산·전투/고용에 소급 → `D4_ERR_TIMING` |
| `DT_ERR_UNSUPPORTED_DISPATCH` | 상위 CE/D4 합의 없는 동월 비저장 실시간 에너지 허용 → `D4_ERR_RULESET_COMPAT` |
| `DT_ERR_EVENT_REPLAY` | 동종 연구/확산·훈련/가동 사건 중복 적용 → `D4_ERR_EVENT_REPLAY` |
| `DT_ERR_RULESET_COMPAT` | 스키마/유효숫자·RNG·계수/오래된 세이브 불일치 → `D4_ERR_RULESET_COMPAT` |

### 합격 수준

| Gate | 필수 입증 | 현재 |
|---|---|---|
| DT-DESIGN | T1~T5·25 불변/45 시나리오·35 ADR/기계판독 fixture·버전 정본 | **DESIGN CONFIRMED** |
| DT-L0 | 기존 POP/CE/CP/CM·D-D0/DC/DI·C-D4 정본/타입·월시점/수치·스키마 충돌 감사 | NOT_RUN |
| DT-L1 | KnowledgeRegistry·ResearchProject·Contact/Transfer/Patent/license refs·SiteTechniqueAssignment와 원장 어댑터 실제 C++ 구현 | NOT_RUN |
| DT-L2 | 연구/실험·검증·확산/교육·사업장 공정·POP/정치/군사 효과 기능 구현 | NOT_RUN |
| DT-L3 | DT-T01~45 및 DT-INV01~25 + DD0 24, DC 45, DI 45, 기존 D4 48/CE/CP/CM/POP 실제 자동회귀/결정론·롤백 | NOT_RUN |
| DT-L4 | 1,200개월·10만/100만 희소 POP/다수 연구소·기업/지식 전파, Windows/Linux·병렬/저장·크래시 실제 측정 | NOT_RUN |
| DT-COMPLETE | L0~L4 PASS 실측/로그/커밋·seed/규칙 해시·오류정보와 알려진 미지원 기능 명시 | NOT_RUN |

**정의 상태:** `45/45 시험 정의 완료; 0/45 실제 C++ 실행`.
