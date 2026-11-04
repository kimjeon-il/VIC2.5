# G-T4 — 무역정책·관세·통상조약·통관·외환

> 설계: phase-g-trade/1.0 · 2026-11-04 · SPECIFIED / NOT_IMPLEMENTED
> 의존: G-D0/T1~T3, C-P 법률/권한, C-E2/3/4/5 시장·금융·조세·통관, E-D0/E-G2 주권/관할, H-D0 TreatyTerm, F 물류(현행 C-E5 어댑터).
> 핵심: 정책은 거래 조건을 바꾸지만 상품·현금/세금·군사력의 실제 변화를 **그 정책의 법적 효력과 실제 이행 사건**으로 제한한다.

## T4-1. 법적 진실값과 조회 경계

| 규칙/상태 | 단일 원본 | G의 책임 |
|---|---|---|
| 국내 무역법·관세율·수출입허가·독점권·통제법 | C-P Law/AuthorityGrant + C-E4 Fiscal rule | 유효월·관할·상품·거래행위별 조회 |
| 국제통상협정·통행권·보호국 권한·경제제재 의무 | H-D0 Treaty/TreatyTerm/SovereigntyRelationship | 해당 국가에 유효한 조항과 예외를 읽음 |
| 통관 구역·출입국 경계·세관 관할 | E-D0 Jurisdiction + C-P 법적 위임 | 실제 과세·검사 지점 판정 |
| 관세부과/추징·환급근거 | C-E5 CustomsAssessment + C-E4 FiscalLedger | 규칙 적용·통관 증거/참조·중복부과 방지 |
| 관세/세금의 실제 납부, 무역보조금 지급 | C-E3 FinancialLedger | 지급/미수금/연체를 거래 상태에 반영 |
| FX 호가/은행잔액/대외채권·환전 | C-E3/C-E5 금융·환거래 | 실제 환전/결제가능한 금액·비용·시점 검증 |
| 통관물량·통관허가 사용 | G PolicyAllowanceUse(사용 이력) + 실제 Shipment/Customs 참조 | 법정 쿼터/허가의 배정 조정; 별도 상품 원장 아님 |

**TradePolicyProjection**은 특정 (수출처, 수입처, transit_scopes, good/quality/origin, buyer/seller, effective_month, jurisdiction, snapshot)에서 실제 적용할 유효 법률/조약/관세/할당/보조금의 **read-only 합성 결과**다. H가 아직 연결되지 않은 구현 단계는 C-E5 기존 TradeAgreement를 어댑터로 읽고, 두 원본을 동시 쓰기하지 않는다. 원본 이관은 ADR/버전/마이그레이션이 필요하다.

## T4-2. 정책 범주와 실제 실행

1. **수입·수출 관세**: ad_valorem(과세표준 × 세율), specific(상품 로트당 고정액), 역사적 혼합 관세, 해당 법률상 면제/우대·차등. 수입·수출은 독립 범주이고 법률에 의해 수입자/수출자/대리 납부자가 정해진다.
2. **수입·수출 할당량**: 상품·원산지·관할·기간·기업별/총량별로 유효 제한. 한정된 허가/쿼터를 예약 없이 무제한 중복 사용 금지.
3. **통상특권/협정**: 자유무역·특혜관세·통과권·시장접근·외국인 영업권·최혜국 대우 등 개별 TreatyTerm 범위/예외에 따라 적용. **동맹 가입만으로 모든 관세가 무료가 되지 않는다.**
4. **수출입 금지/허가**: 법률·행정부처가 정한 품목·대상·예외·시점에 따른 실제 거래/통관 제한. 금지만으로 이미 도착한 재고의 삭제·기업도산을 강제하지 않는다.
5. **상업/수출입 보조금**: 보조금 자격·한도·법정 지급근거를 Fiscal에 기재하고 실제 재원 있는 Financial 지급으로만 상인 자금 증가. 음수 관세라는 자동 현금 공급 아님.
6. **공동시장/관세동맹**: MarketAssignment, 공동 관세경계·조약·세금귀속·H 국제협력체 규정에서 실제 적용. 독립국의 군사권/주권 합병 아님.
7. **통화·자본 이동 규제**: 금·은본위, 고정/변동환율, 외환 인가·지급제재, 은행/어음·신용 거래는 C-E3/C-E5 실제 금융제도/거래를 따른다.
8. **군사·정치적 봉쇄/제재**: H/C-P의 유효 명령과 C-M의 통제/위협, F/C-E5 접근 제약을 결합. 조약 위반·압류·억류·지연은 실제 사건으로 남긴다.

## T4-3. 관세영역·원산지·통관지

- 한 Market에 여러 CustomsTerritory가 있을 수 있고 독립국 여러 곳이 관세동맹을 만들 수도 있다. **같은 국가 내부의 특수 관세구역**과 별도 시장을 구분한다.
- 각 통관 건을 (origin, departure customs territory, route transit territory, arrival customs territory, owner/consignee, good/class, effective date, procedure)로 식별한다.
- **상품 원산지**는 처음의 실제 생산지역(ProductionEvent/lot provenance)을 추적한다. 재포장·재판매·경유·단순 환적은 자동 원산지 변경 사유가 아니다. 실질적 변형 여부는 산업 C-D 생산레시피/시기별 origin_rule에 근거한 사건을 요구한다.
- 보세/재수출·일시수입·재수입·환급은 각각 법적 과세/유예/환급 조건과 실제 통관 증거가 있을 때만 허용한다. 단순 통과무역이 항상 수입은 아니다.
- 점령지 군정·관세권·주권 귀속이 다를 수 있다. 실제 과세권/징수권이 없는 주체가 세관세를 자동 받는다는 가정 금지.
- 경유국이 운송권만 갖고 상품 title을 취득하지 않았다면 국내 생산/수출로 재계상하지 않는다.

## T4-4. 통관 평가 및 관세 징수

1. 실제 Shipment 또는 법적 통관대상 거래의 경로·경유 관할에서 적격 통관 이벤트를 판정한다. 예상 통관은 세액 확정 사건이 아니다.
2. C-P/H/E의 **유효 법률·조약·관할**을 조회한다. 충돌은 법적 우선순위(헌정·국내법·조약의 해당 국가 효력 및 예외·경과조치)를 버전 있는 규칙으로 판단하고, 해석불능이면 legal_conflict/reject 상태를 명시한다. 한 법체계가 모든 국가에 똑같이 적용된다고 가정하지 않는다.
3. 원산지·분류·과세표준(신고가격, 거래가/보험·운임 포함여부, 유효 환산기준), 관세율·공제/면제를 결정한다. fixed-point와 checked 128-bit 중간연산·버전 있는 반올림 규칙을 사용한다.
4. 관세 채무는 C-E5 CustomsAssessment/FiscalLedger에 **한 번** 기재한다. 미납/압류/통관유예와 실제 재정수입(FinancialLedger)을 구분한다.
5. 납세의무자·실제 납부 가능 자금·세관/은행 결제 후 통관 허용/유예/거부/압류를 반영한다. 선적 예정분을 즉시 전국 판매 재고로 자동 합산하지 않는다.
6. 쿼터/면세사용증/허가를 예약·소진하며 중복 사용·같은 화물 다중 통관청구를 차단한다. 취소된 사용은 rollback/release; 추징/환급은 명시적 후속 사건.
7. 법적 보조금은 Fiscal 의무와 실제 Financial 지급을 구분한다. 지급 예정만으로 판매대금을 수령한 것으로 계산하지 않는다.

예시(단일 통화, 시험용): 과세표준 1,000, 종가 수입관세 10%, 다른 비용/공제 없음 → **관세 채무 100**. 실제 납세 40이라면 정부 현금수입 40, 미수금 60. 기업도 이에 대응하는 미납채무 60을 유지한다. 통관 완료/보류는 유효법률에 따라 별도 판정. 단지 '관세 100 부과'만으로 정부 현금이 100 증가해서는 안 된다.

## T4-5. 할당량·허가·정책 경쟁

- 법률상 QuotaRuleId는 C-P/H 정책의 원본이며 유효기간·region/market/firm/origin/good/quantity_limit·기간별 reset_rule을 갖는다.
- G의 **PolicyAllowanceUseId**는 (rule_ref, time_bucket, trader, reserved_quantity, cleared_quantity, source_event/CustomsAssessment ref, revision)로 정책의 사용을 추적하는 운영기록이다. 상품·현금 원장이 아니며 C-P/H의 법정 총량을 변경할 수 없다.
- 공유 상한: sum(committed_usage + active_reservations) <= applicable_quota_limit. 동일 화물·허가를 두 번 사용 금지. 상품별 최소 lot 및 단위변환을 명시한다.
- 우선허가/경매/과거의 배정관행은 유효 법률의 allocation_rule(우선권·계약의 선후·합법적 경매)로 표현한다. 동일 조건의 경합은 안정적 ID로 결정.
- 무역정책이 월 m의 stage9 채택되면 기본 m+1 stage0 발효. 발효 전 통관/판매·거래를 소급 변경하지 않는다. 장기계약의 경과조치·일부 면제·해지권은 실제 법/조약 조항과 연결.

## T4-6. 외환·국제결제

- Money는 (currency_id, int64_minor_units). 서로 다른 통화는 유효 ExchangeRateQuote와 실제 FXTrade·금융기관 상대·유동성/결제 경로를 이용해 교환한다.
- FX 호가와 실제 FX 체결의 시세·스프레드·최소 lot·규제 한도·가능액은 별개. 환율을 아는 것만으로 무제한 환전할 수 없다.
- 상인신용·환어음·무역대출·은행간 결제·금융제재는 C-E3/E5 실제 예금/채무/담보·지급결제와 G-T5 위험계약으로 처리한다.
- 외환유동성이 없으면 구매를 가짜 1:1 환전/단순 환율 곱하기만으로 성공 처리하지 않는다. 동일통화 보유, 적법한 신용계약 등 실제 대안만 허용한다.
- 초기 단일통화 시험은 정상 실행해야 하지만 모든 거래에서 통화 ID를 유지한다. 다중통화 아직 지원 전이면 GT4_ERR_FX_UNSUPPORTED 또는 GT4_ERR_NO_FX_LIQUIDITY 등으로 명시 거부한다.
- 환차손익/국제수지의 평가시점·회계 통화/현금흐름을 명시한다. 환전만으로 세계 순자산/상품이 생기지 않는다.

## T4-7. 집행·불법 유통·정책 파급

- 밀수/비공식 유통/세관 부패는 실제 검사 능력·행정기관·정치법률·집행위험을 모형화한 이후 확장 가능. 초기에는 **UNSUPPORTED**, 무역금지 우회용 무료 경로 없음.
- 제재의 선언과 집행능력은 다르다. 봉쇄해군·정치적 통제·관할·운송 제약이 없는 국가가 embargo flag만으로 모든 선박/재고를 삭제하지 않는다.
- 관세변화는 실제 수입원가·기업 원료비·POP 구매·세수/통상마찰에 연쇄적으로 반영되며, '보호무역 +생산효율'과 같은 무료 직접 보너스 금지.
- H의 영향력/국제위기는 실제 무역·대외 의존·정책 결과를 조회한다. 그 파생 영향력을 기업 마진에 중복 보너스로 가산하지 않는다.

## T4-8. 실행 단계와 API

- Stage0: 기발효 C-P 법률/H 조약·관세구역/환율권한·수송권/세관 상태 동결, 지난달 예정 도착의 실제 통관 가능성 판정.
- Stage1: 정책 Projection, 허가·쿼터 및 FX/관세예상액 자금 예약, 법적 거래 계획.
- Stage3/7: 각 품목 출하/수입/통관·관세 채무·실제 지급 또는 미지급 기록.
- Stage8: 세수채권/보조금채무·FX 대차/미납·환급 결산.
- Stage9: 신규 정책/조약/예산 변경 확정·차월 발효 예약.
- Stage10: 새 전쟁/점령·통관권한 변화는 E/C-M 사건으로 기록, 이전 stage 소급 변경 없음.
- Stage11: 쿼터/과세/FX/통관 및 유효기간·revision 검증 후 원자 커밋.

API: ResolveApplicableTradeRules(goods, origin, destination, transit, actor, month, snapshot), QuoteDuty(shipment/lot, fiscal_basis, snapshot), ReserveQuotaAllowance(rule, qty, staging), EvaluateTradeLicense(actor, goods, customs_scope, snapshot), ValidateOriginProof(lot/provenance, rule, snapshot), RequestFXSettlement(contract, currencies, amount, snapshot), ExplainPolicyTradeEffects(policy/event, scenario).

## T4-9. 불변식·인수

- GT4-INV01 조약/국내법/관할의 **단일 적용 결과**만 G에 전달; 법률·조약 이중 원본 없음.
- GT4-INV02 관세·환급은 실제 통관·권한에 대한 Fiscal/CustomsAssessment와 Financial 지급을 구분.
- GT4-INV03 같은 화물/과세지점의 중복부과·중복환급, 환급 전 납세액 초과 보상 금지(법적 별도 보상은 독립).
- GT4-INV04 쿼터 사용+유효 예약은 법정 수량 이내, 동률 경합 결정론.
- GT4-INV05 원산지는 실제 생산/변형 이력에 근거; 단순 경유·재판매로 원산지 변경 금지.
- GT4-INV06 FX 변환은 실제 금융계약·대응 분개/유동성 없이 발생하지 않음.
- GT4-INV07 법률 효력 m+1과 기존 계약 경과조치/통관시점 정합.
- GT4-INV08 보조금·금수/봉쇄는 실제 재원·집행능력·법적 범위 없이 상품 삭제/무료 자원 생성 없음.

거부/오류: GT4_ERR_INVALID_TARIFF_BASE, POLICY_CONFLICT, NO_CUSTOMS_AUTHORITY, QUOTA_EXCEEDED, NO_IMPORT_LICENSE, ORIGIN_NOT_VERIFIED, NO_FX_LIQUIDITY, FX_UNSUPPORTED, INSUFFICIENT_DUTY_FUNDS, REVISION_CONFLICT, ILLEGAL_RETROACTIVITY.

GT4-T01~T10 및 기존 C-E4/E5 통관·FX 게이트는 [TRADE_ACCEPTANCE_TESTS](TRADE_ACCEPTANCE_TESTS.md)를 따른다. **명세만 작성했으며 구현·실행 미수행.**
