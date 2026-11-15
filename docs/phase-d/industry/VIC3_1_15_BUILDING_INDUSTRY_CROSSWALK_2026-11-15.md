# VIC2.5 — Victoria 3 1.15.0 산업·건물 종류 대응표

> 설계/참조자료 (2026-11-15), C++ 런타임 미연결. 본 파일은 기존 D-I1~I5/AGENTS.md의 **산업·기업·사업장·레시피 분리**, 실제 자산/원장, 일간 12단계 계약을 변경하지 않는다. 저장 파일: `data/reference/vic3_115_building_crosswalk.json`.

## 출처·범위와 숫자

- [1.15.0 기본게임 건물 99개](https://vic3tabula.com/en/buildings): 도시 23, 농촌 34, 개발 14, 특수 28. 기본게임의 정의·생산방식을 추출한 자료이며 DLC·모드가 덮어쓴 정의의 활성화 여부는 미평가. **99 = 일반 건설가능 99종이 아니다**.
- [원작 위키 건물 목록](https://vic3.paradoxwikis.com/wiki/List_of_buildings), [원작 위키 생산방식](https://vic3.paradoxwikis.com/wiki/Production_method). 직접 접근이 제한되어 1.15 정량 대조는 위 게임 데이터 추출본을 이용했다.
- [1.15 상품 130개(기본 54+명품·특산품 76)](https://vic3tabula.com/en/goods). VIC2.5는 기본상품/품질·출처·브랜드를 구분할 수 있으나 76개를 무조건 별도 기초 산업에 대응시키지 않는다.
- 원작의 건물 `level`, 고정 건설력/비용/직업인원, 자동 금융지구·도시지구·무역센터 출현을 VIC2.5 실물 원장에 그대로 복제하지 않는다.
- 동명 특수 정의(카이저포룸 4, 사그라다 파밀리아 3)는 내부 키가 확인되기 전까지 출처상 중복 출현이라고만 표기한다. JSON의 variant 번호는 **로컬 행 식별용**이다.

## 산업 분류(기존 D-I1 13개 유지)

| 코드 | 상위 분류 | Vic3·VIC2.5 대응 |
|---|---|---|
| `AGRI` | 농경·원예 | 곡물·원예·기호·공업 작물 연결 |
| `LIVE` | 축산·목축 | 기존 도메인의 실제 생산·설비/서비스로 연결 |
| `FORE` | 임업·벌목 | 기존 도메인의 실제 생산·설비/서비스로 연결 |
| `FISH` | 어업·양식 | 기존 도메인의 실제 생산·설비/서비스로 연결 |
| `EXTR` | 광업·채굴 | 기존 도메인의 실제 생산·설비/서비스로 연결 |
| `MANU` | 제조·가공 | 17개 공장 원형을 세부 제조업으로 연결 |
| `ENER` | 연료전환·전력·열 | 기존 도메인의 실제 생산·설비/서비스로 연결 |
| `UTIL` | 상하수·폐기물·환경 | 기존 도메인의 실제 생산·설비/서비스로 연결 |
| `CONS` | 건설·수리·토목 | 기존 도메인의 실제 생산·설비/서비스로 연결 |
| `LOGI` | 운송·창고·통신 | 교통·통신의 현존 노드/간선 연결 |
| `TRAD` | 도소매·중개·숙박 | 실물 거래·보관·중개 연결 |
| `FINA` | 금융·보험·부동산 | 실제 소유·보험·대출 연결 |
| `SERV` | 전문·교육·의료·행정 등 서비스 | 사회·문화·교육·행정 서비스 연결 |

*표의 `NONE`은 13개 산업으로 강제 분류하지 않는 군사·주권행정 객체의 식별값이며, 산업 클래스를 14개로 확장하지 않는다.*

## 전수 대응

### 도시 23개

| 원작 이름 | VIC2.5 표현 | 산업 | 처리 |
|---|---|---|---|
| Arms Industries | 무기공장 | MANU | 현장형 생산시설 |
| Artillery Foundry | 화포주조소 | MANU | 현장형 생산시설 |
| Arts Academy | 예술원 | SERV | 실제 서비스 사업장 |
| Automotive Industries | 자동차공장 | MANU | 현장형 생산시설 |
| Company Headquarter | 기업 본사 | FINA | 법인/운영조직 조회 |
| Electrics Industries | 전기기기공장 | MANU | 현장형 생산시설 |
| Explosives Factory | 폭약공장 | MANU | 현장형 생산시설 |
| Fertilizer Plants | 비료공장 | MANU | 현장형 생산시설 |
| Financial Districts | 금융지구 | FINA | 소유권·투자자 조회 |
| Food Industries | 식품공장 | MANU | 현장형 생산시설 |
| Furniture Manufacturies | 가구공장 | MANU | 현장형 생산시설 |
| Glassworks | 유리·도자기공장 | MANU | 현장형 생산시설 |
| Manor Houses | 장원 저택 | FINA | 소유권·투자자 조회 |
| Motor Industries | 동력기관공장 | MANU | 현장형 생산시설 |
| Munition Plants | 탄약공장 | MANU | 현장형 생산시설 |
| Paper Mills | 제지공장 | MANU | 현장형 생산시설 |
| Regional Company Headquarter | 지역 기업 본부 | FINA | 법인/운영조직 조회 |
| Shipyards | 조선소 | MANU | 현장형 생산시설 |
| Steel Mills | 제철소 | MANU | 현장형 생산시설 |
| Synthetics Plants | 합성물공장 | MANU | 현장형 생산시설 |
| Textile Mills | 방직공장 | MANU | 현장형 생산시설 |
| Tooling Workshops | 공구공장 | MANU | 현장형 생산시설 |
| Urban Center | 도시 중심지 | SERV | 실제 시설·고용의 파생 통계 |

### 농촌 34개

| 원작 이름 | VIC2.5 표현 | 산업 | 처리 |
|---|---|---|---|
| Banana Plantations | 바나나 농장 | AGRI | 현장형 생산시설 |
| Coal Mines | 석탄광산 | EXTR | 자원현장 + 채취설비 |
| Coffee Plantations | 커피 농장 | AGRI | 현장형 생산시설 |
| Copper Mines | 구리광산 | EXTR | 자원현장 + 채취설비 |
| Cossack Host | 코사크 군단 | NONE | 군제·POP/부대 원장 |
| Cotton Plantations | 목화 농장 | AGRI | 현장형 생산시설 |
| Dye Plantations | 염료 농장 | AGRI | 현장형 생산시설 |
| Fishing Wharves | 어항·어업시설 | FISH | 현장형 생산시설 |
| Gold Fields | 노천 금광지 | EXTR | 자원 발견·활동 방식 |
| Gold Mines | 금광산 | EXTR | 자원현장 + 채취설비 |
| Iron Mines | 철광산 | EXTR | 자원현장 + 채취설비 |
| Lead Mines | 납광산 | EXTR | 자원현장 + 채취설비 |
| Livestock Ranches | 목장 | LIVE | 현장형 생산시설 |
| Logging Camps | 벌목장 | FORE | 자원현장 + 채취설비 |
| Maize Farms | 옥수수 농장 | AGRI | 현장형 생산시설 |
| Millet Farms | 잡곡 농장 | AGRI | 현장형 생산시설 |
| Oil Rigs | 유정 | EXTR | 자원현장 + 채취설비 |
| Opium Plantations | 아편 농장 | AGRI | 현장형 생산시설 |
| Rice Farms | 벼 농장 | AGRI | 현장형 생산시설 |
| Rubber Plantations | 고무 농장 | AGRI | 현장형 생산시설 |
| Rye Farms | 호밀 농장 | AGRI | 현장형 생산시설 |
| Silk Plantations | 양잠 농장 | AGRI | 현장형 생산시설 |
| Subsistence Farms | 생계 농가 | AGRI | 가구 자영·자가소비 활동 |
| Subsistence Fishing Villages | 생계 어촌 | FISH | 가구 자영·자가소비 활동 |
| Subsistence Orchards | 생계 과수원 | AGRI | 가구 자영·자가소비 활동 |
| Subsistence Pastures | 생계 목축지 | LIVE | 가구 자영·자가소비 활동 |
| Subsistence Rice Paddies | 생계 논 | AGRI | 가구 자영·자가소비 활동 |
| Sugar Plantations | 설탕 농장 | AGRI | 현장형 생산시설 |
| Sulfur Mines | 유황광산 | EXTR | 자원현장 + 채취설비 |
| Tea Plantations | 차 농장 | AGRI | 현장형 생산시설 |
| Tobacco Plantations | 담배 농장 | AGRI | 현장형 생산시설 |
| Vineyards | 포도원 | AGRI | 현장형 생산시설 |
| Whaling Stations | 포경기지 | FISH | 현장형 생산시설 |
| Wheat Farms | 밀 농장 | AGRI | 현장형 생산시설 |

### 개발 14개

| 원작 이름 | VIC2.5 표현 | 산업 | 처리 |
|---|---|---|---|
| Army Logistics Center | 육군 병참 거점 | NONE | 현존 부대·보급/물류 파생 |
| Barracks | 병영 | NONE | 실물 군사시설 |
| Conscription Center | 징병소 | NONE | 법/행정/POP 동원 |
| Construction Sector | 건설 부문 | CONS | 시공자·인력·기자재 기반 용량 |
| Government Administration | 정부청사 | SERV | 공공행정 사업장 |
| Naval Administration | 해군 행정기관 | NONE | 해군 공공행정·지휘시설 |
| Naval Fortification | 해안 요새 | NONE | 실물 군사시설 |
| Naval Logistics Center | 해군 병참 거점 | NONE | 현존 부대·보급/물류 파생 |
| Port | 항만 | LOGI | 실제 교통간선/노드·운송서비스 |
| Power Plants | 발전소 | ENER | 발전·송전·비저장 서비스 |
| Railway | 철도 | LOGI | 실제 교통간선/노드·운송서비스 |
| Skyscraper | 마천루 | TRAD | 용도별 실제 사무·상업 자산 |
| Trade Center | 무역센터 | TRAD | 실제 상인·보관·거래시설 |
| University | 대학 | SERV | 실제 서비스 사업장 |

### 특수 28개 정의(고유 명칭 23개)

| 원작 이름 | 대응 | 산업 | 방식 |
|---|---|---|---|
| Angkor Wat | 앙코르와트 | SERV | 실재 역사유산 자산 |
| Atocha Railway Station | 아토차역 | LOGI | 실제 교통망 연결 자산 |
| Big Ben | 빅벤 | SERV | 실재 역사유산 자산 |
| Cristo Redentor | 구세주 그리스도상 | SERV | 실재 역사유산 자산 |
| Forbidden City | 자금성 | SERV | 실재 역사유산 자산 |
| Grand Theater of Havana | 아바나 대극장 | SERV | 실재 역사유산 자산 |
| Hagia Sophia | 아야소피아 | SERV | 실재 역사유산 자산 |
| Kaiserforum (동명 정의 ×4) | 카이저포룸 | SERV | 실재 역사유산 자산 |
| Kiel Canal | 킬 운하 | LOGI | 실제 교통망 연결 자산 |
| Manila Cathedral | 마닐라 대성당 | SERV | 실재 역사유산 자산 |
| Mosque of Djenne | 젠네 모스크 | SERV | 실재 역사유산 자산 |
| Panama Canal | 파나마 운하 | LOGI | 실제 교통망 연결 자산 |
| Pena Palace | 페나 궁전 | SERV | 실재 역사유산 자산 |
| Power Bloc Statue | 세력권 조각상 | SERV | 실재 역사유산 자산 |
| Sagrada Família Cathedral (동명 정의 ×3) | 사그라다 파밀리아 | SERV | 실재 역사유산 자산 |
| Saint Basil’s Cathedral | 성 바실리 대성당 | SERV | 실재 역사유산 자산 |
| Statue of Liberty | 자유의 여신상 | SERV | 실재 역사유산 자산 |
| Suez Canal | 수에즈 운하 | LOGI | 실제 교통망 연결 자산 |
| Taj Mahal | 타지마할 | SERV | 실재 역사유산 자산 |
| The Eiffel Tower | 에펠탑 | SERV | 실재 역사유산 자산 |
| Vatican City | 바티칸시국 | NONE | 국가/주권 참조(건물 금지) |
| Victoria Terminus | 빅토리아 터미너스 | LOGI | 실제 교통망 연결 자산 |
| White House | 백악관 | SERV | 정부시설·역사유산 |

## 원작과 다른 구현 계약(주의)

1. `IndustryClass`는 활동·통계 코드, `BuildingArchetype`(신규 제안)은 시설·사업장 구성 청사진, `ProductionFacility`는 실제 가동설비, `IndustryRecipe`는 투입·산출 공정이다. 건물의 고정 레벨 = 설비/직원/원가/능력이 아니다.
2. 곡물 농장 5종은 같은 곡물류 경제시장으로 묶을 수 있지만 농법·토양·재배기간·면적 제약을 유지. 플랜테이션은 별도 소유계급이 아니라 해당 작물 현장형 사업장이며 법/소유/노동이 따로 결정한다.
3. 자급농장·자급어촌·자급과수원·자급목장·자급논의 5개는 가구 `OwnAccountActivity`와 POP/실물 자가소비로 계산; 공짜 건설레벨이나 별도 유령 고용인원이 아니다.
4. Gold Fields는 탐사된 자원현장/노천채광 활동, Gold Mines는 물리적 광산 설비. `ResourceSite`의 채굴 유량을 사용하며 지하 총매장량·잔존량 도입 금지. Gold가 게임 재정 화폐와 직접 동일한 것은 아니다.
5. Manor Houses, Financial Districts, Company/Regional HQ는 소유주/기업 조직 및 실제 사무소의 조회/서비스로 대응. 같은 배당·자산·노동을 별도 원장으로 한 번 더 만들지 않는다.
6. Urban Center는 인구·시설·상업/도시 서비스의 실물 기반 파생 상태. 실제 노동·토지/가게·시설 없는 무료 `Services` 생성 금지.
7. Construction Sector는 실제 건설기업·장비·인력·자재·진행공사 D-C에서 계산. 포인트 기반 가공의 건설력을 신규 독립 스톡으로 저장하지 않는다.
8. Trade Center는 상인·창고·상업활동이 있는 때 서비스 시설. 직판에는 무역센터 설치가 필수 아님. 선박 운송·철도는 실제 유효 노선, 운송사, 용량·운임·소요일·기간을 요구한다.
9. Electricity와 Transport/Services/Merchant Marine은 VIC2.5에서 물리적 송배전·서비스·선박·운송 경로 제약에 연결. 1.15 상품의 거래 가능 표시를 그대로 따라 원격 즉시배송/창고 내 무료 보관을 만들지 않는다. 전력 동월 급전은 I2-DISPATCH 계약 전 I2-SAFE 제약 유지.
10. 군사·징병·해군행정·군수시설은 POP/군대·실재 장비·보급/공공행정 도메인에 연결. 군사 인력·탄약·보급을 회사의 가상 상품으로 이중 생성하지 않는다.
11. 운하·역사는 지리정보/건설 완공일/해상 통로에 연결. 유산·기념물의 외생적 전국 산업생산 보너스는 기본 적용하지 않는다. Vatican City는 건축물과 주권국가를 동일 객체로 만들지 않는다.
12. 1.15에 별도인 Fertilizer Plants·Explosives Factory를 **서로 다른 건물 원형**으로, Copper Mines·Vineyards도 목록에 포함. 1.13 등 구버전 자료로 덮어쓰지 않는다.
13. 최초 사용 가능 연도는 **원작 해금일/기술 노드 복제 금지**. 시대별 역사 등장·실물 자원·현지 기술·기업/노동·법률/시장에 근거한 Ruleset에서 판정. 원작의 £표시 건설력은 역사적 실측 건설비가 아니다.

## 추천 데이터·코드 연결 방식

```text
ReferenceCrosswalk(99, spec-only) ──> ArchetypeCatalog (유효기간/설비요건/업종)
                                ├─> Existing IndustryClass / IndustryRecipe
                                ├─> Existing ProductionFacility / ResourceSite
                                ├─> Existing C-E Inventory/Financial/Employment/Ownership
                                ├─> D-C ConstructionProject ── commissioning
                                ├─> G/H real transport and merchants
                                └─> P0 daily economy and company AI
```

`BuildingArchetype` 도입 시 추가할 최소 필드: `id, display_name, industry_class, mapping_kind, allowed_activities[], site_requirements[], facility_requirements[], historical_period, technology_ruleset_ref?, valid_regions?, source_provenance, version`. **ID는 소스 레이블과 구분하고**, `BuildingArchetype` 자체에는 노동·현금·상품·자산 계정 원장을 두지 않는다. 모든 운영 실적은 기존 엔진에서만 발생한다.

### 작업 순서

- **P0:** 99개 참조 행·13개 산업 클래스 연결. 원작 일반 건물 중 비물리/파생 객체 분리, 중복 명칭/금지 규칙을 스키마에 명시.
- **P1:** 실제 건설 가능한 생산·서비스 원형, 레시피·상품/기술, 위치/현장 조건과 설치 자산의 연결. 농경·광산·제철·공구·식품·섬유·항만·철도 핵심 체인부터.
- **P2:** 기업 자율 투자·확장/전환·대출·법률·POP·시장·물류 연동. 기존 D-C 준공/철거와 일간 12단계 유지.
- **P3:** 역사적 개장/기술/인프라/시설·산업분포 초기값 입력. 원작 수치 그대로 쓰지 말고 근거있는 보정과 출처/시점 관리.

**현재 상태:** 대응표 및 산업·건물 분류 *설계 정리*만 완료. 런타임 카탈로그/실제 99개 건물 데이터 로더, 생산 레시피, 산업 AI 작동이 구현됐다는 뜻이 아니다. 이 문서의 작성 자체로 빌드/테스트를 시행하지 않는다.
