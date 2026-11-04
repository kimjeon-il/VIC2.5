# E-G1 — 고정 프로빈스·정적 지리·공간 인접성

> 상태: SPECIFIED / NOT IMPLEMENTED · 상위 E-D0 phase-e-common/1.0

## 1. 책임·비책임

G1은 ProvinceId 기반 정적 육상 공간, 해역, 지형·면적, 좌표-프로빈스 매핑, 지리 접촉 및 인접 그래프, 지리 데이터 버전/출처만 소유. POP/시설/자원 재고/행정 소속/주권/군사교전/운송용량은 소유하지 않는다. G2~G5는 read-only G1 API를 사용.

## 2. ProvinceDefinition

ProvinceDefinition { province_id:ProvinceId, dataset_id:DatasetId, area_m2:checked-u64, representative_lon_lat:int-fixedpoint, bbox_parts:[GeoBounds], static_terrain_profile_id, coastline_contact_refs, geometry_blob_digest, display_alias_metadata, revision_static }.
- 게임 중 ProvinceId 안정, ID 0 invalid, 폐지·증설 불가. 다중 부분형상과 180도 경도 횡단 bbox_parts 지원.
- canonical geometry는 오프라인 전처리에서 확정; 런타임은 raster lookup + topology + 공간 index를 읽는다. 지형/고도는 한 픽셀의 높이값만이 아니라 별도 terrain/elevation raster로 관리.
- 면적은 측지 계산 결과를 사용하고 위경도 raster 픽셀 수×상수로 정하지 않는다. 정밀도·단위와 source_provenance 유지.
- 동적 land use, 인구밀도, 시설점유와 resource deposit은 Phase D 및 후속 환경/경제 시스템의 원본.

## 3. 지리 topology

Graph nodes: ProvinceId, SeaZoneId (node-kind 구별). Edges:
- LandBoundaryContact(a,b,shared_length_m,boundary_id). 하나의 점에서만 만나는 경우 접경 간선 아님.
- CoastContact(province,sea_zone,coast_length_m), 하나의 해안에 복수 해역 가능.
- WaterCrossing/TransportConnection은 기초 지리 접촉과 분리된 별도 edge/connection overlay. 통행량/관세/수송서비스는 Phase F 및 C-M 담당.
- GeographicBarrier는 산지·경사·강 등의 물리 제약 사실/품질을 제공하되 통행 가능 여부 결정은 기술/군사/물류의 규칙과 연결.
- 방향성은 지리 접촉에 필요하지 않으나 이동간선은 방향성 가능. 인접 그래프 대칭·정렬·중복없음·참조 무결성 검증.
- enclaves, islands, disconnected/multipart provinces, polar caps, antimeridian seam, lakes and inland seas의 지리 구분 지원.
- SeaZone은 일반 Population residence region이나 행정주체가 아님; 해상경계·영해는 E-G2/H에서 확장.

## 4. 프로빈스 설정 원칙

전 세계 동일 면적 셀을 강제하지 않음. 국경·도시·산업·인구·군사상 중요도가 높은 지역은 정밀, 광역 비거주 지역은 상대적으로 크게 설계 가능. 단, Province 구획은 모든 지원 시나리오 기간에 걸쳐 동일. 역사적 경계가 고정 Province 내부를 횡단하면 완전한 재현 불가능함을 import warning으로 보고하고, 프로빈스를 자동 runtime 분할하지 않음.
기본 행정구역·선거구가 대체로 프로빈스 전체의 집합으로 표현 가능한지 검증; 필요한 경우 초기 지리 설계에서 더 작은 Province를 정의. 목표 수량/최소 면적은 임의 확정하지 않고 목표 역사 기간·저사양 성능 실측/데이터 품질로 산정.

## 5. 오프라인 컴파일 파이프라인

A. 소스 자산 취득: 각 자료의 라이선스·정확도·제작일·공간기준·형상 정밀도 저장.
B. 규칙 기반 normalize: 좌표계 통일, anti-meridian split, polygon winding/topology repair, invalid feature quarantine, 중복·누락 로그. 불확실 경계 임의 발명 금지.
C. ProvinceRegistry 구축: 고유 ID 발급, multiparts 확인, 정적 면적·대표좌표·경계 bbox.
D. coverage validate: 육지 대상 coverage 정확히 하나의 ProvinceId; 미분류는 explicit Unmapped/NoData 정책으로 따로 검출. 겹침과 틈은 침묵 보정 금지.
E. canonical topology create: true shared boundary check; corner-only는 reject; 교량/해협의 특별 연결은 의미 구분.
F. 공간 index compile: coordinate-to-id lookup용 원본 resolution tile/index + bbox·edge index; gameplay ID와 화면 GPU packed index 변환 별도.
G. G5 렌더 파일 export: cube-sphere quadtree int-ID raster, terrain/height/landmask mips, 작은 섬·월경지 보조표시 메타.
H. SHA-256 등 content hash, tile-manifest/size/endianness/checksum, deterministic sort, import report.

## 6. API

GetProvince(ProvinceId), LocateLandProvince(lon,lat,canonical_dataset), GetLandNeighbors(ProvinceId), GetCoastNeighbors(ProvinceId), GetSeaZone(coord), HasLandBorder(a,b), GetStaticTerrain(province), GetDatasetProvenance(), ResolveRegionBinding(RegionId,scenario_id).
모든 조회는 immutable dataset_id+digest를 암묵/명시 참조; region binding의 의미는 Phase E 시나리오에 국한. 임의 Region을 자동 1:N 프로빈스로 분해하지 않음.

## 7. 결정론·성능·오류

- Dataset 변환은 버전 고정, stable sorted ID, 명시적 seam/overlap tie rule, 빌드 해시; 모호한 픽셀을 랜덤 할당하지 않음.
- 런타임 정적 topology 재구성 금지; 전역 지형 배열 재계산 금지. 공간 index와 tile cache는 memory-mapped/지연 로드 가능.
- 지오메트리 데이터 손상, 빠진 Province/SeaZone ref, 틈·겹침, 비대칭 adjacency, 부적합 static geometry hash는 import 거부.
- 미지원 역사 경계·해상 영유권은 데이터 정확도/기능 지원 경고이지 임의 geometry 편집 명령으로 대체하지 않음.

## 8. 수락 표적

G1-A01 인접 프로빈스 양쪽 경계 대칭, shared_length 합계 일관.
G1-A02 corner touch는 육상 인접 아님.
G1-A03 해안/해협/섬의 접촉과 물리적 연결 분리.
G1-A04 ±180도 경도 및 극지 좌표의 정본 lookup.
G1-A05 다중 부분형상 및 내부 호수/NoData.
G1-A06 ProvinceId 안정/수정 불가, 세이브 재연결.
G1-A07 입력 자료 겹침/틈·유효성 오류 검출.
G1-A08 10만/100만 POP과 무관하게 static adjacency 내용 동일.
G1-A09 원본 자료 SHA 및 license/provenance manifest.
G1-A10 고정 province로 재현 불가능한 역사 경계의 명시적 판정.
