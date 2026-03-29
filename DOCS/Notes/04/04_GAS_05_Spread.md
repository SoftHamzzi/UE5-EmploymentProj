# 기획서: SpreadDistributionCurve PDF 룩업 테이블

> 우선순위 5 — 발사 어빌리티 완료 후 진행.
> 현재 단순 X→Y 리매핑 구조를 확률 밀도 기반 샘플링으로 교체.

---

## 1. 목표

- `SpreadDistributionCurve`를 PDF(확률 밀도 함수)로 해석
- BeginPlay에서 CDF 룩업 테이블 구축 → 이진탐색 역CDF 샘플링
- 디자이너가 X=반경비율(0=중심, 1=가장자리), Y=상대확률로 커브를 직관적으로 그릴 수 있게 함

완료 기준: 산탄총 PelletCount=5 발사 시 펠릿이 커브 형태대로 분포. 중심 집중 커브라면 중심에 몰림 확인.

---

## 2. 현재 코드 문제

```cpp
// AEPWeapon::Fire() 내부
const float R = WeaponDef->SpreadDistributionCurve
    ? WeaponDef->SpreadDistributionCurve->GetFloatValue(FMath::FRand())
    : FMath::FRand();
```

**문제**: `GetFloatValue(FMath::FRand())`는 X축을 0~1 난수로 사용.
이 경우 커브가 역CDF 형태(X=누적확률, Y=반경비율)로 그려져야만 올바른 분포가 나옴.
디자이너가 원하는 방향(X=반경비율, Y=확률)과 정반대.

**원하는 방향**:
- X=반경비율(0~1), Y=상대확률(높을수록 해당 반경에 더 많이 몰림)
- 예: X=0, Y=1 + X=1, Y=0 → 중심부에 집중

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPWeapon.h` | `SpreadCDFTable`, `CDFTableSize`, `BuildSpreadCDFTable()`, `SampleSpread()` 추가 |
| `EPWeapon.cpp` | `BeginPlay`에서 `BuildSpreadCDFTable()` 호출, `Fire()`에서 `SampleSpread()` 사용 |

---

## 4. 이론 정리

### PDF → CDF → 역CDF 샘플링

```
PDF(x) = 커브의 Y값 (x에서의 상대 확률 밀도)

CDF(x) = ∫₀ˣ PDF(t) dt  (0~x 구간의 누적 확률)

역CDF 샘플링:
    u = FMath::FRand()  // 균등 분포 난수 0~1
    r = CDF⁻¹(u)        // u에 해당하는 반경비율 반환
```

이 방식을 쓰면 `PDF(x)`가 높은 구간에 샘플이 더 많이 모임.

### 이산화(Discretization)

연속 커브를 직접 적분하는 대신, 256개 슬롯으로 나눠 근사:

```
CDFTable[i] = i번째 구간까지의 누적 확률 (i = 0~255)
```

샘플링 시 `u = FMath::FRand()` → CDFTable에서 이진탐색 → 해당 인덱스를 반경비율로 변환.

---

## 5. 구현

### EPWeapon.h 추가

```cpp
// Private 섹션에 추가
private:
    static constexpr int32 CDFTableSize = 256;
    TArray<float> SpreadCDFTable; // [0] = 0, [255] = 1.0 (정규화 완료)

    // BeginPlay에서 호출 — SpreadDistributionCurve PDF를 이산화하여 CDF 구축
    void BuildSpreadCDFTable();

    // 이진탐색으로 역CDF 샘플링 → 반경비율 (0~1) 반환
    float SampleSpread() const;
```

### EPWeapon.cpp 구현

```cpp
void AEPWeapon::BeginPlay()
{
    Super::BeginPlay();
    BuildSpreadCDFTable();
}

void AEPWeapon::BuildSpreadCDFTable()
{
    SpreadCDFTable.SetNumUninitialized(CDFTableSize);

    if (!WeaponDef || !WeaponDef->SpreadDistributionCurve)
    {
        // 커브 없으면 균등 분포 (선형 CDF)
        for (int32 i = 0; i < CDFTableSize; ++i)
            SpreadCDFTable[i] = static_cast<float>(i + 1) / CDFTableSize;
        return;
    }

    // Step 1: 각 구간의 PDF 값 샘플링 (사다리꼴 적분)
    double Cumulative = 0.0;
    TArray<double> RawCDF;
    RawCDF.SetNumUninitialized(CDFTableSize);

    for (int32 i = 0; i < CDFTableSize; ++i)
    {
        // X 범위: [i/N, (i+1)/N], 중점에서 PDF 값 샘플
        const float XMid = (i + 0.5f) / CDFTableSize;
        const float PDFVal = FMath::Max(0.f,
            WeaponDef->SpreadDistributionCurve->GetFloatValue(XMid));
        Cumulative += PDFVal;
        RawCDF[i] = Cumulative;
    }

    // Step 2: 정규화 (마지막 값이 1.0이 되도록)
    if (Cumulative > KINDA_SMALL_NUMBER)
    {
        for (int32 i = 0; i < CDFTableSize; ++i)
            SpreadCDFTable[i] = static_cast<float>(RawCDF[i] / Cumulative);
    }
    else
    {
        // PDF 전체가 0 → 균등 분포 폴백
        for (int32 i = 0; i < CDFTableSize; ++i)
            SpreadCDFTable[i] = static_cast<float>(i + 1) / CDFTableSize;
    }
}

float AEPWeapon::SampleSpread() const
{
    if (SpreadCDFTable.IsEmpty())
        return FMath::FRand();

    const float U = FMath::FRand(); // 균등 난수 0~1

    // 이진탐색: U <= CDFTable[i]인 첫 번째 i 탐색
    int32 Lo = 0, Hi = CDFTableSize - 1;
    while (Lo < Hi)
    {
        const int32 Mid = (Lo + Hi) / 2;
        if (SpreadCDFTable[Mid] < U)
            Lo = Mid + 1;
        else
            Hi = Mid;
    }

    // 반경비율 반환 (0~1)
    return static_cast<float>(Lo) / CDFTableSize;
}
```

### Fire() 내부 교체

```cpp
// 기존
const float R = WeaponDef->SpreadDistributionCurve
    ? WeaponDef->SpreadDistributionCurve->GetFloatValue(FMath::FRand())
    : FMath::FRand();

// 변경 후
const float R = SampleSpread();
```

---

## 6. 커브 그리기 가이드

에디터에서 `DA_AK74`의 `SpreadDistributionCurve` 편집 시:

| 원하는 분포 | 커브 형태 |
|-------------|-----------|
| 중심 집중 | X=0, Y=1.0 → X=1, Y=0 (우하향 직선 또는 볼록 곡선) |
| 가장자리 집중 | X=0, Y=0 → X=1, Y=1.0 (우상향) |
| 도넛형 (중간 집중) | X=0.3~0.7 구간 Y가 높고 양 끝 낮음 |
| 이중 피크 | X=0.1, Y=1.0 / X=0.5, Y=0.1 / X=0.9, Y=1.0 |
| 균등 분포 | Y=1 상수 (또는 커브 없음) |

> Y값의 절댓값은 중요하지 않음 — 비율로 정규화하므로. `Y=2`와 `Y=200`은 동일.

---

## 7. 완료 체크리스트

- [ ] `BuildSpreadCDFTable()` BeginPlay 호출 확인
- [ ] CDFTable 마지막 값이 1.0에 근사 (로그 출력으로 확인)
- [ ] 중심 집중 커브로 산탄총 PelletCount=5 발사 → 중심 밀집 확인 (DebugDraw)
- [ ] 균등 커브 (Y=1 상수) → 이전 `FMath::FRand()`와 동일 분포 확인
- [ ] 커브 없을 때 균등 분포 폴백 동작 확인
- [ ] 연속 발사 시 성능 이상 없음 (SampleSpread = O(log N), N=256)

---

## 8. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| 총알이 항상 가장자리 | PDF Y값이 X=1에만 높음 | 커브 확인 — X=0 부근 Y값 올릴 것 |
| BuildSpreadCDFTable 미호출 | `SpreadCDFTable` 비어있음 → 균등 폴백 | BeginPlay 호출 확인, 로그 추가 |
| WeaponDef 런타임 교체 시 CDF 미갱신 | BeginPlay만 호출 | 무기 정의 교체 시 `BuildSpreadCDFTable()` 재호출 |
| 커브 Y값 음수 | `FMath::Max(0.f, ...)` 클램핑 | 이미 처리됨, 에디터에서 음수 입력하지 않도록 주의 |
