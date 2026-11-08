# VIC2.5 — 빅토리아 3 1.15 기반 법률·이념·이익집단 구현 현황

- 날짜: 2026-11-08
- 작업 브랜치: `work/politics` (기존 브랜치 유지, 신규 브랜치 생성하지 않음)
- 범위: Phase C-P1/P2/P3, 1.15식 법률·이념 카탈로그, 기존 일간 Stage 0/9, P1 정통성/기관, 정치 관련 저장 호환성
- 분류: **실제 C++ 구현 + 테스트 / 위키·게임 원본과의 완전 동일 구현을 주장하지 않음**

## 기준 자료와 정책

1. Victoria 3 공식 위키의 법률·이익집단·이념 구조: https://vic3.paradoxwikis.com/wiki/Laws , https://vic3.paradoxwikis.com/wiki/Interest_group
2. 위키 문서 오픈 미러: https://github.com/maxzxc0110/Victoria3/blob/main/Victoria_3_Wiki/knowledge_base/03_Laws_Technology/Laws.md
3. Victoria 3 1.15 법률 원본 추출: https://vic3tabula.com/en/laws (2026-11-05, 148개 항목)
4. Victoria 3 1.15 이념 원본 추출: https://vic3tabula.com/en/ideologies (2026-11-05, 190개 항목)
5. 게임 데이터 참조 구조: https://github.com/tjysdsg/Victoria-3-files/tree/master/game/common/laws , https://github.com/tjysdsg/Victoria-3-files/tree/master/game/common/ideologies

법률은 분야당 한 개가 활성이고, 이익집단·지도자 이념은 법률 선택지를 5단계로 평가한다. 해당 법률을 교체하면 이익집단 승인·정치운동·정통성 등에 변동한다.

**게임 효과의 숫자 중 일부는 명시적인 VIC2.5 조정치다. 빅토리아 3의 정확한 계수나 설치된 DLC별 조건을 그대로 복제하지 않았다.**

## 구현 — 실제 코드 경로

| 구성 | 실제 동작 | 소스 |
|---|---|---|
| 카탈로그 | 26개 그룹, 147개 법률 ID. 참조 불가능한 법률 조합 차단. 기존 임의 분야 법률과 호환 | `phase_c_politics_catalog.hpp`, `phase_c_p2_impl.hpp` |
| 기본 이념 | 8개 P3 이익집단 타입에 근거한 기본 이념 프로필, 여러 이념 중첩, 5단계(-2..+2) 평가 | `phase_c_politics_catalog.hpp`, `phase_c_p3_impl.hpp` |
| 국가별 이념·지도자 | 실제 P3 이익집단의 이념 변경, 등록된 P1 PersonAnchor에 한정한 지도자 이념 변경 | `phase_c_p3_impl.hpp` |
| 정치력 | 기존 POP 실제 이익집단 가입 숫자·조직화·시행 법률 정치력 가산을 읽어 비율 산정 | `p3_group_view` |
| 이익집단 승인 | 시행 중인 카탈로그 법률의 이념별 평균 평가 + 실제 조직원 급진성, 종전 단일 선호가 설정된 경우 대체 이슈 처리 | `p3_group_view` |
| 법안 지지 | 현행/제안 법률의 이념별 차이를 실제 IG 정치력으로 가중한 찬성/반대/중립/집권집단 찬성 비율 조회 | `p2_political_support` |
| 정통성 | 기존 국회·지지집단·헌정 적합도 계산에 집권 집단 간 관측 가능한 법률 선호 불일치에 따른 감점(상한 20) | `p1_legitimacy` |
| 권리·정책 | 여성선거권, 공공의료, 결사·파업, 관세 상한, 소득세 상한을 현재 P2 유효 법률 및 실제 법적 명령 검증에 연결 | `p2_rule` + 기존 Stage 명령 |
| 공공기관 | no_schools / no_health_system / no_police / no_social_security 제한, 기존 실제 지출·직원·서비스 공급 상한 보존 | `p1_institution_capacity` |
| 저장 | 구 P1/P2/P3 구조체의 바이너리 레이아웃은 보존. `VIC25_PHASEC_P3_IDEOLOGIES_1` 선택적 확장으로 이념·지도자 자료 저장 및 검증 | `phase_c_snapshot.hpp` |
| 일간 처리 | 실제 도메인의 Stage 9에서 이념 변경을 허가, 법령은 기존 P2 효력일 및 Stage 0을 그대로 사용 | `phase_c.hpp`, `daily_time.hpp` |

### P2 법률 효과와 우선순위

- 사용자/시나리오가 조문 타입(`P2ClauseKind`)으로 명시한 실제 조항과 증보가 우선한다.
- 조문에 해당 사항이 없으면 활성 카탈로그 법률로부터 **지원되는 타입의 법적 제한만** 보조적으로 파생한다.
- 분야별 효과가 없는 경우, 효과를 가상으로 만들거나 임의의 현금/POP을 생성하지 않는다.
- 예: `no_health_system`인 국가는 기존 P1 공공의료기관의 실제 직원이 있더라도 공공의료 접근이 차단된다. `free_trade`에선 법적 수입 관세 상한 0, `consumption_based_taxation`에선 임금소득세 법정상한 0. 기존 P2 증보가 명시적으로 지정하면 이를 따른다.
- 중요: 현재 데이터가 말하는 '효과 없음'은 해당 법률의 모든 규칙을 구현했다는 뜻이 아니라 **도메인 효과 어댑터가 아직 없는 것**이다. 국가별 조건과 법안 서술 이벤트는 별도 구현 범위다.

### 이념·정치력 계산

1. `P3GroupType` 기본 이념은 8개 유형별 템플릿을 제공하되, 개별 국가/특수 이익집단은 `p3_set_ideologies`로 명시적 덮어쓰기를 지원한다.
2. 인물에게 실존 P1 `PersonAnchor`가 연결된 경우에만 지도자 이념을 추가 평가한다. 이념 변경 때문에 인물이 자동으로 생성되지 않는다.
3. 각 이념의 개별 시행 법률 평가를 -2~+2 정수로 합산, 지도자 이념은 2중 가중한다. 이념에 해당 법률 의견이 없다면 반영하지 않는다.
4. 법률과 연결된 이익집단 정치력 배율은 살아 있는 POP의 등록 집단원 수에만 적용하며 정치력 자원 원장, 가상 회사·인구를 따로 쓰지 않는다.
5. 이익집단 승인에는 시행 법률 선호·실제 관측 POP 급진성이 반영된다.
6. 정통성은 기초 지지집단/의석/헌정 점수에서 집권집단들의 법률 선호가 정반대일 때 추가 하락한다. 정확한 빅토리아 3 합산식 복제 대신 이미 구현된 VIC2.5의 기존 정통성 수식을 보존한다.

## 테스트

`tests/test_politics_catalog.cpp`의 실제 컴파일/CTest 사례:
- 26법률 그룹의 스키마, 법률-그룹 불일치 거부, 이념별 서로 다른 찬반, 실존 지도자 연결, 불법 가짜 이념 거부
- 자유방임/협동소유 법률에 따른 실제 IG 정치력·정통성 비교 및 POP/예산 불변
- 의료/사회보장기관, 정치적 결사·파업, 여성선거권, 조세 법정 한도
- 명시적인 P2 증보가 카탈로그의 fallback 권리를 합법적으로 덮어쓰는 우선순위
- 실제 기존 Phase GH/daily 이력 및 새 P3 이념 세이브의 바이너리 결정론적 저장·복원

실행 명령: `cmake -S . -B build -DBUILD_TESTING=ON`, `cmake --build build --target vic25_politics_catalog_tests`, `ctest --test-dir build -R vic25_politics_catalog_tests --output-on-failure`

정규 통합 CI는 `VIC2.5 consolidated integration Linux Windows`, 집중 CI는 `VIC2.5 content engine focused regression` 워크플로.

## 아직 미구현·추가 필요

**완전 1.15 parity라면 반드시 추가해야 할 항목이다. 이번 구현의 완성 경계 밖이다.**

- 1.15 게임 원본 148개 법률 가운데 카탈로그 147개만 매핑(모든 DLC·국가별 법률 조건과 변형 통합은 아직 미확정).
- 1.15 게임 원본 이념 190개 중 모든 개별 이념과 5단계 법률 지지표를 데이터로 전수 적재한 상태는 아님. 현재 주로 8개 기본 IG의 대표 이념과 일부 인물 이념이 구현되어 있다.
- 부·직업·차별·시민권·종교별 실측 정치력, 이익집단 특성 발동, 지도자 후계·개별 선택 이벤트, 실제 전 세계 정치 인물·법률 보정은 아직 미완성.
- 소유권·토지개혁·노예제·전면 이민/차별·아동노동·경제/복지 실물 효과·식민화·교육 기회·경제법별 민간 투자 제한·군법 전반의 전용 도메인 연결은 아직 불완전하다.
- 법률 발의/토론/교착/통과의 빅토리아 3식 확률·기간·급진 반응, 정부 청원·입법 정치 AI, 완전한 P1~P5 정치 UI는 후속 기능이다.

**테스트 GREEN = 구현된 계약의 회귀 통과이지, 위 목록까지 모두 완성됐다는 뜻이 아님.**
