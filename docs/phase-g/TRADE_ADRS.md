# Phase G — 상업·무역 설계 결정 기록(ADR)

> phase-g-trade/1.0 · 2026-11-04 · SPECIFIED / NOT_IMPLEMENTED
> 하위 G-T1~T5는 G-D0 및 이 결정집의 변경 없이 알고리즘/역사적 계수만 상세화한다. 상위 원본 소유권 변경은 별도 ADR·계약 버전 증가·마이그레이션·회귀를 요구한다.

| ID | 결정 | 이유 / 배제 대안 |
|---|---|---|
| GT-ADR01 | 상업·무역은 독립 G, 실제 매매·가격·재고·금융은 C 소유 | 기업의 거래 의사결정과 회계정본을 분리, 이중 ledger 금지 |
| GT-ADR02 | 모든 무역회사는 기존 Firm, 무역센터는 ProductionSite/CapitalAsset | 무역 전용 가상국가기업·노동·건물레벨 원본 방지 |
| GT-ADR03 | 제조기업 직접 수출입·위탁/중개·무역회사 재판매를 모두 허용 | 무역센터/중개 강제 경유 및 과도한 조작 제거 |
| GT-ADR04 | 거래 허가·대리권·보관권·상품 title을 독립 권리로 취급 | 계약/운송업자가 자동 소유자가 되는 오류 방지 |
| GT-ADR05 | world market은 가격/거래정보 reference view, 실물 창고 아님 | 1.9 Victoria 3의 세계시장 개념을 참고하되 물품 무한생성 불허 |
| GT-ADR06 | MarketId·CountryId·ProvinceId·CustomsTerritory·LogisticsNode 분리 | 다국가 공통시장·국내 관세구역·임시점령·복잡 경로 지원 |
| GT-ADR07 | 플레이어는 관세·허가·보조금·공공조달·인프라·통상조약을 결정 | 임의 '수출량 +10/무역력' 버튼 없이 경제적 유인으로 자율교역 |
| GT-ADR08 | 기업별 후보 탐색 후 C-E2 Order/TradeLedger로 청산 | 두 번째 시장 체결 엔진·국가 할당원장 금지 |
| GT-ADR09 | 당월 MarketQuote 고정·다음달 재가격(C-E2) | 동월 가격 재귀/수요·공급 시차 위반 방지 |
| GT-ADR10 | 가격차 이익은 실제 구매/판매 가능호가·자금·세금·금융·용량·위험 조건을 충족해야 유효 | 지역가격차만으로 실물 없는 차익 실현 금지 |
| GT-ADR11 | 단기 수익성보다 기존 계약 이행·정부가 실제 지불하는 정책 지원 등이 우선할 수 있음 | 기업/정부의 합법적 장기전략 허용, 무료 자원은 배제 |
| GT-ADR12 | G-TradeExecution은 계약 이행의 정본, C-E2 체결·F/C-E5 Shipment가 실물 거래 정본 | 계약=납품=지급의 단일 플래그 방지 |
| GT-ADR13 | 계약권리·title·risk·custodian·물리적 위치·세관 상태·대금 상태 독립 | 출하 순간 목적지 가용재고/현금 자동 이동 방지 |
| GT-ADR14 | 모든 예약은 C/D 공통 원장/예약관리 체계 사용 | 상업·건설·군수의 같은 돈/재고/노동/선박 이중사용 금지 |
| GT-ADR15 | 0개월 운송 예외는 C-E5 검증된 당월 인도만 | 모든 장거리 수송을 무료·즉시 처리하지 않음 |
| GT-ADR16 | 국내법(C-P)·국제조약(H) → 하나의 TradePolicyProjection | 두 TradeAgreement 원본과 관세 이중부과 방지 |
| GT-ADR17 | 관세채무·실제 현금 납부·수입 가능성 각각 따로 기록 | 재정수입/인도시점·미수금 일치 |
| GT-ADR18 | 쿼터 법정 총량은 C-P/H, 실제 소진은 G PolicyAllowanceUse 운영이력 | 상품원장 추가 없이 다수 상인의 동시경합 일관화 |
| GT-ADR19 | FX는 C-E3/E5의 실제 은행 유동성·교환거래를 통한 결제만 허용 | 통화 다른 계정 합산·무상 1:1 교환 금지 |
| GT-ADR20 | 보험·보증은 금융기관/법률·채무와 실제 지급능력이 필요 | 위험수치 자체로 무료 배상금 지급 금지 |
| GT-ADR21 | 사고·전쟁·제재는 실물/정책 사건 → 이행/재고/가격 → 차월 대응 | 자동 재고삭제·정치적 외생 보너스·동월 무한루프 제거 |
| GT-ADR22 | 국제수지·통관 무역·소유권 이전·국내 거래·GDP를 분리 집계 | 재수출/환적/상인 재판매를 이중 생산으로 취급 금지 |
| GT-ADR23 | 연산은 희소시장/상품·경로·revision·스냅샷 기반으로 제한·결정론 보장 | 매월 모든 기업·시장·상품 조합 전수검색 회피 |
| GT-ADR24 | UI는 핵심 정책만 노출하고 툴팁에 예상/실제/원인/불확실성 표시 | 내부 정교함을 플레이어의 추가 버튼·미시조작으로 전가하지 않음 |

## 별도 후속 설계·계수 보정에 맡기는 사항

1. 업종·상품별 선호, 영업비·보관단가, 위험도·보험료/이자·FX 스프레드, 기업의 정보접근/예측오차 등은 실제 역사/지역/시대별 Ruleset의 파라미터. 빅토리아 원작의 값/파일을 복제하지 않는다.
2. 국제 상업 관측(GlobalTradeReference)의 기준 가격 산출 알고리즘(가격 구간·이상값 처리)과 sparse 후보 제한 K는 실측·성능 벤치마크 후 선택한다. 어떤 방법도 실제 상대 매물 없는 거래를 허용하지 않는다.
3. F/C-E5의 물류 원본·시간, H의 조약 원본, C-E5의 통관/FX 구현 상태는 현재 분리되어 있다. 새 코드가 해당 기능을 조용히 '구현됨'으로 가정하지 않고 unavailable/unsupported를 반환해야 한다.
4. 밀수/역외금융/근대 파생상품/고빈도 거래·해적행위의 세부 모형은 현재 G-T1~T5 기본 범위에 포함하지 않는다. 미래 확장하더라도 합법·비합법의 구분 및 돈/상품/실물/위험 원장은 동일하게 유지한다.
5. 당월 stage0 정책 동결, stage1 예약, stage3/7 거래, stage8 결산, stage9 차월 결정, stage10 군사/POP 변화, stage11 차월가격·검증을 변경하려면 상위 C-D0 계약 개정이 먼저다.

## 참고 자료(아이디어 참고, 코드/수치 불복제)

- Paradox 공식 1.9 소개: https://www.paradoxinteractive.com/games/victoria-3/news
- Victoria 3 Dev Diary #143(세계시장/무역센터/자율화): https://forum.paradoxplaza.com/forum/developer-diary/victoria-3-dev-diary-143-trade-rework-the-world-market.1676327/
- Victoria 3 Dev Diary #54(과거 무역로 개편: 버전 차이 유의): https://www.paradoxinteractive.com/games/victoria-3/news/dev-diary-54-trade-revisions
- C-E5 운송·무역: ../phase-c/economy/C-E5_TRANSPORT_INTERNATIONAL_TRADE.md
- G-D0 공통 계약: G-D0_COMMON_CONTRACT.md

**상태:** GT-ADR01~24는 문서상의 채택 결정이다. 실제 C++ 구현, 테스트 62개 및 G-V의 PASS를 뜻하지 않는다.
