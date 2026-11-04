# EV-T10 — 고정 프로빈스 약 10,000개 계획: 역사 국경 재현성 설계 (코드 구현 보류)

- 상태: **DESIGN_ONLY / BLOCKED_BY_SOURCE_DATA**
- 2026-11-04
- 사용자 의도: 프로빈스 구획은 역사적 국경선/행정경계 등의 교차·합집합을 고려해 약 10,000개 규모로 검토하되, 당장 실물 프로빈스 데이터·GIS importer·국가별 할당·원본 분할/병합을 구현하지 않는다.
- EV-T10은 지리자료에 근거해 실제 역사적 경계가 고정 Province 내부를 횡단하는지 진단하고 재현불가를 검출해야 하는 수락시험. **설계 문서만으로 PASS 처리 불가**.

## 입력

- 지원 시기별 유효 국경/행정경계 원본(layer kind, effective date range, jurisdiction confidence, licenses, provenance).
- 후보 고정 Province boundaries(geometry dataset version/hash).
- 게임 지도 최대 표시 축척과 오차·소지역 중요도 정책. 원칙적으로 정확히 10,000개를 강제하지 않음.

## 알고리즘 (후속 작업에서 구현)

1. 전 세계 역사자료를 기간·주권/행정/선거 구획 유형별 정규화, 출처 명시; 무근거 자동 보간/추정 경계 금지.
2. 후보 고정 Province에 역사적 경계선과 인접 국가·행정구역 소속을 오버레이하고, 각 역사 기간마다 경계가 Province 안을 횡단한 불일치(overlap/mismatch)를 계산.
3. mismatch 객체는 ProvinceId / source line / valid period / measured area+length / priority / confidence / source URLs·hash / border_kind를 포함. 모호하면 unresolved로 남긴다.
4. 지형·주권 경계를 교차하는 최소 분할 후보를 만들되, 미세 변경은 최대 축척 시 식별 가능한가를 기준으로 pruning. 동일 수선 교점, 얇은 슬리버, 좁은 회랑, 호수·섬·외딴 비연속 구역은 별도 지오메트리 검증.
5. 비용 예산의 증가는 공간적 복잡도·POP cell density·과거 시간스텝에 의해 추산. 실제 저사양 CPU/RAM/VRAM 벤치 후 1만→1.5만/2만/3만 확장 여부 결정.
6. 기초 ProvinceId/경계는 지도 데이터 버전마다 안정적으로 고정. 새 데이터셋으로 편집하면 세이브 마이그레이션 필요, 런타임 분할 금지.
7. 사용자에게 재현 가능한 border-change coverage 비율(길이/면적/중요 사건), 지역별 omitted event list 및 그 근거 제공.

## 수락 오라클

- 독립 출처에 기초한 역사 경계가 고정 프로빈스 내부를 통과하는 샘플에서 해당 ProvinceId와 시점/면적·길이/출처를 특정하여 diagnostic이 출력돼야 함.
- 경계를 모두 정확히 재현한다고 허위 주장하지 않음. low-confidence 자료를 임의로 확정하지 않음.
- 런타임 정적 프로빈스 경계·ID에 변경 0, POP/산업/군사/경제 원장 변경 0.
- 수천 건 과거 경계와 여러 지리자료 레이어를 처리하는 실제 GIS importer와 fixture가 준비된 이후에만 EV-T10을 RUN/PASS.
- 본 단계에서는 이 시험을 **BLOCKED (DESIGN_ONLY)**로 유지. EV-T60 최종 COMPLETE 조건을 충족하지 못함.
