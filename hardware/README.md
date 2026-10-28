# 측창 모터 제어기 하드웨어

프로젝트 공유 폴더의 회로도 작업을 그대로 옮겨 둔 것이다.

- `schematic/` 제어 기판 회로도(빵판 기준 통합본, 손 납땜 PCB)
  - `제어기_회로도.svg/.png`, `제어기_부품배치도.svg/.png`
  - `제어기_아트웍_의뢰.pdf` 아트웍 의뢰서 (`controller_artwork_request.pdf`는 같은 파일의 영문 이름)
  - `artwork_draft/` KiCad PCB 초안, 거버 초안, 확인용 PDF
  - `src/` 위 결과물을 만드는 파이썬 원본 (`schematic.py` 회로도, `bom.py` 부품표, `pcb_*.py` PCB, `make_pdf.py`와 `pdf.js` 의뢰서 PDF)
- `changes/` 회로 변경안 메모 (확인받아 반영한 내용)
- `old/` 통합 전의 빵판 회로도와 베이스 보드 회로도. 지금은 `schematic/`이 기준이다.
