# 0.2.x 고정 장비 기준선

이 디렉터리는 `HEUNGBAE-1`, Intel64 Family 6 Model 151 Stepping 2, 20 logical processors, Visual Studio 2026 v145를 0.2.x 성능 회귀용 고정 장비로 승인한다. 최초 기준은 공식 suite의 각 시나리오를 동일 조건으로 5회 실행한 중앙값이다.

`approval.json`, `environment.json`과 `aggregate.csv`는 한 묶음이다. 장비나 측정 조건을 바꾸려면 세 파일을 새 디렉터리에서 다시 승인해야 하며 기존 기준을 덮어쓰지 않는다.

```powershell
tools\run-benchmark.ps1 -Profile suite -Runs 5 -DurationSeconds 2 -Connections 32 -BaselineCsv benchmarks\baselines\heungbae-1-vs2026\aggregate.csv
```
