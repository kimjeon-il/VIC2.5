# VIC2.5 Phase E — 지리·영토·행정구역 상세 설계

- 상태: SPECIFIED / NOT_IMPLEMENTED (설계만 작성, C++ 구현·회귀 통과 주장 금지)
- 기준: 2026-11-04
- 계약: phase-e-common/1.0
- 작성 분기: work/phase-e-design (work/phase-d-design에 기반하여 별도 작성; Phase D C++ 구현 분기와 아직 통합되지 않음)
- 의존성: POP-D0~D3, C-D0/C-D4, D-D0/D-V. 상위 계약 불변, 변경 시 ADR 및 세이브 마이그레이션.

## 문서 목록

1. [E-D0 공통 계약](E-D0_COMMON_CONTRACT.md): 원본 소유권, ID, 시간, 행정·관할 권한, 이벤트, 저장·오류·불변식
2. [E-G1 기초 지리·프로빈스](E-G1_PROVINCES_GEOGRAPHY_TOPOLOGY.md): 고정 공간 단위, 정적 자료, 해역/인접, 데이터 ETL·검증
3. [E-G2 영토·점령·주권](E-G2_SOVEREIGNTY_CONTROL_BORDERS.md): 다층 관할, 영토 양도·독립, 분쟁, 경계 변경
4. [E-G3 동적 행정구역](E-G3_DYNAMIC_ADMINISTRATIVE_DIVISIONS.md): 가변 깊이, 소속·계층, 분할·병합·편입, lineage
5. [E-G4 관할·집계 확장](E-G4_SCOPES_ELECTIONS_LOCAL_GOVERNMENT.md): 선거구·지방정부와 행정구역 분리, 지역별 집계
6. [E-G5 래스터 지구본](E-G5_RASTER_GLOBE_RENDERING.md): 큐브구면 타일, 정수 ProvinceId 래스터, GPU LUT, 국경·선택·LOD
7. [E-V 통합 검증](E-V_INTEGRATION_VALIDATION.md): EV-T01~T60 개별 오라클, 보존·결정론·성능·복구

## 책임 경계

- Phase E는 지리적 주소·고정 프로빈스·유효한 주권/행정/관할 관계를 소유·제공.
- Phase B는 POP 인구원본, C는 경제·정치·군사 정본, D는 건설·산업·기술 정본을 그대로 유지.
- Phase F는 수송·물류, Phase G는 상업·무역, Phase H는 외교의 도메인 행동을 소유하며 E의 공간 데이터만 참조.
- GPU 래스터/색상/지구본 표시가 지리·영토의 정본이 되어서는 안 됨.
- 모든 공통 연산은 POP-D3/C-D0의 12-stage 월간 원자 트랜잭션 안에서 진행.

## 상태 게이트

E-D0 및 E-G1~G5의 상세 설계와 EV-T01~T60 시험 명세는 문서로 정의됨. 구현 PASS, 화면 실측 FPS, 인수 완료로 판단하지 않음. Phase C/D의 기존 테스트 카운트와도 합산하지 않음. E 통합 엔진을 구축한 이후 각각 실행 증빙·실제 오라클·환경 정보를 보고할 것.
