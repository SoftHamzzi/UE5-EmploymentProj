# Spread CDF 분포 시스템

> Spread 분포를 PDF(확률 밀도) 기반으로 샘플링.
> 디자이너가 X=반경비율, Y=상대확률로 커브를 직관적으로 그릴 수 있게 함.

---

## 현재 상태

**C++ 구현 완료.** 남은 작업은 에디터에서 커브 에셋 설정 + PIE 검증뿐.

---

## 완료된 C++ 작업

### EPWeapon.h (완료)
```cpp
private:
    static constexpr int32 CDFTableSize = 256;
    TArray<float> SpreadCDFTable;
    void BuildSpreadCDFTable();
    float SampleSpread() const;
```

### EPWeapon.cpp (완료)
- `BeginPlay`: `BuildSpreadCDFTable()` 호출
- `Fire()`: `SampleSpread()`로 반경비율 R 획득
- `BuildSpreadCDFTable()`: `SpreadDistributionCurve` PDF → CDF 룩업 테이블 구축 (256슬롯, 사다리꼴 적분 + 정규화)
- `SampleSpread()`: 균등 난수 U → 이진탐색 역CDF → 반경비율 반환. 커브 없으면 `FMath::FRand()` 폴백

---

## 남은 작업 — 에디터

### 1. 커브 에셋 생성

Content Browser → 우클릭 → **Miscellaneous → Curve** → `CurveFloat` 선택
저장 위치 예: `Content/Data/Weapons/Curves/FC_AK74_Spread`

커브 그리기:

| 원하는 분포 | 커브 형태 |
|-------------|-----------|
| 중심 집중 (기본 권장) | X=0, Y=1.0 → X=1, Y=0 (우하향) |
| 균등 분포 | Y=1 상수 (또는 커브 미설정) |
| 도넛형 | X=0.3~0.7 구간 Y 높음 |

> Y값 절댓값은 무관 — 정규화하므로 Y=2와 Y=200은 동일.

### 2. DA_AK74에 커브 연결

`DA_AK74_HitScan` → Details → **Weapon | Spread → SpreadDistributionCurve** 슬롯에 생성한 커브 에셋 할당.

---

## PIE 검증

1. 산탄총 `PelletCount = 5` 이상으로 설정
2. 벽을 향해 발사
3. 탄흔 데칼 위치로 스프레드 패턴 육안 확인
   - 중심 집중 커브 → 탄흔이 중심부에 몰림
   - 균등 커브 → 이전 `FMath::FRand()` 방식과 동일 분포

---

## 층화 샘플링 — Phi 각도 편중 수정

### 문제

`Fire()` 95번 줄:
```cpp
const float Phi = FMath::FRand() * TWO_PI;
```

펠릿마다 Phi를 독립적으로 뽑기 때문에 산탄총 같은 다중 펠릿 상황에서 여러 발이 특정 방향으로 몰릴 수 있음. R(반경)은 CDF로 제어되지만 각도 분포는 무보장.

### 수정 — 층화 샘플링

원을 펠릿 수만큼 동일 섹터로 나누고, 각 펠릿은 자기 섹터 안에서만 Phi를 뽑음:

```cpp
// 기존
const float Phi = FMath::FRand() * TWO_PI;

// 변경 후
const float SectorSize = TWO_PI / Count;
const float Phi = (i * SectorSize) + FMath::FRand() * SectorSize;
```

`SectorSize` 계산은 루프 밖으로 꺼내면 됨:

```cpp
const float SectorSize = TWO_PI / Count;

for (int32 i = 0; i < Count; i++)
{
    const float R     = SampleSpread();
    const float Theta = R * HalfAngle;
    const float Phi   = (i * SectorSize) + FMath::FRand() * SectorSize;

    OutPellets.Add(
        AimDir  * FMath::Cos(Theta)
        + Up    * FMath::Sin(Theta) * FMath::Cos(Phi)
        + Right * FMath::Sin(Theta) * FMath::Sin(Phi)
    );
}
```

섹터 내에서는 여전히 랜덤이라 기계적으로 균등해 보이지 않으면서, 원주 전체에 반드시 분산됨.

> PelletCount = 1(단발)일 때는 `SectorSize = TWO_PI`이므로 기존 동작과 동일.

---

## 완료 체크리스트

### C++ (완료)
- [x] `BuildSpreadCDFTable()` BeginPlay 호출
- [x] `SampleSpread()` Fire() 내 사용
- [x] 커브 없을 때 균등 분포 폴백

### 에디터 (완료)
- [x] CurveFloat 에셋 생성 및 중심 집중 커브 설정
- [x] `DA_AK74` SpreadDistributionCurve 슬롯 연결

### 코드 수정 필요
- [ ] `Fire()` 95번 줄: Phi 층화 샘플링 적용

### 검증
- [ ] 층화 샘플링 적용 후 산탄총 발사 → 펠릿이 원주 전체에 분산 확인
- [ ] 중심 집중 커브 → 펠릿이 중심부에 밀집 확인
- [ ] PelletCount = 1(단발) → 기존 동작과 동일 확인
