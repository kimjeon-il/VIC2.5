# EV-T10 — 역사 경계 기반 10,000~20,000개 프로빈스 / 30,000개 성능 상한 설계

> 2026-11-04 · DESIGN_SPECIFIED / IMPLEMENTATION_DEFERRED / EV-T10 BLOCKED
> E-D0 / E-G1~G5 / Phase B,C,D 원장 계약을 유지한다. 실제 역사 GIS 자료 생성, 프로빈스 분할 및 30k 실측은 이번 작업에 포함되지 않는다.

## 1. 목표와 비목표

- **실제 제작 목표: 10,000~20,000 Province**. 역사적 국경·행정 경계·중요 지역의 변동을 반영해 지역별 가변 밀도. 수량 맞추기용 기계적 균등 분할 금지.
- **설계·부하 시험 목표: 30,000 Province**. 30k까지 정적 조회·지구본 조작·월별 경제 계산이 불필요하게 전 세계 프로빈스를 반복 순회하지 않도록 설계. '렉 없음'은 목표 장비·실제 부하에서 측정한 경우에만 주장.
- 30,000은 런타임 고정 배열의 크기나 하드코딩 상한이 아님. 버전 관리된 dataset manifest의 검증 정책 상한이며, 초과시 사전 벤치/스키마·GPU 정수 인덱스 용량 검증 후 확장.
- 시뮬레이션 중 ProvinceId/형상은 불변. 동적 국가·행정구역·점령·선거구는 소속·관할의 유효기간 관계로 표현. 시나리오별 1801년 이후 역사적 경계 재현이 우선, 필요하면 예외/불확실성을 명시.

## 2. 프로빈스 설계 우선순위

1. 1801년~게임 지원 최종시점의 **국가 주권 국경**과 중대한 영토교환·독립·병합.
2. 국가별 1·2급 행정구역, 영토 관리·지방정부 경계, 시나리오에서 재편 가능해야 하는 구획.
3. 전쟁·외교·산업·물류에 의미 있는 해협·회랑·월경지·도서·주요 지형 장벽.
4. POP/기업/자원·교통의 지리적 해상도와 지도 최대 확대에서의 식별 가능성.
5. 선거구는 보조 우선순위. 프로빈스 내부의 선거구는 E-G4의 **인구 분할**로 표현할 수 있으나 실제 지도 경계인 척 표시하지 않음.

우선순위는 출처 품질·실제 지리 면적·경계 중요도를 함께 평가하며 임의의 인구수나 미확인 국경을 생성하지 않는다. 모든 경계 후보에는 source_id, valid_from, valid_until_excl, boundary_kind, confidence, geometry_digest, license를 부여한다.

## 3. 오프라인 제작 파이프라인

1. 입력 레이어별 CRS/지오메트리/유효기간·출처 정규화. antimeridian·극점·다중 폴리곤·내륙수면을 별도 취급.
2. 시대별 경계선 union을 구축하되 정확히 일치하지 않는 중첩선·스냅 허용오차·sliver는 무조건 검토 목록에 기록. 좌표 반올림으로 국경을 임의 변경하지 않음.
3. 교차선으로 만들어지는 후보 셀의 topology·면적·경계 접촉을 검증. 역사적으로 의미 없는 미세 변화는 별도 omission record로 남기고 제거 가능.
4. 재현 중요도와 공간 비용을 평가해 10k~20k 후보 집합을 생성. 30k를 초과하면 임의 병합하지 말고 충돌 목록과 지역별 후보 수량을 제출.
5. 전체 육지 coverage 정확히 1개 ProvinceId, 수역/미확정 지역 explicit NoData/SeaZone, 인접 그래프 양방향 대칭 및 corner-only 제외.
6. 안정적인 64-bit ProvinceId와 dataset hash·형상 checksum·면적·대표점·bbox·source provenance 확정.
7. CPU authoritative coordinate→ProvinceId index, 압축 정적 adjacency, GPU용 dense integer render index + cube-sphere tile manifest를 **오프라인에서 한 번** 컴파일.
8. 경계 시기별 historical replay: 각 사건이 고정 Province 집합으로 재현 가능한지 검사. 불가능한 사건은 mismatch 목록으로 보존하고 재현율 보고.
9. 최종 dataset은 read-only memory-mapped/압축 블록 단위로 로딩; 세이브에는 dataset hash 및 ProvinceId 관계를 기록하고 다른 데이터셋에 무단 재매핑 금지.

## 4. 데이터 레이아웃과 조회 복잡도

| 계층 | 정본·자료형 | 성능 규칙 |
|---|---|---|
| Province Registry | 64-bit stable ID, geometry hash, area, bbox, representative point | 초기 로드 시 dense id→index 구축; 런타임 O(1) 기대 조회 |
| Adjacency | 정렬된 CSR offsets+neighbor indices, border edge IDs | 전체 O(P+E) 저장; 변경은 정적 그래프 수정 0 |
| Sovereignty/Control | Phase C 원본 주권·군사통제 | E/G5는 snapshot 기반 읽기 전용 dense palette/cache |
| Administration | Province→leaf, leaf→parent, 시간별 관계 | 재편 시 변경된 Province/하위 트리만 invalidate |
| POP/Facility | Phase B/C/D 기존 원장에 Province/Region binding | 프로빈스 수와 POP 셀 수를 동일시하지 않음; 매월 30k 전체 POP 재분배 금지 |
| Render ID | 32-bit dense GPU integer index, 0=NoData | nearest integer sampling, static tile 재생성 금지 |
| Borders | 정적 접촉 edge + dynamic owner/status lookup | 변경 Province와 인접 edge만 dirty; 국가별 전세계 mesh rebuild 금지 |
| Picking | authoritative 최고 정밀 공간 index | coarse LOD 색상/대표 ID로 정본 선택 금지 |

- 정적 adjacency는 sparse CSR; 모든 프로빈스 쌍의 P×P 행렬 금지.
- 정적 geometry는 GPU에 세계 전체 벡터 polygon을 상주시킬 필요 없음. CPU/파일에는 원본 정확도 유지, GPU에는 화면에 필요한 래스터 tile만 상주.
- 원본 주권·POP·돈·재고를 E 렌더러에 writable copy로 복제하지 않음.
- 월간 전체 스냅샷 복사가 30k와 POP 1m에서 병목이 되는지 계측하고, 필요하면 immutable snapshot + copy-on-write/변경원장으로 단계적 전환. 변경 전후 해시·회계 동일성 필수.

## 5. 30k 성능 계약: 가설과 인수 분리

**프로파일을 테스트 전에 고정**: 저사양 PC 1종, 기준 PC 1종, OS/CPU/GPU/드라이버/RAM/해상도, 카메라 이동 경로, POP 셀 수·기업·시설·시장·군대·사건 수를 모두 명시.

잠정 UX 목표(검증 전 미확정):
- 일반적인 지구본 회전·확대: 저사양 **30 FPS 이상**, 기준 장치 **60 FPS 목표**. 평균 FPS뿐 아니라 P95/P99 frame time, 입력→표시 지연, tile miss/IO stall을 측정.
- 평시 1개월 진행: POP 10만·100만 셀 각각 P50/P95/P99, 최대 RSS, allocations, domain별 CPU 시간을 별도 기록. 목표 상한은 실측 기준선과 실제 플레이 허용치로 사전 확정.
- 지리 변경 1개/100개/3,000개 Province, 행정 reparent 1개/대규모 subtree에서 dirty GPU LUT range, affected borders, cache invalidation 수를 계측. 정적 ID tile regeneration **0**.
- 10k/20k/30k 프로빈스 × POP 100k/1m × 기업/시설/군대 부하의 교차 매트릭스를 최소 3회 독립 실행. 30k를 100k 프로빈스 합성 부하 통과로 자동 승인하지 않음.
- GPU VRAM/CPU RAM peak, render p95/p99, 월별 stage time, save/reload, 1/4 worker deterministic hashes, 1,200개월 장기 시뮬레이션을 각각 검사.
- 화면 타일/지도 모드는 LOD fallback·bounded LRU·memory budget. 메모리 압박 시 LOD/지형 품질을 우선 낮추되 게임 정본을 변경하지 않음.

2026-11-04 참고용 기존 CI 합성 결과: 10k Province/100k POP Linux 월 P50 342.403ms, peak RSS 79.387MiB; 100k Province/1m POP Linux P50 4052.010ms, peak 756.938MiB. 이것은 30k 실제 세계·GPU FPS 인증이 아님.

## 6. 국경 정확도와 수량 배분 평가

- 사건별: supported / partial / unsupported / uncertain, 역사적 날짜·원본 지도 출처·지리 오차·누락 이유 기록.
- 지역별: historical border length weighted coverage, event importance weighted coverage, 행정 경계 coverage, 작은 도서·월경지 보존율, candidate Province 수량과 데이터 품질.
- '전체 사건 중 몇 %' 단일 지표로 정확도를 숨기지 않음. 작은 중요 영토를 놓친 경우 별도 critical mismatch로 판정.
- 우선 10k 후보를 평가한 후 15k/20k 후보와 정밀도 증가량·메모리/렌더 비용을 비교하고, 필요한 지역만 30k 이내에서 선택적으로 세분화.
- 세이브 호환성: 후보 지도 변경은 새 dataset_id로 구분, 명시적 1:N/N:1 Province migration plan 없으면 구세이브 로딩 거부.

## 7. EV-T10 및 추가 성능 시험 계획

- EV-T10-A: 1801년 이후 서로 다른 시점의 실제 경계가 고정 Province 내부를 횡단하는 fixture를 만들고 정확한 ID·시점·source/area/length diagnostic 검증.
- EV-T10-B: 작은 월경지·해협·섬·분리 영토·극지·180° seam·호수·corner-only 인접/중복/틈 검사.
- EV-T10-C: 10k/20k/30k 후보로 같은 역사 사건 목록을 재생하고 지원/미지원 판정 및 데이터 출처의 재현성 비교.
- EV-T10-D: 30k Province의 static CSR·GPU integer ID raster·tile LRU·sparse owner LUT 갱신·저장/복구·월별 POP/금융 불변 검사.
- EV-T10-E: 실제 저사양·기준 GPU에서 지도 회전·확대·경계 변경 프레임타임 P50/P95/P99, CPU/GPU 메모리 및 입력 지연.
- EV-T10-F: POP 100k/1m, 30k Province, 1,200개월 복합 시나리오·1/4 worker replay·월별 원장 해시 검증.
- **현재 판정: DESIGN_SPECIFIED / EV-T10 BLOCKED.** 이 문서는 성능 구조와 향후 인수 오라클을 확정할 뿐 실제 30k 세계 지리·저사양 테스트를 실행하지 않는다. EV-T60도 BLOCKED 유지.
