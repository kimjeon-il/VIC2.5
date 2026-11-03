# POP-D0~D3 설계 문서 색인

**상태: D0~D3 설계 확정 / POP-I1~I4 신규 독립 모듈 구현·검증** · 설계 계약 버전: pop-design/1.1 · 작성: 2026-11-03

이 디렉터리의 D0~D3 문서는 VIC2.5 POP 데이터/사건/월별 연산의 **새 단일 설계 기준**이다. 기존 docs/POP_MODEL.md, docs/ARCHITECTURE.md, docs/MONTHLY_RULES.md에는 초기 스캐폴드에 관한 역사적 기술이 남아 있으며 충돌할 경우 이 계약을 우선한다. 기존 C++20 스캐폴드는 변경하지 않았다.

| 단계 | 설계 결정 | 문서 |
|---|---|---|
| **D0** | 인구 코호트·경제 POP·가구 풀과 PopulationSlice 유일한 headcount 원본, ID/소유 책임 | [D0_ARCHITECTURE](D0_ARCHITECTURE.md) |
| **D1** | 필드형식, 복합 건강·학력, 자산과 직업 분리, 희소 인구 셀, 참조·누락 데이터 | [D1_SCHEMA](D1_SCHEMA.md) |
| **D2** | 출생·사망·이주·이직·질병·부상·징집·제대 등 원자적 이벤트, 자산/고용 동기화 | [D2_TRANSITIONS](D2_TRANSITIONS.md) |
| **D3** | 12단계 월별 실행, 롤백, 결정론, 분할·병합, 성능 프로파일링, 저장 형식 | [D3_RUNTIME](D3_RUNTIME.md) |

부속:
- [**Phase B 구현 보고서**](PHASE_B_IMPLEMENTATION.md) · [로컬 검증 결과](PHASE_B_TEST_RESULTS.md)
- [DESIGN_DECISIONS](DESIGN_DECISIONS.md): 설계결정(ADR) 및 구현 중 튜닝/보류 항목
- [MIGRATION](MIGRATION.md): 기존 1.0 Pop/CohortKey/원장과 호환·전환 계획
- [ACCEPTANCE_TESTS](ACCEPTANCE_TESTS.md): D0~D3 구현 게이트/반례/장기 검증 계획
- [6인구 기준 fixture](examples/six_people.json) · [예시 JSON Schema](examples/pop-design-1.1.schema.json): 스키마·집계·비노동인구 검수용 데이터. JSON Schema는 교차 참조/수량 보존까지 보장하지 않음

## 작업 완료와 미완료 구분

- **완료(설계):** D0/D1/D2/D3의 책임·자료형·상태 전이·계산 일정·검증 항목을 문서로 확정.
- **완료(신규 `vic25::phaseb`):** PopulationSlice, 복합 건강·교육, 가구별 추정 필요량, 월간 인구/보건/직업/교육 이벤트, Phase B 세계에 한정한 월별 staging/rollback, 인공 데이터 100년·10만 셀 검사.
- **미완료(전체 엔진):** 신규 POP를 기존 S0 금융·재고·상품시장·정부 원장과 통합하는 원자적 커밋, 실물 재고/소비, 세이브 호환성, 100만 셀 장기 성능 검증.
- **현재 빌드:** 구형 스캐폴드와 신규 Phase B는 공존한다. D0~D3 전 영역의 구현 완료나 기존 v0.2 원본 복원을 의미하지 않는다.

## 다음 구현 순서

1. Phase B 신규 레지스트리를 기존 S0 금융·고용·소유·재고 원장과 연결할 어댑터/마이그레이션 구현.
2. 월간 StageWorld에 기존 경제·인구의 모든 원장을 포함해 완전한 원자적 커밋 검증.
3. 실측 인구·보건·교육 자료의 계수 보정과 가구 소비의 실제 상품시장 청산.
4. POP-I5 경제 통합 → I6 정치 → I7 군사 → I8 저장/대규모 최적화.

설계는 인구·경제·정치·군사 연결에 필요한 인터페이스를 우선 고정하며, 빅토리아 3의 원본 코드·수치·모딩 파일을 복제한 것은 아니다.
