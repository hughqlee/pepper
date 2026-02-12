# Pepper
Pepper는 Pepper's Ghost 기법을 활용한 데스크톱 투명 디스플레이 프로젝트입니다.  
약 $100 이하의 예산으로, 주변 소음 크기에 따라 캐릭터가 `sleepy / work / dizzy` 상태로 반응하도록 설계되었습니다.

## What It Does
- 마이크 입력으로 실시간 소음 레벨(dB SPL 추정) 측정
- LVGL 애니메이션 프레임 전환으로 상태 표현
- ESP32-P4 기반 원형 디스플레이에서 동작

## BOM
- 4인치 원형 디스플레이 + ESP32 보드  
  https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.4c.htm?sku=31522
- 유리 돔 10x18cm (TBD)
- 타원형 아크릴 패널 (TBD)
- 3D 프린트 케이스 (TBD)

## Firmware Quick Start
`firmware/` 디렉토리에서 실행:

```bash
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

권장 환경: ESP-IDF 5.5.x (`dependencies.lock` 기준 5.5.2).

## Repository Layout
- `firmware/`: ESP-IDF 프로젝트(코드, 설정, 컴포넌트 의존성)
- `hardware/`: 제작용 하드웨어 파일(DXF 등)
- `docs/`, `media/`: 문서 및 미디어 자산

## Roadmap
1. 한국어 음성 대화 인터랙션 추가 (on-device/edge 추론 검토)
2. 소형 버전 프로토타입 제작
3. 조립 가이드 및 부품 도면 공개

## Reference
- Original inspiration: https://www.veeb.ch/projects/the-valley-beneath-the-pepper-dome







