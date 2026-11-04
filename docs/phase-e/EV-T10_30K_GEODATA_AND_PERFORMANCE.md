# EV-T10 — 역사 경계 오프라인 검사와 30,000개 프로빈스 성능 계약

> 2026-11-04 · phase-e-common/1.0 / E-G1·E-G5. 런타임 프로빈스 분할 금지, 실제 전 세계 고정 프로빈스 제작 및 정확성 검증은 별도.

## 1. 규모 결정

- 목표: **10,000~20,000개** 고정 프로빈스. 역사적 경계의 중요도가 높은 지역에 집중 배분. 개발·성능 스트레스는 **30,000개**에서도 수행.
- 30,000개는 게임 전체의 하드 상한이 아니다. 기초 ID는 안정적 uint64, 렌더러는 별도의 uint32 dense index를 사용하며 ID 재사용 금지.
- 기본 지리는 시대별 정치·행정경계와 해안·하천·지형을 참고해 오프라인에서 설계한다. 불명확한 19세기 국경을 임의로 '정밀한' 것으로 확정하지 않는다.
- 중요한 지방·독립 영토·월경지·좁은 회랑은 작게, 역사지리 변화가 적은 광역지대는 크게 구성한다. 10,000개라는 목표 수 때문에 분리돼야 하는 경계를 억지로 합치지 않음.

## 2. 실제 EV-T10 검사기

- scripts/ev10_history_polygon_import.py: 역사 GeoJSON Polygon/MultiPolygon에서 서로 다른 주권/행정주체가 실제로 공유하는 **내부 경계선만** 추출. 동일 주권 내부·해안선의 잘못된 주권경계 판정 금지.
- scripts/ev10_historical_overlay.py: 고정 Province 후보 GeoJSON에 대해 Shapely2 STRtree로 공간 인덱스 생성 후 역사적인 LineString과 겹치는 후보만 정밀 교차 검사. Province 내부에서 발견된 경계 길이를 WGS84 측지 거리로 산정해 원본 연도·출처·라이선스·정밀도와 함께 JSON으로 보고.
- geometry 오류/프로빈스 ID 중복/출처 누락/CRS84 경계 오류는 거부. ±180도 반자오선 비정규화 라인은 무작위 좌표를 생성하지 않고 별도의 검역 목록에 기록.
- default from_year=1801. 정확한 날짜 자료가 없는 스냅샷은 임의 일자로 세분화하지 않고 연도 스냅샷이라고 표시.
- 역사적 경계의 누락·오차를 **경고**할 뿐, 런타임에 ProvinceId·POP·금융·재고·주권을 수정하지 않는다.
- 테스트 입력은 현재 합성된 fixture임. 전체 전 세계 1801~현대의 실물 데이터가 공급되기 전에는 EV-T10 역사 정확성 최종 PASS 금지.

## 3. 사용 가능한 출처의 범위와 라이선스

- Natural Earth 10m 현대 기준선(공공영역): https://www.naturalearthdata.com/about/terms-of-use/ . 표시상 de facto 국가 경계를 기본값으로 함.
- geoBoundaries gbOpen ADM0~ADM5(국가마다 커버리지/연도/허가 및 속성 상이): https://www.geoboundaries.org/api.html . 속성별 라이선스/출처 메타데이터 확인.
- ETH CShapes 2.0(전세계 1886~2019, 유럽 1816~): https://icr.ethz.ch/data/cshapes/ . 1801~1885 전 세계 연속자료로 오인 금지.
- Aourednik Historical Basemaps (1815/1900/1914 등 날짜별 GeoJSON): https://github.com/aourednik/historical-basemaps . GPL-3.0, BORDERPRECISION 1~3. 문화/정치권역 혼합·중첩·경계 불확실성·제작한계가 있으므로 다른 독립 사료와 재검증 필수.
- 두 독립 출처의 충돌은 provenance에 남기고 임의 한쪽 선택 금지. 타인의 GeoJSON을 허가 없이 코드 저장소에 포함하지 않음.

## 4. 30k 저사양 런타임 구조

- include/vic25/phase_e_compact.hpp: Phase E/Phase C의 실제 원본에서 정적 ProvinceId와 CSR adjacency, uint32 GPU country/admin/military LUT를 **한 번** 컴파일. 런타임 월간/전투 편제에 독립된 두 번째 쓰기 가능한 국가 소유권 원장을 만들지 않음.
- 국경·행정변경이 일어난 ProvinceId 변경집합만 apply() 하고, 직접 연결된 접경 간선만 dirty 기록. 수만 개 프로빈스를 렌더 프레임마다 map 순회하거나 전세계 래스터 재작성하지 않음.
- 30k 압축 조회 배열 수용량의 목표 <8MiB. 이는 index/CSR/LUT만의 제한이며 POP·기업·월별 StagingWorld·정적 래스터 타일·GPU VRAM을 포함한 시스템 총메모리가 아님.
- FrameExchange: C++20 atomic shared_ptr 기반 불변 GPU LUT 스냅샷을 월별 커밋 후 갱신. 실제 월별 시뮬레이션을 별도 worker에서 수행하는 동안 UI/렌더러는 이전 커밋의 배열을 lock 없이 조회하여 동기 월별 계산의 UI 스톨을 피하는 기반을 제공. 실제 전체 렌더/UI 작업자 통합은 이후 별도 검증해야 함.
- bench/phase_e_30k.cpp: 10k/20k/30k 각각 seed와 CSR build, 두 달 실제 C/D/E 월별 staging, 10개 영토 변경과 border dirty, 600회 CPU 가상 프레임 조회/메모리 예산 계측.
- 이는 FPS 60 보장이 아님. GPU VRAM/FPS·실제 저사양 PC 프레임 지연 및 10~100만 POP/군사/기업 장기부하는 별도 검증해야 함.

## 5. 테스트와 인수 게이트

- CTest vic25_ev10_compact_10000/20000/30000: uint32 GPU 인덱스와 정치 LUT 및 CSR 크기·증분 갱신 검증. 실행시간/메모리는 CI 환경의 합성 참조 수치.
- CTest vic25_ev10_history_overlay: 합성 Polygon/LineString의 역사 경계 내부 통과 검출, 정렬·출처·동일 주권 판정, 후보 30k R-tree query 및 누락/중복/데이터 오류 거부.
- **EV-T10 진짜 수락** = 실제 10~20k 원본 고정 Province 데이터셋 + 라이선스/출처 명시 역사 국경 선형 데이터 + 시대별 지리적 정밀도 오라클/중복 경계 검증 + 의미있는 오차 누락 보고. 현 단계는 검사기를 구현했을 뿐 전 세계 역사 지리자료가 완비되지 않았음.
- **EV-T60** 역시 T10 역사자료 수락 전까지 BLOCKED. 본문 CTest 통과를 게임 전체 최종 인수로 승격하지 않는다.

## 6. 실행 예시

python -m pip install shapely==2.1.2 pyproj==3.7.2
python scripts/ev10_history_polygon_import.py --polygons history_1900.geojson --year 1900 --kind sovereignty --source 'source name' --source-url 'source URL' --license 'licence' --output historical_lines.geojson
python scripts/ev10_historical_overlay.py --provinces candidate_provinces.geojson --history-lines historical_lines.geojson --budget 30000 --output ev10_report.json

**현재 결과는 구현 코드의 실제 CI 실행 확인 후 기록한다.**

- 외부 실역사 참고자료 샘플: 핀 지정 GPL-3.0 Aourednik 역사 지도 1815와 1900에서 일시적 유럽 후보 및 상이한 주권 경계선을 추출·검사하는 script 제공. 원본 geometry는 저장소에 포함하지 않으며, 표본의 결과를 본편 10~20k 고정 프로빈스에 대한 증명으로 간주하지 않는다.
