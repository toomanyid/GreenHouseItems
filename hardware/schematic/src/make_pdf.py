# 아트웍 업체용 PDF: 요청 사항, BOM, 회로도, 부품 배치도
import html
from bom import BOM, refkey
from itertools import groupby
def refs(rs):
    rs = sorted(rs, key=refkey); out = []
    for p, grp in groupby(rs, key=lambda r: refkey(r)[0]):
        ns = [refkey(r)[1] for r in grp]; i = 0
        while i < len(ns):
            j = i
            while j+1 < len(ns) and ns[j+1] == ns[j]+1: j += 1
            out.append(f'{p}{ns[i]}' if j-i < 2 else f'{p}{ns[i]}~{p}{ns[j]}') if j-i >= 2 else out.extend(f'{p}{n}' for n in ns[i:j+1])
            i = j+1
    return ', '.join(out)
e = html.escape
rows = ''.join(f'<tr><td class="n">{i+1}</td><td>{e(refs(b[0]))}</td><td class="n">{len(b[0])}</td><td><b>{e(b[1])}</b></td><td>{e(b[2])}</td><td>{e(b[3])}</td><td>{e(b[4])}</td></tr>' for i, b in enumerate(BOM))
total = sum(len(b[0]) for b in BOM)
req = [
 ('기판', '101 × 67 mm, FR-4 1.6 mm, 2층, 동박 1 oz, 무연 HASL, 솔더마스크 녹색, 실크 흰색 윗면. 모서리 R1.5.'),
 ('실장', '모든 부품 스루홀, 손납땜. 패드 지름은 구멍 + 0.7 mm 이상 (단자대·TO-220·1N5822는 + 1.0 mm).'),
 ('배치', '첨부한 부품 배치도대로. 단자대(J1~J7), XH 커넥터(J8~J10), ESP32 소켓(J11·J12), 고정 구멍(H1~H4) 위치는 고정. 나머지는 아래 간격을 지키는 범위에서 옮겨도 됨.'),
 ('굵은 배선', '24V 입력(J1 → D1·D2 → 24V_F → C1·U1), GND 귀환, MC 코일(J2 → Q1 → Q2 → GND)은 폭 1.5 mm 이상 (최대 약 3 A). 5V(U1 → J11 L21, J10)는 0.8 mm 이상. 신호는 0.3~0.5 mm.'),
 ('간격', '24V 그물(빨강 영역)과 3.3V·5V 신호 사이 구리 간격 1 mm 이상, 그 밖은 0.3 mm 이상. 모터 전류는 이 기판을 지나지 않음.'),
 ('GND', '아래층은 GND 면으로 채움. U1의 0V와 입력 GND는 같은 GND (기판 밖 SMPS − 한 곳에서 전력 GND와 만남).'),
 ('안테나', 'U2(ESP32) 안테나는 기판 왼쪽 밖으로 나옴. 기판 왼쪽 끝 5 mm와 H1 둘레는 양면 모두 구리 없음. H1은 나일론 나사.'),
 ('발열 부품', 'R26(120Ω 1W)은 2~3 mm 띄워 납땜하므로 구멍 둘레 열 차단 패드. R26, F1~F3 둘레 2.5 mm에 다른 부품 없음, ESP32 보드와 5 mm 이상.'),
 ('ESP32 밑', 'J11·J12 사이(배치도 보라색)는 높이 5 mm 이하 부품만. 눕힌 저항(피치 10.16 mm)과 낮은 세라믹.'),
 ('고정 구멍', 'H1~H4 M3, 구멍 3.2 mm 비도금, 둘레 지름 6.4 mm 안에 부품·배선 없음.'),
 ('실크', '모든 부품 기호, 극성(+), 1번 핀, 단자 핀 이름. J8 "M1", J9 "M2". JP2 옆에 왼쪽 세로줄 "50W", 오른쪽 세로줄 "300W". JP1 "50W 닫음". J1 "24V_A 24V_B GND GND".'),
 ('실물 확인', 'ESP32 소켓 줄 간격(22.86 mm로 둠), U1 URB4805S 핀 배치, BZ1 피치는 받은 실물로 맞춘 뒤 아트웍.'),
 ('받을 것', '거버, 드릴 파일, 확인용 PDF(층별, 실크 포함). 제작 전에 확인하고 진행.'),
]
reqh = ''.join(f'<tr><th>{e(a)}</th><td>{e(b)}</td></tr>' for a, b in req)
doc = f'''<!doctype html><html lang="ko"><head><meta charset="utf-8"><style>
@page {{ size: A4 landscape; margin: 10mm 11mm; }} @page big {{ size: A3 landscape; margin: 10mm; }} .big {{ page: big; }}
body {{ font-family: 'Noto Sans CJK KR','Noto Sans KR',sans-serif; color:#0f172a; font-size:9.6pt; margin:0; }}
h1 {{ font-size:17pt; margin:0 0 2mm; }} h2 {{ font-size:12.5pt; margin:0 0 2.5mm; }}
.sub {{ color:#475569; margin-bottom:4mm; }}
table {{ border-collapse:collapse; width:100%; }}
th, td {{ border:0.6pt solid #94a3b8; padding:1.3mm 1.8mm; vertical-align:top; text-align:left; }}
.req th {{ width:26mm; background:#f1f5f9; }}
.bom th {{ background:#e2e8f0; font-size:9pt; }} .bom td {{ font-size:8.6pt; }} .bom tr {{ break-inside:avoid; }}
.bom thead {{ display:table-header-group; }}
td.n {{ text-align:right; white-space:nowrap; }}
.page {{ break-after:page; }}
.fig {{ width:100%; height:262mm; object-fit:contain; display:block; }}
.foot {{ color:#475569; margin-top:2.5mm; }}
</style></head><body>
<div class="page">
<h1>측창 모터 제어기 · 제어 기판 아트웍 의뢰</h1>
<div class="sub">2026-10-24 · 첨부: 1. 요청 사항 · 2. 부품 목록(BOM) · 3. 회로도 · 4. 부품 배치도 · 부품 기호는 세 자료에서 모두 같다 · 부품 {total}개, 고정 구멍 4개</div>
<h2>1. 요청 사항</h2>
<table class="req">{reqh}</table>
</div>
<div class="page">
<h2>2. 부품 목록 (BOM, 기판 1장)</h2>
<table class="bom"><thead><tr><th style="width:6mm">#</th><th style="width:44mm">기호</th><th style="width:9mm">수량</th><th style="width:30mm">값</th><th style="width:52mm">규격·품번</th><th>풋프린트</th><th style="width:52mm">비고</th></tr></thead><tbody>{rows}
<tr><td></td><td><b>합계</b></td><td class="n"><b>{total}</b></td><td colspan="4">고정 구멍 H1~H4 (M3) 따로. U2 ESP32-S3-DevKitC-1은 J11·J12 소켓에 꽂는 모듈이라 기판에 납땜하지 않음.</td></tr></tbody></table>
<div class="foot">기판 밖 부품(MC, 퓨즈, 1.5KE30A, 1.5KE33CA, 모터 드라이버 BTS7960, 패널 보드)은 이 목록에 없다.</div>
</div>
<div class="page big"><h2>3. 회로도</h2><img class="fig" src="제어기_회로도.svg"></div>
<div class="big"><h2>4. 부품 배치도 (위에서 본 그림, 101 × 67 mm)</h2><img class="fig" src="제어기_부품배치도.svg"></div>
</body></html>'''
open('artwork.html', 'w', encoding='utf-8').write(doc)
print(total)
