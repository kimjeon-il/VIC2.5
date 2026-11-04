# Phase F — 일별 물류 시간 인터페이스 (후속 구현 계약)

> `vic25-time/2.0`의 Phase F 간 인터페이스만 확정. **Phase F 완성형 C++ 엔진과 독립 세이브 포맷은 현재 저장소에 없음.**

- `ShipmentId`, `RouteId`, `TransportNodeId`, `TransportEdgeId`는 기존 G/H/E 참조와 통합. 물품 소유권과 실물 재고는 기존 InventoryLedger 정본이다.
- 새 물류 명령은 `depart_day`, `expected_arrival_day`, `actual_arrival_day`, `last_processed_day` 등 일별 시간 필드를 요구한다. 월별 `transit_months`를 임의로 `×30` 변환하지 않는다.
- 실제 운송/도착/손실은 발생일의 JournalBatch와 원본 Inventory/Ownership/Financial의 원자적 사건을 요구한다.
- 이동 시간·수송량·군수/무역 자원 예약은 같은 날짜의 실제 경로/시설 용량을 초과할 수 없다.
- 항구·철도·수로·육상 경로는 E의 고정 지리 위에 배치되지만, 프로빈스가 직접 운송량·재고를 중복 소유하지 않는다.
- 아직 실제 일별 물류 스키마가 없는 G/H 레거시 세이브는 기존의 월별 선적/도착 스케줄을 원문대로 유지하며 월말에 정산한다. 정당한 원본 timestamp 없이 day ETA를 임의 생성하지 않는다.
- 완료 게이트: 28/29/30/31일 배송, 윤년, 월 경계·국경 변경·장치 손실과 무관한 배송, 중복적재/도착 방지, C/G/H 진짜 원장 합계, 30k Province 공간인덱스 부하 및 저장·복구.
