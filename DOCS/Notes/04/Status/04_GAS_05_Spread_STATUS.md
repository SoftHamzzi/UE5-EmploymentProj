# 04_GAS_05_Spread — 구현 상태

**전체 상태: 완료 (PIE 검수 완료)**

---

## C++ (완료)

- `EPWeapon.h`: `CDFTableSize`, `SpreadCDFTable`, `BuildSpreadCDFTable()`, `SampleSpread()` 추가
- `EPWeapon.cpp` BeginPlay: `BuildSpreadCDFTable()` 호출
- `EPWeapon.cpp` Fire(): `SampleSpread()`로 반경비율 R 획득
- `BuildSpreadCDFTable()`: PDF → CDF 룩업 테이블 (256슬롯, 사다리꼴 적분 + 정규화). 커브 없으면 균등 분포 폴백
- `SampleSpread()`: 균등 난수 U → 이진탐색 역CDF → 반경비율 반환

## 에디터 (완료)

- `CurveFloat` 에셋 생성 및 중심 집중 커브 설정
- `DA_AK74` SpreadDistributionCurve 슬롯 연결 완료
