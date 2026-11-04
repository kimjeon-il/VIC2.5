# J-N1 — 자연환경·기후·토양·수계 상세 설계

> phase-j-nature/1.0 · SPECIFIED / NOT IMPLEMENTED · 상위: J-D0, E-D0/E-G1, D-I2, POP/C-E

## N1-1. 공간 범위와 책임
기본 육상 단위는 Phase E의 안정적 ProvinceId(최대 30,000개 성능 표적)이며, 연안/해역은 SeaZoneId와 접촉관계만 참조한다. 실제 역사 시나리오의 POP RegionId는 E-D0의 1:1 RegionProvinceBinding을 따른다. S0의 무지도 Region은 독립 fixture로 허용한다.
- E: 프로빈스 형상·지형/고도·공간 인접·해안/수계 topology·지리 데이터 hash.
- J-N1: 자연환경 배경 데이터(기후대·강수/기온의 계절패턴·생태/토양/수분/유역 특성)와 동적 환경 상태.
- D/C: 농업/건설/산업의 실물 생산·시설·상품·가격. J가 생산량/재고를 직접 생성하지 않는다.

## N1-2. 최소 상태
| 객체/필드 | 의미·단위 | 소유 |
|---|---|---|
| EnvironmentalProfile(profile_id, province_id, geo_dataset_id, climate_class, elevation/terrain_refs, soil_class, hydrological_refs, biome, source_precision) | 정적/준정적 지리 및 기후 성격, 대량 래스터는 별도 읽기 전용 데이터셋 | J가 환경 보조 레이어, E의 형상·고도 원본 재사용 |
| MonthlyClimateContext(province_id, month, precipitation_mm, mean_temp_tenths_c, evapotranspiration_mm, snowpack_proxy, drought_indicator, uncertainty) | 계절성 월간 관측/시나리오 입력, 강수·온도 직접 연산 간소화 | J |
| EnvironmentalState(province_id, soil_moisture_index, surface_water_index, soil_fertility_index, vegetation_index, changed_month, revision, event_refs) | 현재 지역 조건. 무조건 모든 항목을 정밀 관측치로 취급하지 않음 | J |
| LandUseView(province_id, use_categories, share_basis, ownership/site_refs) | 실제 농지·시설·산림/도시 토지 이용을 D/E/Ownership으로부터 집계 | 읽기 전용 |
| EnvironmentalConditionEvent(event_id, province_id, kind, input_refs, delta, month, source_hash) | 강수·가뭄/토양·식생 등 변화 이력 | J |

0과 미상(unknown)을 별도 tagged field로 둔다. 시계열 자료가 부족하면 기후대+계절로 생성한 **모형 추정값**임을 보존하며 역사 관측값으로 오인하지 않도록 precision/source_kind 표시.

## N1-3. 월별 변화 규칙
1. stage0에서 정적 기후 profile과 계절·전월말 환경 상태를 근거로 해당 월의 **운영에 유효한 기상/자연 조건**을 고정한다. 이미 진행된 생산을 월중 재계산하지 않는다.
2. 토양수분 = clamp(지난달 수분 + 유효 강수/유입 − 증발산 − 실제 농업/산업 취수 − 배수/유출, 규칙상 물리 범위). 모든 입력은 명시 단위로 환산하며 토양수분지수 등 추상 지표와 실물 수량 단위를 섞지 않는다.
3. 농업 성과에 적용할 환경 제약/수익성은 토양·계절·수분·작물/기술의 상호 작용 함수로 전달. 실제 재배/수확은 C-E1/D-I2가 실행한다. 한 가뭄이 J-N1과 J-D1에서 **별도 동일 생산 손실을 중복** 발생시키지 않는다.
4. 수계·유역 영향은 E의 실제 인접/연결 정보와 방향성 있는 수문 참조가 있을 때만 구현. 접경만으로 강물 흐름을 가정하지 않는다.
5. 식생 변화/산림 복구는 J-R1 RenewablePool 및 실제 벌채/식재/기후에 연결. E의 정적 지형이나 국가 경계 형상 변경으로 표현하지 않는다.
6. 지속적 기후 변화가 필요하다면 규칙 기반 느린 외생 추세/시나리오 데이터로 취급하고 별도의 전 지구 대기/해양 수치모형은 필수 범위에 넣지 않는다.
7. 계절 계산은 게임 역법 MonthIndex에 매핑. 남·북반구 차이, 열대 건기/우기, 고지대, 극지 등의 단순 계절 지연을 데이터로 지원. 전국 동일 농번기나 비율 강제 금지.

## N1-4. 단계·인터페이스
- stage0 QueryMonthlyEnvironment(province, month, snapshot): 기후·시설·농업에 사용할 유효 제약 확정.
- stage1~4: 건설/농업/산업/자원·물사용 계획과 실제 생산. J는 읽기 전용 제약/수량 확인 근거 제공; 생산은 C/D에서 작성.
- stage9: 차월 관개·배수/보전 등 합법 투자/토지관리 변경 예약.
- stage10: 실제 취수·오염·재해/복구·생장/서식지 변화와 월말 자연조건 상태 갱신.
- stage11: 강수·수분/토양·단위/자료 정밀도 및 C/D 사건을 대조하여 원자 커밋.

APIs: GetStaticEnvironment(ProvinceId, dataset_hash), GetMonthlyClimate(ProvinceId, MonthIndex, snapshot), ResolveAgriculturalConstraints(site, crop, month, snapshot), ResolveConstructionWeather(site, month), SubmitEnvironmentalStateChange(cause_event, region, delta, stage10), ExplainNatureImpact(site, period).

## N1-5. 성능/오류
정적 기후·환경 레이어는 프로빈스별/월별 인덱스로 미리 집계, 변경 없으면 재연산 금지. 동적 상태는 SoA/희소 변경집합. 환경 갱신·접근은 Province 단위 O(P) 이하를 기준으로 두고, POP 셀×프로빈스 교차결합을 금지한다. 참조 없는 유역/불일치 데이터 hash/잘못된 단위는 reject.

## N1-6. 필수 시험 J-N1-T01~T10
T01 남/북반구 반대 계절; T02 열대 우기/건기; T03 unknown vs 0 강수; T04 지형 E 정본 불변; T05 농업 시점별 가뭄 제약; T06 Stage10 새 홍수로 Stage4 기생산 소급 수정 금지; T07 수계 방향이 잘못된 경우 흐름 거부; T08 산림·토지이용 소유권 분리; T09 30k Province 정적/동적 캐시 일치; T10 저장·복구/결정론/환경 이벤트 idempotency.

**상태:** 상세 설계 확정, 기상/수문 C++ 엔진 미구현.
