# Phase C — 경제·정치·군사 설계 색인

**설계 상태: C-D0·C-E1~E5·C-P1~P5·C-M1~M5·C-D4 정본 등록 / C++ 통합 프로토타입 D4 48개 합성시험 PASS / 완성형 역사 게임 엔진 인수 전**  
버전: phase-c-common/1.0 · 기준일: 2026-11-03

## 기본 문서

- [**C-D0 공통 설계 계약**](C-D0_COMMON_CONTRACT.md): 기존 POP-D0~D3와 월별 12단계를 유지하면서 경제·정치·군사의 데이터 소유·이벤트·지역·정치효력·군사배정·트랜잭션·불변조건·수락 게이트를 확정
- [**C-E1~E5 경제 세부 설계**](economy/README.md): 기업·노동·생산, 상품·시장·가격, 가계·금융·투자, 조세·정부재정, 운송·국제무역 및 28 ADR·경제 인수검증
- [**C-M1~M5 군사 세부 설계**](military/README.md): 병역·징병, 편제·장비, 군수·수송, 작전·사상자, 전쟁·점령·종전 및 34 ADR·군사 인수검증
- [**C-D4 경제·정치·군사 통합 검증 설계**](integration/README.md): 단일 World/시간 규약·30개 불변조건·48개 통합 시험, 실제 테스트/저장/성능 게이트와 정치 설계 정본 및 계약·검증 선행조건 명시
- 선행 POP 계약: [POP-D0~D3 색인](../pop/README.md) — pop-design/1.1
- Phase B 구현(별도 브랜치): [인구·사회 기초 구현 보고서](https://github.com/kimjeon-il/VIC2.5/blob/work/pop-phase-b/docs/pop/PHASE_B_IMPLEMENTATION.md) — 인구·건강·교육·가구·고용 모듈만 독립 실행; 경제·정치·군사 통합 여부와 구분

## 후속 세부 설계

| 단계 | 범위 | 상태 |
|---|---|---|
| C-D0 | 공통 객체·원장·ID·월별 유효시점·트랜잭션·수락 기준 | **설계 확정** |
| C-E1~E5 | [기업·노동·상품·시장·금융·정부재정·무역](economy/README.md) | **설계 확정** |
| C-P1~P5 | [국가·정부·법률·정당·선거·정치운동](politics/README.md) | **GitHub main 설계 정본 등록·C++ 일부 구현** |
| C-M1~M5 | [병역·군제·장비·보급·전투·전쟁·점령](military/README.md) | **설계 확정** |
| C-D4 | [교차 분야 검증 규약·원자적 통합·장기 회귀](integration/README.md) | **설계 확정 / D4 프로토타입 48개 시험 PASS** |

**주의:** C-D0, C-E1~E5, C-M1~M5 및 C-D4 확정은 *설계 문서*의 완료를 뜻한다. 정치 C-P1~P5 설계 원본은 PR #7로 GitHub main에 병합됐다. C++ 통합 프로토타입의 [48개 D4 시험 통과 보고서](ENGINE_IMPLEMENTATION_STATUS.md)는 별도 문서로 관리하며, 완성형 구현 완료를 뜻하지 않는다. 경제·정치·군사의 **설계 전체 기능 구현·역사적 검증·최종 D4-DONE**은 완료되지 않았다. 후속 단계는 [C-D0 공통 설계 계약](C-D0_COMMON_CONTRACT.md), [경제 계약](economy/README.md), [군사 계약](military/README.md)의 원본·유효시점·원장 규약을 유지해야 한다.
