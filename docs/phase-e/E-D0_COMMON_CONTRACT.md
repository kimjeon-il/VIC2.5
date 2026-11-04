# E-D0 — 공간·영토·행정구역 공통 설계 계약

> 계약 version = phase-e-common/1.0 / 2026-11-04 / SPECIFIED, NOT IMPLEMENTED.
> 상위: POP-D0~D3; C-D0 + C-D4; D-D0 + D-V. 충돌 시 상위 POP → C 시간/원장 → D 자원 → E 공통 → E-G 상세 순서를 적용. 상위 변경은 ADR, 버전, 마이그레이션 필수.

## 1. 역할과 금지사항

1. Province는 게임 도중 분할·병합·경계 변경하지 않는 고정 공간 기초 단위. 행정구역/국가/시장은 Province와 동일한 식별자가 아니다.
2. 인구 원본은 PopulationSlice.count 하나, 고용·자금·재고·재산·법률·군사·생산의 원본은 기존 분야의 레지스트리/원장만 사용. E는 다른 원본을 복제하지 않는다.
3. ProvinceRegistry/GeographyRegistry: 프로빈스 정적 정의, 데이터셋 정본 식별자, 지리 인접 그래프. 인구·시설·주권·동적 토지 이용은 소유하지 않는다.
4. 기존 C-D0 JurisdictionRegistry를 E가 구현·확장한다. 별도 동시 쓰기 가능한 두 번째 sovereignty 원장을 만들지 않는다.
5. AdministrationRegistry: 행정단위 정의/법적 계층/프로빈스의 직속 말단단위 소속 및 이력.
6. TerritorialScopeRegistry: 선거·지방정부·군사관할 등 목적별 관할 범위 관계만 관리. 선거표/재정/법적 권한 본체는 C-P/E/M.
7. Renderer: 정적 raster_id tile + 동적 프로빈스 상태 LUT + 파생된 경계·선택. 래스터 색상/텍스처는 정본이 아니다.

## 2. EntityId와 지역 주소

- 모든 typed ID는 독립 namespace/안정적 64-bit 비재사용 양의 정수; 0은 invalid. 별도 내부 GPU dense index는 32-bit 등으로 재매핑 가능하지만 게임 저장/참조 ID 아님.
- ProvinceId, SeaZoneId, DatasetId, AdjacencyId, JurisdictionId, ClaimId, AdminUnitId, AdminSchemeId, AssignmentId, ScopeId, ScopeAssignmentId는 구별.
- 기존 RegionId는 가상 무지도 테스트에 유효. 실제 지리 시나리오의 거주 Region은 고정 ProvinceId에 명시적 일대일 RegionProvinceBinding을 갖는다. 예전 하나의 Region을 여러 Province로 임의 등분하지 않는다. 재배분은 별도 PopulationEvent/Ownership/Inventory/Employment 사건을 요구.
- 국제 국가 CountryId와 정부 GovernmentId는 구분. 국토 귀속이 국적·주민·현금·소유권을 자동 이전하지 않음.
- 공식 명칭, 정부 별칭, 국기, 영문코드, 표시 색상 등은 stable ID가 아니다.

## 3. 정적 대 동적

정적 공간: ProvinceDefinition + canonical geometry dataset/hash/area/adjacency, SeaZoneDefinition, 좌표 공간인덱스. 역사 연도마다 원본 Province 공간 자체를 덮어쓰지 않는다.
동적 관계: ProvincialTerritorialJurisdiction (sovereignty, civil_administration, fiscal_authority, military_control), Claim/Dispute, AdministrationScheme/Unit/Assignment, TerritorialScope/Assignment; valid_from_month/valid_until_month(exclusive) + source_event_id/revision.
일반 게임 중 프로빈스 형상 편집 불가. 원본 자료 오류 보정·격자 재생성은 Dataset 마이그레이션만.

## 4. 주권/행정/관할

- sovereignty, civil_administration, fiscal_authority, military_control은 상이한 의미이며 독립적으로 유효기간을 가진다. Claim은 확정 주권과 분리.
- 각 배타적 역할에 같은 프로빈스·유효시점에서 모순된 확정 Authority 중복은 거부. 무주지·미확정은 별도 tagged state; 합법 공동통치는 JointAuthority 객체 및 법적 근거로 표현.
- 기본 legal administration scheme은 해당 프로빈스 주권국에 귀속하며, 프로빈스의 active leaf_unit을 하나 정의한다. 상위는 parent 링크의 transitively derived set. 관할 공백은 특별한 과도상태+종료조건으로만 허용.
- 군사점령국이 주권국을 대신하여 설치한 군정·임시 민정은 별도 civil/occupation scope; 주권국 기본 행정계층의 원본을 멋대로 바꾸지 않는다.
- 행정 계층은 임의 깊이이며 각 parent는 같은 scheme·관할 상위에 있고 DAG보다 엄격한 parent-tree(사이클 금지). 직접 소속 프로빈스는 최하위 단위 하나; 국가 직할은 적법한 root-level 단위를 갖는다.
- TerritorialScope는 Electoral, LocalGovernment, Fiscal, MilitaryDistrict, Special 등 purpose·legal_basis·coverage·exclusivity rule에 따라 복수 겹침 가능. 해당 contest/권한에서의 중복 적격 배분은 금지.
- 단일 Province의 일부를 정밀한 공간 경계로 나눌 수 없다. C-P4가 허용하는 POP partition 기반 선거 배분은 공간 경계가 아닌 인구계산의 확장이며, 1인 중복 선거구 배분을 금지.

## 5. 사건, 시간, 원자성

모든 변경은 C-D0 Command → ProposedEvent → revision/권한/시점/공간·교차원장 검증 → JournalBatch → StagingWorld → global invariant → one Commit. 멱등 command_id/event_id; 중복 replay 무효.
- 평시 행정개혁/조약/법률은 월 m stage9 확정 → m+1 stage0 발효. 이미 stage0 동결된 현재월 경제·선거·급여는 소급 정정하지 않음.
- 군사통제는 stage10 전투결과/통제사건에 기록하며 해당 월말 상태 및 다음달 stage0의 유효 snapshot으로 반영. 경제계산에 월중 소급 적용 금지.
- 주권 이전 등 다중 Province 수정은 단일 원자 사건 묶음, 실패 시 공간·인구·금전·재고·정치·군사 모두 AbortMonth. 정당하지 않은 플레이어 명령은 정상 reject; 시스템 내부 불변식 파괴는 HARD ERROR.
- Completed event는 삭제 대신 보상 사건으로 수정. Region/AdministrativeUnit tombstone과 predecessor/successor lineage.

## 6. 조회·집계 계약

Read-only API: GetProvince, LocateProvince, GetNeighbors, GetJurisdiction(province, month, snapshot), GetSovereignProvinces(country), GetAdminMembership(province,scheme), GetAdminProvinces(unit), GetScopes(province,kind), GetScopeStatistics(scope) 등.
각 조회는 WorldSnapshotId, effective_month, geography_dataset_id, jurisdiction/admin/scope revision을 고정. 캐시 소유자별 원본 revision + 관련 membership revision + 시점이 일치하지 않으면 invalid.
국가/행정 인구·기업·GDP·세율·군사력은 파생 통계; 행정구역에 쓰기 가능한 총량 원본 금지. additive measure는 차분 갱신, 비율은 분자/분모 집계 후 재계산.
접경 그래프는 topology 정본에서 조회, 단계별 군사/교통 통행 여부는 자체 분야 규칙으로 결정.

## 7. 세이브·재현·오류

세이브에 phase_e_contract_version=phase-e-common/1.0, geography_dataset_id/hash, registry revisions, 유효 및 예약 관계, 사건 ledger, lineage, last_commit_id, 상위 POP/C/D contract versions, ruleset/seed/numeric policy. 정적 래스터는 별도 검증 가능한 설치 데이터로 참조하고 게임 세이브에 모든 픽셀을 복사하지 않음. 미호환 데이터셋·타 계약은 명시적 마이그레이션 또는 거부.
같은 snapshot/ruleset/seed+명령과 유효 타임라인에서 단일/다중 worker의 공간·인구·원장 해시 동일. GPU 이미지 자체의 플랫폼별 픽셀 완전 동일은 보장하지 않으며 ID/계산 상태는 비트정확.

## 8. 불변식 ID

ED0-INV01 ProvinceId 불변/유일/재사용금지.
ED0-INV02 프로빈스 정적 형상·인접 일반 런타임 수정금지.
ED0-INV03 동일 role/기간 충돌하는 확정관할 없음.
ED0-INV04 Claim과 sovereignty 구별.
ED0-INV05 군정/점령 ≠ 주권 자동이전.
ED0-INV06 행정 parent 사이클 없음.
ED0-INV07 유효 basic scheme의 Province 소속 정확히 하나.
ED0-INV08 행정편제 변화로 ProvinceId/geometry 불변.
ED0-INV09 POP 총량 불변 (유효 POP 사건 별도).
ED0-INV10 금융/재고/소유 무단이전 없음.
ED0-INV11 목적별 scope 규칙·투표자 배분 정합.
ED0-INV12 모든 활성 참조/유효기간/revision 유효.
ED0-INV13 사건 멱등·tombstone/lineage 보존.
ED0-INV14 실패 시 전원장/월원자 롤백.
ED0-INV15 렌더링 캐시 비정본.
ED0-INV16 동등 입력에 동등 공간 커밋 해시.

기본 오류: ED0_ERR_INVALID_PROVINCE, PROVINCE_MUTATION, JURISDICTION_CONFLICT, ADMIN_CYCLE, ADMIN_ASSIGNMENT, INVALID_SCOPE, INVALID_AUTHORITY, TIME_CONFLICT, REVISION_CONFLICT, DATASET_MISMATCH, CROSS_DOMAIN, DUPLICATE_EVENT (상세 prefix ED0_ERR_ 적용). 잘못된 명령은 도메인 Reject; 불변식 손상·고아참조는 HARD ERROR.

## 9. 상세 설계 및 증명 경계

E-G1~G5는 위 계약을 바꿀 수 없고 구체 알고리즘만 추가. EV-T01~T60은 [E-V](E-V_INTEGRATION_VALIDATION.md)의 독립 실행 통합시험으로 증명. 다른 POP/CE/CP/CM/D-V 회귀와 동시 실행 필요. 문서 완성은 PASS가 아니다.
