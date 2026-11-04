# H-V6 — 저장/리플레이·결정론·장기 부하·성능·설명 검증

> phase-h-integration/1.0 · 2026-11-04 · SPECIFIED / NOT_RUN
> 선행 POP/C-D0/C-D4, D-D0/D-V, E-D0/E-V, G-D0/G-T, H-D0/D1~D7, H-V1~V5.
> 범위: worker1/N, 독립 저장·오류 복구, 1,200개월 외교·경제·전쟁 동시 진행, 희소 영향력/기구/위기 검색 및 플레이어 설명 일치.

## 1. 결정론의 확정 범위

- 동일 (contract versions, ruleset, WorldSnapshot, geography_dataset_id/hash, actor information_as_of, world_seed, command/event schedule)일 때 단일·멀티 worker 결과의 **정규 직렬화된 국가/POP/경제/군사·E 관할·G 무역·H 조약/조직/위기 원본 상태와 이벤트 순서·각 분야 ledger hash**는 비트 동일해야 한다.
- 모든 금액/인원/상품수량은 C-D0 정수/고정소수점·checked 128-bit 중간 산술을 따른다. float 누적 순서/비결정적 hash map 순회·OS별 난수나 실시간 clock로 조약 승인을 결정하는 것 금지.
- 분야별 사건/명령의 안정적 총정렬: stage+dependency topological order → 유효 국내 법/조약·승인/계약 우선순위 → stable typed EntityId/command_id/event_id. G 시장체결 우선순위와 C-P/C-M 하위단계의 기존 규칙이 상위다.
- 난수는 (world_seed, month_index, phase, stable entity_id, event_kind, draw_index)로 독립 분리. worker 수·분할·배치 순서에 따라 외교위기/기업·POP/군사 사건 추출이 달라지지 않는다.
- **예측값/신뢰도 계산**은 원본으로부터 결정론적이고, AI의 정보 접근성/비밀조약 누출 없는 정보셋을 고정한다. 경제·전쟁 예측 결과가 실제 사건과 다를 수 있으나 동일 입력의 예측해시 자체는 재현돼야 한다.
- UI 글꼴/화면 렌더링의 픽셀 동일성은 계산 상태 비트정확성과 별개. E의 GPU/래스터 색상·LOD 렌더러는 H 원장에 쓰기 접근 금지. 다만 표시 시 선택한 Country/Province ID 및 영향력/조직·위기 정보는 같은 snapshot에서 일관되어야 한다.

## 2. 저장 포맷과 계약 잠금

H save sidecar/state를 추가할 때에는 기존 C/D/E WorldSnapshot과 **동일 commit_id**의 세이브/저널이 되도록 묶는다. 개별 Phase가 따로 파일에 저장하고 월 인덱스가 다를 때 정상 복원으로 간주할 수 없다.

필수 저장 메타:
- save_schema_version, phase_h_common_version, phase_h_diplomacy_version, phase_h_integration_version, imported POP/C/D/E/G/F contract_versions 및 rule_version.
- world_seed, month_index, commit_id, world_snapshot_hash, geography_dataset_id/hash, cross-domain registry revisions, checked numeric policy/tick granularity.
- Country/Government/office/rule refs 및 H Recognition/Relation/Perception(필요시 secrecy scope), NegotiationCase/Offer, Treaty/Term/Obligation/Ratification, SovereigntyRelationship, Organization/Membership/Decision/Contribution, Crisis/Issue/Participation/Settlement, event lineage/tombstone.
- H-D6 영향력/결속력/위기 위험은 **파생 캐시**이므로 가능한 재계산하며 원본보다 우선 복원하지 않는다. 저장한다면 input snapshot/revision/ruleset/actor view를 함께 보존하고 mismatch 시 폐기한다.
- event journal offset/last_commit/checksum, source_event_id 및 H term/obligation→G customs/C-M conflict/E sovereignty/Financial transaction ID 인과 links.
- 법률/조약 유효기간과 이미 미지급된 조약 의무·미완결 국제위기/조직탈퇴 등 pending state, 미래 효력 예약.

복원 오라클: 정상 저장 후 즉시 로드하면 **정규화 canonical world hash 동일**, 그 뒤 동일 명령/seed 120개월을 진행해 저장 없는 연속 실행과 모든 분야/사건 해시 동일. H가 없던 옛 세이브는 명시적으로 지원한 migration이면 신규 ID/법적 자료 생성 규칙+provenance가 있어야 하며, 임의로 모든 국가를 무동맹·무영향력로 단정 초기화해서 정상 세이브 호환이라고 주장하지 않는다.

- 버전/지역 데이터셋/조약 참조가 비호환이면 명시적 Unsupported/MigrationRequired/CorruptSaveReject 반환. 손상된 파일·불일치된 stage+ledger 부분 로드 성공 금지.
- 저널의 중복 이벤트 replay는 idempotent, 완료월에 부분 적용된 기록을 재적용해 관세/주권/배상금이 두 번 늘어나지 않음.
- 전원차단(crash) 복구는 **완료된 전원장 원자 commit**만 인정. 기록중단은 이전 마지막 완결 commit으로 되돌리거나 검증된 WAL/transaction replay를 수행한다. 단순 파일 load/save 성공만으로 crash recovery DONE이 아니다.

## 3. Fault injection과 종료 원자성

다음 중 하나 이상을 stage0~stage11 각 단계의 안정적 이벤트 순서에서 주입한 뒤 precommit committed_world hash·next_entity_id·month_index·last_commit_id·각 분야 원장/예약/조약 term 적용 키·국제기구 투표/결속·위기 source refs가 동일한지 검증:

1. stage0 두 국가가 같은 Province/기간·역할 sovereignty로 확정하려는 복수 조항.
2. stage1 같은 정부자금·기업 재고·군수 선적을 G 무역, H 외교원조, D 시공이 동시에 이중예약.
3. stage2 외교사절 1명을 다른 직장·군 현역/조직 사무국에 중복 배정.
4. stage3 C-E5 TradeAgreement와 H TreatyTerm이 같은 세관에서 중복 면세/과세를 시도.
5. stage5 국제협력체 분담금 지급 100 예약했으나 실제 자금 부족/이중 지급.
6. stage7 유효 통상조약의 외국 수입화물이 운송중인데 목적지 생산·가계가 사용하려는 시도.
7. stage8 실제 외상/배상 약정100·지급20을 수취100으로 중복 반영하려는 시도.
8. stage9 서명/비준권한 없는 정부/종속국·해산된 의회의 승인; 조건부 합의 DAG cycle.
9. stage10 승인 없는 신규 참전/가짜 군부대·피해 POP 중복, 점령국을 E 주권국으로 즉시 바꾸려는 시도.
10. stage11 orphan TreatyTerm/OrganizationMembership/CrisisIssue ref, 잘못된 revision·카운트·journal checksum 주입.

법적으로 잘못된 신청은 **정상 REJECT**이므로 월 전체 실패를 강제할 필요 없이 해당 명령의 거부와 자원예약 해제만 확인한다. 이미 staging 내부 상태를 훼손하는 invariant fault 또는 계약상 all-or-nothing 필수 패키지 실패 후 부분 이벤트 커밋이 감지되면 **AbortMonth**. 해당 시험이 어떤 모드를 기대하는지 case마다 선언한다.

Fault injection 성공의 유일 기준은 '예외 발생'이 아니라 원본 world/원장/ID/이벤트/외교·정보 뷰가 영향 없이 유지/명시 오류에 맞게 복구되는 것이다.

## 4. 장기 시나리오 및 실제 성능 프로파일

**실행 기간:** 최소 1,200개월(100년), 각 시나리오 2회 같은 시드 및 별도 시드 충격 시뮬레이션. 월별 모든 HV-INV36 + 상위 불변식 검사. 동등 입력이면 worker1/N·save/resume 결과가 같아야 한다.

계층형 synthetic profile (시험 전 baseline 기록, 실측 조건에 맞게 버전 있는 profile 조정):
- HV-P0 smoke: 4개국, 8개 Province, 4 Market, 1 Organization, 2 Treaty, 1 Crisis, 20 POP slices(숫자 예시). 계약·비준/관세/조직/위기 최소 경로 검증.
- HV-P1 small integration: 20개국, 100 Province, 20 Market, 15 InternationalOrganizations, 200 live Treaties, 20 active Crisis/Negotiation, 10,000 POP slices, 1,000 Firm. 한 월에 복수 조약·권한·수송/시장 충격.
- HV-P2 reference: 200개국, 10,000 Province, 500 Market, 80 Organizations, 5,000 active/expired Treaties, 100 simultaneous Crisis/Negotiation, 100,000 POP slices, 10,000 Firm. 실제 연결된 분야의 트랜잭션/원장·영향력 read-only/cache profiling.
- HV-P3 stress: 200+ 국가(실제 테스트에 명시), 10,000 Province, 1,000,000 POP slices, 100,000 Firm, 20,000 TreatyTerms, 200 concurrent Crisis/Negotiation 등 고부하. 메모리/성능·event memory/원장 정합은 별도 실측.

**이 숫자는 성능 프로파일의 제안 범위이며 구현된 실제 인구/국가 규모·성능 측정값이 아님.** 기술·역사시나리오가 다른 실제 운영부하에서 분포가 달라질 수 있으므로 먼저 현실적인 자료 특성을 기록한다.

필수 시나리오:
- 1801년/19세기 보호·속국/왕조조약, 느린 외교서신/통상권 변경·국가 합병/독립 시기별 규칙.
- 국제협력체 설립·자격정지·동시 군사/경제조직 회원권 및 회비/기금.
- 무역/산업/금융 의존 → 제재·전쟁/봉쇄 → 기업/POP 가격·부채/군사 피해 → 외교적 중재/평화·경제 재건.
- 반복 국제위기(중립/중재·부분합의/장기동결·재발), 전쟁이 없는 위기도 포함.
- 장기 국경·주권/행정·시장/관세동맹 이동으로 Province·국가 계보/원장 정합 유지.
- Save at arbitrary committed month → load/resume, partial journal corruption, worker1 vs N, multiple seed runs.

### 계측 항목

측정마다 실제 CPU 모델, RAM, OS, 컴파일러/옵션, 스레드 수, target build commit/contract hashes, sample seed, country/Province/POP/firm/organization/treaty/crisis 수, 월별 명령·이벤트/거래·세관 수 및 소요시간을 기록.

- stage별 평균/P50/P95/P99 wall time와 CPU time, 전체 월 처리량·성능 회귀, 최대 RSS/heap, snapshot/journal size, save/load/replay time.
- H 관계/조약의 조회·ratification/term conflict 처리시간, H-D6 changed-set influence cache hits/misses, 조직 표결/기금 처리, H7 위기 참가·협상 평가, G/C-E5/E 주권 접점 병목.
- baseline과 비교: 변경된 국가쌍/조약·시장/권한 주변만 재계산하는 희소 인덱스/원본 revision 기반 invalidate의 효율; 성능 최적화(A→B→C에서 불필요한 파생 조회 생략) 시 **실제 세계·설명 결과 완전 동등**.
- **성능 수용 기준의 숫자는 시험 대상 하드웨어·목표 FPS/턴 처리시간·메모리/세이브 용량을 실측 전에 합의해 잠금**. 결과를 본 뒤 허용시간을 올리는 사후 조정만으로 PASS 금지.
- 위기/외교 이벤트가 없는데 모든 국가쌍×모든 상품×모든 Province를 매월 전수 재계산하는 방식은 profiler에서 원인을 확인하고 개선해야 한다.

## 5. UI·국가 AI 설명·비밀정보

동일 스냅샷/정보권한에서 툴팁/상세 화면·AI 평가가 **상황을 설명할 수 있는 근거·예측·실현 구분**을 제공해야 한다.

- 통상조약: "대상 철 관세 10% → 0%"는 법적 확정 효과, "기업이익·POP 물가·무역의존도"는 다른 조건/실제 거래에 따른 예상/실현. 즉시 기업이익 +10이나 영향력 +10 확정표시 금지.
- 국제협력체: 공동기금 잔액과 약정 미납, 경제협력 회원/동맹가입·실제 국가/군사 참전, 지도권/의결권의 법적 근거 구분.
- 영향력: 경제·군사·정치·문화별 근거 출처, A→B/B→A, 대체공급망/다른 동맹·국내 여론, 정보 미상/불확실성. '세력권 편입' 버튼 없음.
- 국제위기: 국제화된 분쟁근거, 쟁점별 중립/중재·지원 약속과 실제 군사/금융행동, 전쟁이 없는 해결/동결 경로, 법적 최후통첩 기한/추정 위험 구분.
- 비밀조항과 미도착 외교서신: 사용자가 볼 수 없는 정보는 AI 정책/툴팁에 누출하지 않고, 공적으로 확인한 정보와 미상·추정 뷰 분리.
- 터치/키보드 포커스와 hover 모두에서 사용 가능한 설명 UI, 접근성 체크리스트와 번역/단위·시점 표기.

검증: HV-T64..72, HV-INV31..36, 104개 기존 외교 인수 + 기저 POP/C-D4/D-V/E-V/G/V 테스트. **현재 문서 명세만이며 C++ 테스트나 성능 실측은 이루어지지 않았다.**
