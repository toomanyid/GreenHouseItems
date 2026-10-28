# 제어 기판 부품 배치도 (손납땜 PCB, 스루홀). 단위 mm, 좌표는 부품 외곽(courtyard)의 왼쪽 위.
import sys, itertools
from lib import O, T, esc, FK, svg
BW, BH = 101.0, 80.0          # 기판 크기
GAP = 0.5                     # 외곽끼리 최소 간격
HOT = 2.5                     # 뜨거워질 수 있는 부품 둘레 여유
S = 9.0                       # 그림 배율 px/mm
OX, OY = 140, 180              # 그림 원점

# 표준 외곽
RV = (3.8, 5.2)    # 1/4W 세움 P2.54
RL = (11.0, 2.8)   # 1/4W 눕힘 P10.16 (ESP32 밑)
CD = (5.0, 3.0)    # 세라믹 P2.54/5.08
DV = (3.0, 5.0)    # DO-35 세움
DL = (9.0, 2.4)    # DO-35 눕힘 P7.62 (ESP32 밑)
TV = (4.2, 7.0)    # DO-15 (P6KE30A) 세움
DB = (6.2, 7.6)    # DO-201AD (1N5822) 세움
D41 = (3.6, 5.0)   # DO-41 세움
PTC = (8.0, 3.6)
TO220 = (10.6, 5.0)
HOLE = 3.2         # M3 고정 구멍 주변 비움 반지름 (나사머리 5.5mm + 여유)
HOT_ESP = 5.0      # 뜨거운 부품과 ESP32 보드 사이

def box(q, m=0.0): return (q[2]-m, q[3]-m, q[2]+q[4]+m, q[3]+q[5]+m)
def hit(a, b): return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]

def build(BH):
    P = []; H = []
    def p(ref, x, y, w, h, kind='sig', sub=''): P.append((ref, sub, x, y, w, h, kind))
    TY = BH - 8.6; YB = TY - 0.5
    # ---- ESP32 소켓 (L줄 아래, R줄 위, USB 오른쪽, 안테나 왼쪽 기판 밖)
    p('J12', 2.0, 1.7, 56.4, 2.6, 'esp', 'ESP32 R줄 암 핀헤더 22P')
    p('J11', 2.0, 24.56, 56.4, 2.6, 'esp', 'ESP32 L줄 암 핀헤더 22P')
    # ESP32 밑 (높이 5mm 이하): 눕힌 저항 15, BAT85 1, 세라믹 8
    under = [('R1','10k'),('R2','10k'),('R3','10k'),('R7','10k'),('R8','10k'),('R9','10k'),('R4','10k'),('R10','10k'),
             ('R28','10k'),('R29','10k'),('R30','10k'),('R31','10k'),('R23','10k'),('R25','10k'),('R27','10k'),('D6','BAT85')]
    for i,(r_,v) in enumerate(under):
        c, r = i % 4, i // 4
        p(r_, 7.0 + c*12.7, 5.3 + r*3.7, *(DL if v=='BAT85' else RL), 'under', v)
    for i,(r_,v) in enumerate([('C9','100nF'),('C12','10nF'),('C13','10nF'),('C14','10nF'),('C15','10nF'),('C16','10nF'),('C17','10nF'),('C8','100nF')]):
        p(r_, 7.0 + i*6.2, 20.6, *CD, 'under', v)
    H.append((3.4, 14.4))                       # H1: ESP32 밑 안테나 쪽 (나일론)
    H.append((BW-3.4, 3.4))                     # H2: 오른쪽 위
    H.append((3.6, TY-3.8)); H.append((90.4, TY-3.8))     # H3, H4: 단자대 바로 위 (H4는 J9 왼쪽 아래)
    # ---- 아래 가장자리: 나사 단자대 5.08mm
    def term(ref, x, n, sub): p(ref, x, TY, n*5.08, 8.1, 'conn', sub)
    term('J1', 0.5, 4, '24V_A 24V_B GND GND'); term('J2', 21.4, 2, 'MC 코일'); term('J3', 32.1, 3, 'ESTOP RLY_OUT GND')
    term('J7', 47.9, 2, '온도 T+ T−'); term('J5', 58.6, 4, '원점 센서1'); term('J6', 79.9, 4, '원점 센서2')
    # ---- 오른쪽 가장자리 잠금 커넥터, 패널 커넥터
    p('J8', 94.6, 7.4, 6.0, 22.4, 'conn', 'XH 8P M1')
    p('J9', 94.6, 30.3, 6.0, 22.4, 'conn', 'XH 8P M2')
    p('J10', 60.5, 0.6, 20.4, 6.0, 'conn', 'XH 7P 패널')

    fixed = [box(q, 0) for q in P] + [(hx-HOLE, hy-HOLE, hx+HOLE, hy+HOLE) for hx, hy in H]
    EZ = (-11.0, 0.0, 60.0, 29.2)
    placed = []
    def ok(x, y, w, h, kind, zone):
        m = 2.0 if kind == 'hot' else 0.0       # 뜨거운 부품: 둘레 2.0 + 기본 0.5 = 2.5
        b = (x-m, y-m, x+w+m, y+h+m)
        if b[0] < zone[0]-1e-6 or b[1] < zone[1]-1e-6 or b[2] > zone[2]+1e-6 or b[3] > zone[3]+1e-6: return False
        if hit(b, EZ): return False
        if kind == 'hot' and hit((x-HOT_ESP, y-HOT_ESP, x+w+HOT_ESP, y+h+HOT_ESP), EZ): return False
        for f in fixed:
            if hit((b[0]-GAP, b[1]-GAP, b[2]+GAP, b[3]+GAP), f): return False
        for q in placed:
            mq = 2.0 if q[6] == 'hot' else 0.0
            if hit((b[0]-GAP, b[1]-GAP, b[2]+GAP, b[3]+GAP), box(q, mq)): return False
        return True
    def pack(zone, items, up=False):
        for ref, sub, w, h, kind, *rot in items:
            m = 2.0 if kind == 'hot' else 0.0
            xs = {zone[0]+m}; ys = {zone[1]+m, zone[3]-m}
            for q in placed + [((), (), f[0], f[1], f[2]-f[0], f[3]-f[1], 'f') for f in fixed]:
                mq = 2.0 if q[6] == 'hot' else 0.0
                xs |= {q[2]+q[4]+mq+GAP+m, q[2]-mq-GAP-m}
                ys |= {q[3]+q[5]+mq+GAP+m, q[3]-mq-GAP-m}
            best = None
            for ww, hh in ([(w, h), (h, w)] if rot else [(w, h)]):
                for x in xs:
                    for y in list(ys) + [y0 - hh for y0 in ys]:
                        if ok(x, y, ww, hh, kind, zone):
                            key = ((-(y+hh) if up else y), x)
                            if best is None or key < best[0]: best = (key, x, y, ww, hh)
            if best is None: return False, ref + ' ' + sub
            placed.append((ref, sub, best[1], best[2], best[3], best[4], kind))
        return True, ''
    Z = lambda x1, y1, x2, y2: (x1, y1, x2, y2)
    jobs = [
      # 오른쪽 위: 패널, I2C 레벨 변환(Q4·Q5), 전류 감지, 피에조 (위에서부터)
      (Z(60.5, 0.6, 94.1, YB), False, [
        ('BZ1','피에조 12mm',12.6,12.6,'sig'), ('JP2','3×3',8.0,8.0,'sig'),
        ('R5','2.2k',*RV,'sig'), ('R6','1.3k',*RV,'sig'), ('C10','1µF',4.0,3.0,'sig',1), ('D4','BAT85',*DV,'sig'),
        ('R11','2.2k',*RV,'sig'), ('R12','1.3k',*RV,'sig'), ('C11','1µF',4.0,3.0,'sig',1), ('D5','BAT85',*DV,'sig'),
        ('Q4','2N7000 SDA',5.4,4.2,'sig'), ('Q5','2N7000 SCL',5.4,4.2,'sig'),
        ('R35','4.7k',*RV,'sig'), ('R36','4.7k',*RV,'sig'), ('R37','4.7k',*RV,'sig'), ('R38','4.7k',*RV,'sig'),
        ('Q3','2N7000',5.4,4.2,'sig'), ('R33','100Ω',*RV,'sig'), ('R34','10k',*RV,'sig'), ('R32','1k',*RV,'sig')]),
      # 온도 입력 (J7 위)
      (Z(44.0, 29.7, 72.0, YB), True, [
        ('R26','120Ω 1W 띄움',17.4,5.0,'hot',1), ('F3','PTC 0.05A',*PTC,'hot',1), ('D13','P6KE30A',*TV,'v24',1)]),
      # 주 차단 MC (J2, J3 위)
      (Z(25.4, 29.7, 47.4, YB), True, [
        ('Q1','IRLZ44N',*TO220,'v24'), ('Q2','IRLZ44N',*TO220,'v24'), ('D3','1N4007',*D41,'v24',1),
        ('R17','100Ω',*RV,'sig'), ('R18','10k',*RV,'sig'), ('R19','100Ω',*RV,'sig'), ('R20','10k',*RV,'sig'), ('R21','100k',*RV,'sig'),
        ('R13','100k',*RV,'v24'), ('R14','10k',*RV,'sig'), ('R15','100k',*RV,'v24'), ('R16','10k',*RV,'sig'),
        ('C6','100nF',*CD,'sig',1), ('C7','100nF',*CD,'sig',1), ('JP1','50W 닫음',5.6,3.0,'v24',1)]),
      # 전원 (J1 위)
      (Z(0.4, 29.7, 47.4, YB), True, [
        ('U1','URB4805S',22.0,9.6,'v24'), ('C1','470µF 50V',11.0,11.0,'v24'),
        ('D1','1N5822',*DB,'v24',1), ('D2','1N5822',*DB,'v24',1), ('C2','22µF 50V',6.0,6.0,'v24'),
        ('C4','22µF 25V',6.0,5.4,'sig',1), ('C3','100nF',*CD,'v24',1), ('C5','100nF',*CD,'sig',1)]),
    ]
    for n, x1, x2 in [(1, 58.6, 79.4), (2, 79.4, 100.6)]:
        k = n - 1
        jobs.append((Z(x1, 29.7, x2, YB), True, [
            ('U'+str(n+3),'PC817',10.2,6.6,'sig',1), (f'R{22+2*k}','2.2k ½W',4.6,8.4,'v24',1), (f'F{n}','PTC 0.1A',*PTC,'hot',1),
            (f'D{9+2*k}','P6KE30A',*TV,'v24',1), (f'D{10+2*k}','P6KE30A',*TV,'v24',1), (f'D{7+k}','1N4148',*DV,'v24',1)]))
    for zone, up, items in jobs:
        good, why = pack(zone, items, up)
        if not good: return None, None, why
    return P + placed, H, ''

# ============================================================
#  검사: 기판 안, 겹침, 발열 부품 여유
# ============================================================
# 가장 낮은 기판 높이 찾기
for BH in [x/2 for x in range(110, 161)]:
    P, MH, why = build(BH)
    if P: break
    print(f'BH {BH}: 안 들어감 {why}')
TY = BH - 8.6
errs = []
for q in P:
    x1,y1,x2,y2 = box(q)
    if x1 < 0 or y1 < 0 or x2 > BW or y2 > BH: errs.append(f'기판 밖: {q[0]} {q[1]}')
    if q[6] == 'under' and not (2.0 < x1 and x2 < 58.4 and 4.3 < y1 and y2 < 24.56): errs.append(f'ESP32 밑 영역 밖: {q[0]} {q[1]}')
for a, b in itertools.combinations(P, 2):
    if 'esp' in (a[6], b[6]) and 'under' in (a[6], b[6]): pass
    m = GAP/2
    if a[6] == 'hot' or b[6] == 'hot': m = HOT/2 if a[6] != b[6] else GAP/2
    if hit(box(a, m), box(b, m)): errs.append(f'겹침: {a[0]} {a[1]} / {b[0]} {b[1]}')
# ESP32 밑에는 under 부품만
EZ = (-11.0, 0.0, 60.0, 29.2)
for q in P:
    if q[6] not in ('under', 'esp') and hit(box(q), EZ): errs.append(f'ESP32 보드와 겹침: {q[0]} {q[1]}')
for hx, hy in MH:
    for q in P:
        if hit(box(q), (hx-HOLE, hy-HOLE, hx+HOLE, hy+HOLE)): errs.append(f'구멍과 겹침: {q[0]} {q[1]}')
print('\n'.join(errs) if errs else '검사 통과')
print(f'기판 {BW:.0f} × {BH:.0f} mm, 부품 {len(P)}개')

# ============================================================
#  그림
# ============================================================
def X(v): return round(OX + v*S, 1)
def Y(v): return round(OY + v*S, 1)
FILL = {'v24':('#fee2e2','#b91c1c'), 'sig':('#e0f2fe','#0369a1'), 'hot':('#fed7aa','#c2410c'),
        'under':('#ede9fe','#6d28d9'), 'conn':('#f1f5f9','#334155'), 'esp':('#e0e7ff','#3730a3')}
T(24,40,'측창 모터 제어기 · 제어 기판 부품 배치도 (손납땜 PCB, 스루홀)',24,'ph',bold=True)
T(24,66,f'위에서 본 그림 · 기판 {BW:.0f} × {BH:.0f} mm, 2층 · 회로도 schematic/제어기_회로도의 부품을 따름 · 상자는 부품이 차지하는 자리(외곽)이며 배선은 그리지 않음 · 2026-10-24',13,'muted')
T(24,88,'부품 크기는 흔한 품번 기준 추정이다. ESP32 소켓 줄 간격, URB4805S, 피에조는 받은 실물로 재서 맞춘다.',13,'muted')
# 범례
lx = 24
for k, name in [('v24','24V 쪽'),('sig','3.3V·5V 신호 쪽'),('hot','뜨거워질 수 있음 (둘레 2.5mm 비움)'),('under','ESP32 밑 (높이 5mm 이하)'),('conn','단자·커넥터')]:
    f, st = FILL[k]
    O.append(f'<rect x="{lx}" y="106" width="22" height="14" rx="2" fill="{f}" stroke="{st}" stroke-width="1.4"/>')
    T(lx+28, 118, name, 12, 'muted'); lx += 50 + len(name)*12.5
# 기판
O.append(f'<rect x="{X(0)}" y="{Y(0)}" width="{BW*S}" height="{BH*S}" rx="{1.5*S}" fill="#f0fdf4" stroke="#166534" stroke-width="2.2"/>')
# 눈금 (10mm)
for v in range(0, int(BW)+1, 10):
    O.append(f'<path d="M{X(v)},{Y(0)-4} V{Y(0)-12}" stroke="#94a3b8" stroke-width="1"/>'); T(X(v), Y(0)-16, str(v), 10, 'muted', 'middle')
for v in range(0, int(BH)+1, 10):
    O.append(f'<path d="M{X(0)-4},{Y(v)} H{X(0)-12}" stroke="#94a3b8" stroke-width="1"/>'); T(X(0)-16, Y(v)+4, str(v), 10, 'muted', 'end')
# 고정 구멍 M3
# ESP32 개발 보드 윤곽과 안테나
O.append(f'<rect x="{X(-11)}" y="{Y(0)}" width="{71*S}" height="{29.2*S}" rx="4" fill="none" stroke="#3730a3" stroke-width="1.6" stroke-dasharray="8 4"/>')
O.append(f'<rect x="{X(-11)}" y="{Y(3)}" width="{11*S}" height="{23*S}" fill="#c7d2fe" fill-opacity=".5" stroke="#3730a3" stroke-width="1"/>')
T(X(-5.5), Y(15)+4, '안테나', 11, 'tsig', 'middle', True); T(X(-5.5), Y(17.5)+4, '(기판 밖)', 10, 'muted', 'middle')
T(X(30), Y(-3.2), 'U2 ESP32-S3-DevKitC-1 (J11·J12 소켓에 꽂음, 점선)', 12, 't', 'middle', True)
T(X(60.5), Y(14), 'USB →', 11, 't', 'start', True)
for q in P:
    ref, sub, x, y, w, h, kind = q
    f, st = FILL[kind]
    if kind == 'hot':
        O.append(f'<rect x="{X(x-HOT)}" y="{Y(y-HOT)}" width="{(w+2*HOT)*S}" height="{(h+2*HOT)*S}" rx="3" fill="none" stroke="#c2410c" stroke-width="1" stroke-dasharray="4 3"/>')
    op = ' fill-opacity=".85"' if kind == 'under' else ''
    O.append(f'<rect x="{X(x)}" y="{Y(y)}" width="{w*S}" height="{h*S}" rx="2" fill="{f}"{op} stroke="{st}" stroke-width="1.3"/>')
    cx, cy = X(x + w/2), Y(y + h/2)
    small = min(w, h) < 4.5
    one = f'{ref} {sub}' if sub else ref
    if w >= h or kind in ('conn','esp'):
        if small:
            T(cx, cy + 3.5, one if len(one)*6.2 < w*S - 4 else ref, 10, 't', 'middle', True)
        else:
            T(cx, cy + (3.5 if not sub else -1), ref, 11, 't', 'middle', True)
            if sub: T(cx, cy + 11, sub, 9, 'muted', 'middle')
    else:
        lab = one if len(one)*5.8 < h*S - 4 else ref
        O.append(f'<text class="t" x="{cx}" y="{cy}" font-size="9.5" font-weight="bold" text-anchor="middle" font-family="{FK}" transform="rotate(-90 {cx} {cy})" dy="3.5">{esc(lab)}</text>')
for hi, (mx, my) in enumerate(MH):
    O.append(f'<circle cx="{X(mx)}" cy="{Y(my)}" r="{1.6*S}" fill="#ffffff" stroke="#166534" stroke-width="1.4"/><circle cx="{X(mx)}" cy="{Y(my)}" r="{3.2*S}" fill="none" stroke="#166534" stroke-width="1" stroke-dasharray="3 3"/>')
    T(X(mx), Y(my)+4, f'H{hi+1}', 10, 'muted', 'middle', True)
# 치수
O.append(f'<path d="M{X(0)},{Y(BH)+30} H{X(BW)} M{X(0)},{Y(BH)+22} V{Y(BH)+38} M{X(BW)},{Y(BH)+22} V{Y(BH)+38}" stroke="#166534" stroke-width="1.4"/>')
T((X(0)+X(BW))/2, Y(BH)+48, f'{BW:.0f} mm', 13, 't', 'middle', True)
O.append(f'<path d="M{X(BW)+30},{Y(0)} V{Y(BH)} M{X(BW)+22},{Y(0)} H{X(BW)+38} M{X(BW)+22},{Y(BH)} H{X(BW)+38}" stroke="#166534" stroke-width="1.4"/>')
T(X(BW)+44, (Y(0)+Y(BH))/2, f'{BH:.0f} mm', 13, 't', 'start', True)
WD = max(int(X(BW) + 120), int(lx) + 20); NY = int(Y(BH) + 80)
NOTES = []
def note(s): NOTES.append(s)
exec(open('pcb_notes.py', encoding='utf-8').read()) if __import__('os').path.exists('pcb_notes.py') else None
O.append(f'<rect class="panel" x="20" y="{NY-26}" width="{WD-40}" height="{34 + 26*len(NOTES)}" rx="8"/>')
T(34, NY, '배치 원칙과 안전 간격', 15, 'ph', bold=True)
for i, n in enumerate(NOTES): T(34, NY + 26 + i*26, '• ' + n, 12.5, 't')
HT = NY + 26*len(NOTES) + 30
out = sys.argv[1] if len(sys.argv) > 1 else '제어기_부품배치도.svg'
open(out, 'w', encoding='utf-8').write(svg(WD, HT, '측창 모터 제어기 제어 기판 부품 배치도'))
