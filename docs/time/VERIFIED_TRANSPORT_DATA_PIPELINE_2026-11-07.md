# 검증된 세계 교통망 데이터 입력 계약 (2026-11-07)

**구현**: `scripts/build_verified_transport_links.py` + `include/vic25/verified_transport_seed.hpp` + Phase E/G 실물 교통망. 사용자가 요구한 전 세계 육상·철도·해상·하천·항공 운송을 **실측 출처로 제한해 입력할 수 있는 체계**다.

> **현재 상태:** 엔진·입력 파이프라인 코드만 작업 브랜치에 구현됐다. **세계 전체의 실제 교통로·지리 연결·역사별 운송시간·운송량은 아직 적재하지 않았고, 빌드/검사도 실행하지 않았다.** 출처가 없는 구간의 링크, 일수, 용량, 운임, 항공편을 자동 생성하지 않는다.

## 공개 원본

1. **OSM Planet / Geofabrik**: [전 세계 원본 설명](https://wiki.openstreetmap.org/wiki/Planet.osm), [Geofabrik 대륙·국가별 추출](https://download.geofabrik.de/). 도로·철도·하천/수로 등 현재 지도에서 정렬 가능한 실재 선형 형상. 세계 모든 실제 상업 운송이 존재한다는 증거는 아님. Geofabrik은 보통 매일 갱신한다.
2. **Natural Earth 1:10m**: [공식 배포](https://www.naturalearthdata.com/downloads/), 도로·철도 등의 일반화된 세계 범용 벡터 보조자료. 1:10m 자료만으로 실제 영업 운임·시간·처리용량을 확정할 수 없음.
3. **상업 해운/항공**: OSM의 공항/항만 점 또는 해안선만으로 **국제 항로·운항시간·운송능력을 만들어서는 안 된다.** 실제 연결과 이동시간, 영업 연혁·회차는 별도 출처가 있는 선형 노선 및 정량 기록을 요구한다.

**라이선스**: OSM/Geofabrik의 데이터와 파생 데이터는 [ODbL](https://www.openstreetmap.org/copyright), 공개 공유 시 OSM 공헌자 표시·파생 데이터의 관련 ODbL 요건 검토가 필요하다. 출처별 원본 URL, 파일 SHA-256, 데이터의 작성·발효시기, 인게임 국경 시기 정합성을 함께 관리한다. 현대 OSM 노선을 1801년 철도·항공노선으로 소급 사용하면 안 된다.

## 두 단계 입력

**A. 지리학적 물리 연결** (`geographic_links.tsv`)
- 인게임 Phase E Province GeoJSON 객체의 `properties.province_id`와 실제 도로/철도/해상/하천/항공 선형 GeoJSON(또는 GeoJSONSeq)을 공간 조인한다.
- 각 연결에 원본 선형 객체 ID, 실제 Province 출발/도착 ID 및 모드(`road`, `rail`, `sea`, `river`, `air`)를 기록한다.
- Phase E의 실제 `SpatialState.geographic_links`에 등록한다. 인접하지 않은 두 지방의 임의 항공/해상 연결을 만들지 않는다.
- 공간 연결은 **이동 가능성**이지 영업편, 운송능력 또는 보험계약이 아니다.

**B. 역사적 실운송 서비스** (`transport_services.tsv`)
- 연결된 물리 경로 ID와 실제 공급자/운송사업자 ID(기존 C Wallet), 실측 월간 수송량, 운임, 실제 운송 일수, 역사적 유효 시작·끝 월 및 정량 근거를 요구한다.
- `valid_from_month`와 `valid_through_month`는 인게임의 `year * 12 + month - 1`로 명시한다. 현대 데이터를 과거 날짜에 자동 복사하지 않는다.
- 제출된 서비스 원본 파일의 SHA-256과 지도 원본 SHA-256을 기록하고, 시뮬레이터 시작 시 해당 날짜에 유효한 검증 서비스만 G TransportEdge에 연결한다.
- 서비스 누락·근거 불일치·용량/기간/운송일수 미확정 시 해당 간선은 정식 운송서비스로 인정하지 않는다.

## 데이터 준비 예시

```bash
# Geofabrik에서 실제로 받은 지역 원본으로 실행.
# 전 세계를 한 번에 메모리에 적재하지 말고 대륙/지역별 반복.
osmium export your-region.osm.pbf -o your-region.geojsonseq

python scripts/build_verified_transport_links.py \
  --provinces your-phase-e-provinces.geojson \
  --lines your-region.geojsonseq \
  --services documented-transport-services.csv \
  --output prepared-transport/region1
```

`--services` 파일에는 다음 열이 필수: `service_id,link_id,carrier,monthly_capacity,freight_per_unit,transit_days,valid_from_month,valid_through_month,source_url`. 서비스의 `link_id`는 A단계에서 생성된 `geographic_links.tsv`와 정확히 일치해야 한다. 독립 출처로 입증되지 않은 서비스는 작성하지 않는다.

출력:
- `geographic_links.tsv`: 실제 검증 연결 후보
- `transport_services.tsv`: 출처와 수송능력 및 유효기간을 갖춘 영업서비스
- `provenance.json`: 입력 SHA-256, 적재 대상 수, 라이선스 유의사항

C++는 `vic25::verified_transport::seed(gh_world, link_stream, service_stream, starting_month)`를 Phase E/G 초기화 후 첫 일별 커밋 전에 호출한다. `gh_world`에는 Province와 운송사업자 Actor가 이미 등록되어 있어야 한다.

## 2026-11-07 후속: 지역 통합·역사적 연월일 엄격 처리

- 추가 원본: [OpenHistoricalMap](https://www.openhistoricalmap.org/)의 역사적 교통선·철도와 `start_date`/`end_date`. 태그는 `YYYY`, `YYYY-MM`, `YYYY-MM-DD`의 다양한 정밀도를 허용하므로 **모르는 월·일을 만들어서는 안 된다**. 연도만 명시된 개통일은 해당 해 내부의 임의 월이 아니라 **다음 해부터 존재가 확실한 완전한 월**, 종료 연도만 명시된 폐쇄일은 **전년도까지 존재가 확실한 완전한 월**로 보수 처리한다.
- `scripts/build_verified_transport_links.py`는 OHM의 확실한 존속기간을 `physical_line_lifetimes.tsv`로 기록하고, 서비스 운행 날짜가 선형의 역사적 존속 범위 밖이면 자료 입력을 거부한다.
- `scripts/merge_verified_transport_regions.py`는 같은 인게임 Province ID를 공유하는 여러 대륙·국가별 파일을 연결하고, 실측 링크 ID 충돌·서비스 소유권·물리적인 연결이 없는 항로·역사 날짜 불일치를 확인한다.
- `vic25::verified_transport::seed`에는 필요시 다섯 번째 인수 `std::istream* historical_line_lifetimes`를 제공한다. 인게임 시작 이후 개통하는 증거 있는 운송서비스도 사전에 보존되며, 실제 유효 월에만 G 경로가 활성화된다.

```bash
python scripts/merge_verified_transport_regions.py \
  prepared-transport/region1 prepared-transport/region2 \
  --output prepared-transport/world-verified
```

지역별 지리선이 실제 노선을 공유하지 않는 곳을 입력기가 **임의로 연결하지 않는다**. 공개 자료의 링크와 기록 유효성을 검사할 뿐, 개별 기록의 역사적 진위나 시설 수송량이 모두 검증됐다고 보증하지 않는다. 공개 현행 OSM의 과거 운송망 정확성은 별도 실측·문헌 조사에 의존한다.

## 완성 기준과 현재 미달 사유

- [ ] 실제 Phase E Province 폴리곤의 세계 전역 커버리지 확보 및 원본 버전/해시 확인
- [ ] 대륙별 도로·철도·항해·하천·항공 간선의 역사적 존재 여부 구분
- [ ] 개별 연결의 증명된 일별 운송시간·수송량·요금·실제 영업자 데이터
- [ ] 실제 전 세계 데이터 적재 후 게임 저장 및 연도별 교통망 활성화
- [ ] 대규모 지역·운송량 성능·회귀 측정 및 법정 ODbL 표시 검토

따라서 본 변경을 **‘전 세계 실측 교통망 구축 완료’로 표시하지 말 것.** 현재는 세계 전역을 대상으로 반복 실행 가능한 검증형 임포터와 물류 엔진 연동 단계다.
