# E-G5 — GPU 래스터 기반 3D 지구본·LOD·동적 경계

> SPECIFIED / NOT IMPLEMENTED · E-D0/G1/G2/G3/G4 + C++20 engine. 그래픽 API나 프레임 성능의 실제 검증은 E-V.

## 1. 핵심 원칙

VIC2.5는 저사양에서 작동하는 3D 지구본. 실시간 세계 vector polygon triangulation/tessellation은 필수 경로로 쓰지 않는다. 정적 ProvinceId raster + terrain/height/landmask tiles, 동적 Province→country/admin/military/status 조회 LUT, GPU 색상·경계 표시를 분리. GPU 전용 데이터는 정확한 ProvinceId canonical ID/ownership의 복제 정본이 아니다.
벡터 자료는 데이터 제작 과정(off-line)에서 허용. 지구본 메시는 일반 구면/큐브 구면 메쉬이고 매 국가 변경마다 지도를 재메싱하지 않음. 라벨/강조 같은 일부 UI 선·문자 렌더러는 별도 가능하나 광역 경계 정본은 raster와 topology.

## 2. 프로젝션·타일 계약

기본 런타임 투영: cube-sphere 6-face quadtree. 전처리에서 위경도 canonical geography → cube face UV tiles 생성. face/LOD/x/y와 padding(경계 neighbor pixels)이 tile key. 정수 ID tile, terrain albedo, elevation, ocean/land mask은 서로 다른 레이어. cube face seam·극지 corner·subtile 경계 정합 테스트 필수.
구면 vs 지구타원체 모델은 renderer coordinate transform/terrain sampling contract로 버전 관리; gameplay 면적·거리 정본과 이미지 투영의 오차를 혼동하지 않음.
최초 prototype이 equirectangular texture sphere를 사용할 수 있으나, canonical tile manifest/schema는 cube-sphere target이고 equirectangular는 import format으로 처리. API 종속 graphics backend 미확정(OpenGL/Vulkan/D3D etc), C++ core와 render adapter 독립.

## 3. ID 래스터·GPU 색상

Canonical ProvinceId: stable 64-bit typed ID, 내부 dense mapping render_idx[ProvinceId]는 0 reserved / 무효·NoData; GPU integer texel에는 정수 index. 팔레트는 texture/storage buffer idx→ sovereign/admin/military/style index. 모든 표현 lookup은 snapshot/mapper revision과 호환되어야 함.
ID texture에서 linear/mipmap color blending 금지, nearest integer sample. Color and terrain texture는 filter 허용. 수량 한계가 GPU 32-bit index를 넘을 경우 render dataset import reject or explicit multi-page mapping (실제 ID 자체 truncate 금지).
LOD coarse ID raster는 섬/월경지 등 소형 Province가 사라질 수 있으므로 center/locator metadata+visibility threshold markers/edge overlay를 별도 관리하고 coarse raster를 canonical province membership으로 사용하지 않음.
정확한 picking은 ray-sphere(or terrain) intersection → canonical lon/lat → indexed highest-resolution authoritative ProvinceId lookup. 저해상도 displayed pixmap의 대표색/ID를 권위로 오인하지 않음. 클릭 중 streaming incomplete이면 pending/unknown 상태로 응답, 임의 주변 ID 대체 금지.

## 4. 경계 표현

BorderPair(a,b) static adjacency from G1. Sovereignty(a,t)!=Sovereignty(b,t) ⇒ national border; admin ancestor(at level) differs ⇒ admin border; military_control differs ⇒ occupied/control line candidate. G2가 role/unknown/joint state에 따라 styles/priority. 한 Province 소속 변경 시 its neighbor-border states만 갱신, 세계 전체 polygon redraw 금지.
경계 그리기 방법은 ID adjacency compare with pixel footprint + screen-space anti-aliased coverage, precomputed edge-distance coverage tile, 또는 hybrid. 시각적 두께는 px 단위로 제어하고 세부 축척에서 계단현상·corner join·cap·tile seam의 접속을 검증. 가는 국경을 최소 해상도에서 복원해야 할 경우 높은 LOD/사전 라인 coverage/fallback highlight, border의 단순한 픽셀 검사를 확정 물리 경계로 사용 금지.
서로 다른 관할/claim/unknown은 legend/patterns를 통해 구분; colors만으로 구별 불가능한 사용자용 patterns 고려.

## 5. 런타임 업데이트 경로

on WorldCommit(spatial_change_set):
A. snapshot/revision 검증; changed ProvinceIds unique sort.
B. affected authority/admin/scope country indices and relevant Province dynamic LUT entries build.
C. changed province and neighbors => border edge dirty range; derived country/admin caches revision update.
D. GPU upload sparse dynamic LUT modified ranges; 필요한 경우 border mask tile invalidation.
E. static terrain/province-id tiles unaffected. UI map displays only committed snapshot, 중간 stage pending state는 명시적 preview UI에서만 사용.
국가 변경만으로 전 세계 id raster regenerate 금지. 행정구역 parent 변경에서 수천 Province가 영향받을 수 있어 변화된 범위를 chunk/bitmap+batched GPU updates.

## 6. LOD·streaming·메모리

View-dependent tile quadtree: visible & near-future only, zoom/projection/viewport; deterministic priority from camera and fixed tiebreak, cache budget bounded LRU, asynchronous IO/decode separate from main render thread. GPU memory pressure detected => lower LOD/disable terrain complexity before map crash. Tiles have fixed max size and checksums; corrupt tile ≠ gameplay world corruption.
동작 품질 목표는 저사양 profile에서 부드러운 조작/지도 변경, 통합 벤치가 통계 P50/P95/P99 FPS/frame time, GPU VRAM, RAM, tile cache hit, allocations, out-of-core stalls를 기록. 구체 요구 FPS/VRAM은 목표 하드웨어 프로파일과 테스트 결과를 수집하여 E-V acceptance threshold에 사전 명시해야 함; 실제 검증 없이 60 FPS 달성을 주장하지 않음.
GPU 렌더링의 플랫폼마다 anti-aliasing 픽셀 경계는 일부 차이 가능. CPU 정본 ProvinceId/territory/hashes는 정확하게 동일해야 하고, 렌더 golden screenshot은 허용오차를 명시.

## 7. 디스플레이 레이어

Terrain, Political, Administration(level), Occupation, Population, Industry, Infrastructure, Trade overlay 등 시각 모드; 모든 파생 색상은 동일 ProvinceId/WorldSnapshot을 읽음. POP density/industrial production은 B/D 원본에서 derived UI scale cache만 읽음.
과거 시점 표시를 지원할 때는 역사 데이터 유효기간을 기준으로 렌더 snapshot 조회; 현재 게임 메모리의 state 변경이나 gameplay 타임라인 변경으로 간주하지 않음.

## 8. failure handling

Missing tile → 안전한 대체 평면/NoData 표시와 로딩 표시; 게임 state unchanged.
GPU device loss/context reset → render resources regenerate from asset dataset + committed snapshot, world commit 및 인구/금융 원본 no mutation.
Incorrect ID tile hash / palette index OOB → render asset validation reject, mislabelled Province에 잘못된 Country tooltip 제공 금지.
Seam/polar errors → dedicated G5 tests, 좌표조회 canonical index 우선.
Static raster ID tile source hash must equal compatible dataset hash; mismatches reject.

## 9. 렌더러 시험 샘플

G5-A01 camera roundtrip lon/lat↔cube face UV, ±180 seam and poles.
G5-A02 integer ProvinceId samples exact; bilinear mixing forbidden.
G5-A03 tiny island at zoom-out visible via locator, canonical select accurate.
G5-A04 transfer P100 country A→B: id tile digest unchanged, palette/border changed.
G5-A05 admin level switching never overwrites sovereignty.
G5-A06 control line vs legal border distinction.
G5-A07 unloaded/pending tile click never returns stale random province.
G5-A08 device loss + reinitialization world hash same.
G5-A09 no full-world texture recompute for single province edit: instrumentation dirty count equals bounded affected entries.
G5-A10 target hardware FPS/VRAM reproducible bench with explicit device/profile, status NOT_RUN before actual execution.
