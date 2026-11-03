# POP-D0~D3 설계 문서 색인

**상태: 설계 확정 / 구현 전** · 설계 계약 버전: pop-design/1.1 · 작성: 2026-11-03

이 디렉터리의 D0~D3 문서는 VIC2.5 POP 데이터/사건/월별 연산의 **새 단일 설계 기준**이다. 기존 docs/POP_MODEL.md, docs/ARCHITECTURE.md, docs/MONTHLY_RULES.md에는 초기 스캐폴드에 관한 역사적 기술이 남아 있으며 충돌할 경우 이 계약을 우선한다. 기존 C++20 스캐폴드는 변경하지 않았다.

| 단계 | 설계 결정 | 문서 |
|---|---|---|
| **D0** | 인구 코호트·경제 POP·가구 풀과 PopulationSlice 유일한 headcount 원본, ID/소유 책임 | [D0_ARCHITECTURE](D0_ARCHITECTURE.md) |
| **D1** | 필드형식, 복합 건강·학력, 자산과 직업 분리, 희소 인구 셀, 참조·누락 데이터 | [D1_SCHEMA](D1_SCHEMA.md) |
| **D2** | 출생·사망·이주·이직·질병·부상·징집·제대 등 원자적 이벤트, 자산/고용 동기화 | [D2_TRANSITIONS](D2_TRANSITIONS.md) |
| **D3** | 12단계 월별 실행, 롤백, 결정론, 분할·병합, 성능 프로파일링, 저장 형식 | [D3_RUNTIME](D3_RUNTIME.md) |

부속:
- [DESIGN_DECISIONS](DESIGN_DECISIONS.md): 설계결정(ADR) 및 구현 중 튜닝/보류 항목
- [MIGRATION](MIGRATION.md): 기존 1.0 Pop/CohortKey/원장과 호환·전환 계획
- [ACCEPTANCE_TESTS](ACCEPTANCE_TESTS.md): D0~D3 구현 게이트/반례/장기 검증 계획
- [6인구 기준 fixture](examples/six_people.json) · [예시 JSON Schema](examples/pop-design-1.1.schema.json): 스키마·집계·비노동인구 검수용 데이터. JSON Schema는 교차 참조/수량 보존까지 보장하지 않음

## 작업 완료와 미완료 구분

- **완료(설계):** D0/D1/D2/D3의 책임·자료형·상태 전이·계산 일정·검증 항목을 문서로 확정.
- **미완료(구현):** 실제 PopulationSlice 레지스트리, HealthProfile 분할, 이벤트 원자성/rollback, 스냅샷, 가구 엔진, 월별 경제 청산, 성능 벤치마크는 구현되지 않음.
- **현재 빌드:** 이전 커밋의 POP/금융/재고 최소 스캐폴드. 여기서 POP-D0~D3 완료라고 표시하는 것은 **설계 완료**만 의미함.

## 구현 착수 순서

1. Migration facade/새 레지스트리와 6인구 fixture → D0/D1 계약 단위 테스트.
2. 원자적 PopulationEvent / 고용·보건·자산 동시전이 → D2 테스트.
3. 월간 StageWorld/Commit, 결정론적 seed와 회귀 검사 → D3 테스트.
4. 그 후 POP-I1(출생·사망·고령화)부터 기능 추가.

설계는 인구·경제·정치·군사 연결에 필요한 인터페이스를 우선 고정하며, 빅토리아 3의 원본 코드·수치·모딩 파일을 복제한 것은 아니다.
