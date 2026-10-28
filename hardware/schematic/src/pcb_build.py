# KiCad 기판 초안 만들기: 연결표(pcb_netlist) + 배치도(pcb_layout) → 제어기.kicad_pcb
# 1단계: python3 pcb_build.py place  → 부품 배치, 판 외곽, 구멍, 안테나 쪽 금지 구역, Specctra DSN
# 2단계: (Freerouting으로 DSN → SES)
# 3단계: python3 pcb_build.py finish → SES 가져오기, GND 면 채우기, 저장
import sys, os, re
import pcbnew
from pcb_netlist import FP, NETS, POWER, MID

LIB = '/usr/share/kicad/footprints'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'pcb_out'
os.makedirs(OUT, exist_ok=True)
PCB = os.path.join(OUT, 'controller.kicad_pcb')
# 배치도 상자보다 실제 풋프린트가 커서 패드가 닿는 곳만 조금 옮긴다 (mm)
NUDGE = {'D1': (-0.8, 0), 'D2': (0.8, 0), 'D3': (0, 1.2), 'R17': (0, -0.4), 'D11': (-0.6, 0)}
O = (30.0, 30.0)                        # 판 왼쪽 위를 KiCad 좌표 (30, 30) mm에
mm = pcbnew.FromMM
CU2 = pcbnew.LSET(); CU2.AddLayer(pcbnew.F_Cu); CU2.AddLayer(pcbnew.B_Cu)
def P(x, y): return pcbnew.VECTOR2I(mm(O[0] + x), mm(O[1] + y))

def layout():
    s = open('pcb_layout.py', encoding='utf-8').read(); g = {}
    exec(s[:s.index('errs = []')], g)
    return g['P'], g['MH'], g['BW'], g['BH']

def make_jp2():
    fp = pcbnew.FOOTPRINT(None)
    fp.SetFPID(pcbnew.LIB_ID('local', 'PinHeader_3x03_P2.54mm'))
    for r in range(3):
        for c in range(3):
            pad = pcbnew.PAD(fp); pad.SetNumber(str(3*r + c + 1))
            pad.SetShape(pcbnew.PAD_SHAPE_RECT if r == c == 0 else pcbnew.PAD_SHAPE_CIRCLE)
            pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH); pad.SetLayerSet(pad.PTHMask())
            pad.SetSize(pcbnew.VECTOR2I(mm(1.5), mm(1.5)))   # 가운데 핀에서 선이 빠져나오게 조금 작게; pad.SetDrillSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
            v = pcbnew.VECTOR2I(mm(c*2.54), mm(r*2.54)); pad.SetPos0(v); pad.SetPosition(v); fp.Add(pad)
    return fp

def load(ref):
    lib, name = FP[ref]
    if name == 'PinHeader_3x03_P2.54mm_Vertical': return make_jp2()
    return pcbnew.FootprintLoad(os.path.join(LIB, lib + '.pretty'), name)

def bbox(fp):
    b = fp.GetBoundingBox(False, False)
    return pcbnew.ToMM(b.GetLeft()), pcbnew.ToMM(b.GetTop()), pcbnew.ToMM(b.GetRight()), pcbnew.ToMM(b.GetBottom())

def pin1_ok(fp, w, h):
    # 커넥터는 1번 핀이 왼쪽(가로로 긴 상자) 또는 위(세로로 긴 상자)
    pads = sorted(fp.Pads(), key=lambda p: int(p.GetNumber()) if p.GetNumber().isdigit() else 0)
    if len(pads) < 2: return True
    a, z = pads[0].GetPosition(), pads[-1].GetPosition()
    return (a.x < z.x) if w >= h else (a.y < z.y)

def place():
    B = pcbnew.BOARD()
    ds = B.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    nets = {}
    for ref in NETS:
        for n in NETS[ref].values():
            if n not in nets:
                ni = pcbnew.NETINFO_ITEM(B, n); B.Add(ni); nets[n] = ni
    Pl, MH, BW, BH = layout()
    boxes = {q[0]: q for q in Pl}
    for i, (hx, hy) in enumerate(MH):
        boxes[f'H{i+1}'] = (f'H{i+1}', '', hx - 3.2, hy - 3.2, 6.4, 6.4, 'hole')
    for ref in FP:
        q = boxes[ref]; _, sub, x, y, w, h, kind = q
        best = None
        for rot in (0, 90, 180, 270):
            fp = load(ref); fp.SetOrientationDegrees(rot)
            l, t, r_, b_ = bbox(fp); fw, fh = r_ - l, b_ - t
            score = abs((fw >= fh) - (w >= h)) * 10 + (0 if pin1_ok(fp, w, h) else 1) + rot / 1000
            if best is None or score < best[0]: best = (score, rot)
        fp = load(ref); fp.SetOrientationDegrees(best[1]); fp.SetReference(ref)
        val = (sub.split(' ')[0] if sub else '').replace('Ω', 'R').replace('µ', 'u')
        fp.SetValue(val if val.isascii() and val else FP[ref][1][:16])
        B.Add(fp)
        l, t, r_, b_ = bbox(fp)
        if kind in ('conn', 'esp'):       # 커넥터는 패드 줄을 상자 가운데에 (단자대는 이웃과 이어 붙는 피치 기준)
            xs = [pcbnew.ToMM(p_.GetPosition().x) for p_ in fp.Pads()]; ys = [pcbnew.ToMM(p_.GetPosition().y) for p_ in fp.Pads()]
            l, t, r_, b_ = min(xs), min(ys), max(xs), max(ys)
        cx, cy = (l + r_) / 2, (t + b_) / 2
        dx, dy = NUDGE.get(ref, (0, 0)); cx -= dx; cy -= dy
        fp.Move(pcbnew.VECTOR2I(mm(O[0] + x + w/2 - cx), mm(O[1] + y + h/2 - cy)))
        for pad in fp.Pads():
            n = NETS[ref].get(pad.GetNumber())
            if n: pad.SetNet(nets[n])
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.9), mm(0.9))); fp.Reference().SetTextThickness(mm(0.15))
        fp.Value().SetVisible(False)
    # 판 외곽 (모서리 R1.5)
    seg = lambda a, b: _line(B, a, b)
    r = 1.5
    for a, b in [((r, 0), (BW - r, 0)), ((BW, r), (BW, BH - r)), ((BW - r, BH), (r, BH)), ((0, BH - r), (0, r))]: seg(a, b)
    for cx, cy, sa in [(r, r, 180), (BW - r, r, 270), (BW - r, BH - r, 0), (r, BH - r, 90)]:
        arc = pcbnew.PCB_SHAPE(B); arc.SetShape(pcbnew.SHAPE_T_ARC); arc.SetLayer(pcbnew.Edge_Cuts); arc.SetWidth(mm(0.1))
        arc.SetCenter(P(cx, cy)); import math
        arc.SetStart(P(cx + r*math.cos(math.radians(sa)), cy + r*math.sin(math.radians(sa))))
        arc.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T)); B.Add(arc)
    # 안테나 쪽 왼쪽 끝 2 mm (ESP32 보드 밑): 배선·비아·구리 면 금지
    ka = pcbnew.ZONE(B); ka.SetIsRuleArea(True); ka.SetDoNotAllowTracks(True); ka.SetDoNotAllowVias(True)
    ka.SetDoNotAllowCopperPour(True); ka.SetDoNotAllowPads(False); ka.SetDoNotAllowFootprints(False)
    ka.SetLayerSet(CU2)
    ol = ka.Outline(); ol.NewOutline()
    for x, y in [(0, 0), (2.0, 0), (2.0, 29.2), (0, 29.2)]: ol.Append(mm(O[0] + x), mm(O[1] + y))
    B.Add(ka)
    # 판 가장자리 0.8 mm 띠: 배선 금지 (구리-가장자리 간격)
    for x1, y1, x2, y2 in [(0, 0, BW, 0.8), (0, BH - 0.8, BW, BH), (0, 0, 0.8, BH), (BW - 0.8, 0, BW, BH)]:
        ke = pcbnew.ZONE(B); ke.SetIsRuleArea(True); ke.SetDoNotAllowTracks(True); ke.SetDoNotAllowVias(True)
        ke.SetDoNotAllowCopperPour(False); ke.SetDoNotAllowPads(False); ke.SetDoNotAllowFootprints(False)
        ke.SetLayerSet(CU2); ol = ke.Outline(); ol.NewOutline()
        for x, y in [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]: ol.Append(mm(O[0] + x), mm(O[1] + y))
        B.Add(ke)
    # 규칙: 최소 배선 0.3, 간격 0.3, 비아 1.0/0.5 (손납땜, 2층 일반 공정)
    ds.m_TrackMinWidth = mm(0.3); ds.m_MinClearance = mm(0.3)
    ds.m_ViasMinSize = mm(1.0); ds.m_MinThroughDrill = mm(0.5)
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetTrackWidth(mm(0.4)); nc.SetClearance(mm(0.3)); nc.SetViaDiameter(mm(1.0)); nc.SetViaDrill(mm(0.5))
    B.SetFileName(PCB); pcbnew.SaveBoard(PCB, B)
    dsn = os.path.join(OUT, 'board.dsn')
    pcbnew.ExportSpecctraDSN(B, dsn)
    patch_dsn(dsn)
    print('배치 끝:', len(FP), '부품 →', PCB)

def _line(B, a, b):
    s = pcbnew.PCB_SHAPE(B); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1))
    s.SetStart(P(*a)); s.SetEnd(P(*b)); B.Add(s)

def patch_dsn(path):
    # 넷 굵기: 24V·GND·코일 1.5 mm (간격 0.5), +5V 0.8 mm, 나머지 0.4 mm. Specctra 단위는 µm
    t = open(path, encoding='utf-8').read()
    q = lambda n: f'"{n}"' if re.search(r'[^A-Za-z0-9_+]', n) else n
    def cls(name, nets, w, c):
        return f'    (class {name} {" ".join(q(n) for n in sorted(nets))}\n      (circuit (use_via "Via[0-1]_1000:500_um"))\n      (rule (width {w}) (clearance {c}))\n    )\n'
    add = cls('power', POWER - {'GND'}, 1000, 400) + cls('gnd', {'GND'}, 800, 300) + cls('mid', MID, 600, 300) + cls('narrow', {'M1_SEL', 'M2_SEL', 'IO39'}, 300, 300)
    # 기존 클래스에서 이 넷들을 빼고, 새 클래스를 network 끝에 더한다
    allspecial = POWER | MID | {'GND', 'M1_SEL', 'M2_SEL', 'IO39'}
    def strip(m):
        body = m.group(0)
        for n in allspecial:
            for form in {n, f'"{n}"'}: body = re.sub(r'(?<=\s)' + re.escape(form) + r'(?=[\s)])', '', body)
        return body
    t = re.sub(r'\(class kicad_default[^()]*', strip, t)
    i = t.rindex('(wiring')
    j = t.rindex(')', 0, i)            # network 블록의 닫는 괄호
    t = t[:j] + add + '  ' + t[j:]
    open(path, 'w', encoding='utf-8').write(t)

def finish():
    B = pcbnew.LoadBoard(PCB)
    ses = os.path.join(OUT, 'board.ses')
    print('SES 가져오기:', import_ses(B, ses))
    Pl, MH, BW, BH = layout(); add_gnd(B, BW, BH)
    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(PCB, B)
    print('저장:', PCB, '배선', len(B.GetTracks()))

def import_ses(B, path):
    # Specctra SES(0.1 µm 단위, y는 위가 +)를 배선·비아로 옮긴다
    t = open(path, encoding='utf-8').read()
    t = t[t.index('(network_out'):]
    toks = re.findall(r'"[^"]*"|\(|\)|[^\s()]+', t)
    i = 0; net = None; n_tr = n_via = 0
    u = lambda v: int(round(int(v) * 100))          # 0.1 µm → nm
    while i < len(toks):
        if toks[i] == '(' and toks[i+1] == 'net':
            net = B.FindNet(toks[i+2].strip('"')); i += 3; continue
        if toks[i] == '(' and toks[i+1] == 'path':
            layer = B.GetLayerID(toks[i+2]); w = u(toks[i+3]); j = i + 4; pts = []
            while toks[j] != ')': pts.append((u(toks[j]), -u(toks[j+1]))); j += 2
            for a, b in zip(pts, pts[1:]):
                tr = pcbnew.PCB_TRACK(B); tr.SetStart(pcbnew.VECTOR2I(*a)); tr.SetEnd(pcbnew.VECTOR2I(*b))
                tr.SetWidth(w); tr.SetLayer(layer); tr.SetNet(net); B.Add(tr); n_tr += 1
            i = j; continue
        if toks[i] == '(' and toks[i+1] == 'via':
            v = pcbnew.PCB_VIA(B); v.SetPosition(pcbnew.VECTOR2I(u(toks[i+3]), -u(toks[i+4])))
            v.SetWidth(mm(1.0)); v.SetDrill(mm(0.5)); v.SetViaType(pcbnew.VIATYPE_THROUGH)
            v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); v.SetNet(net); B.Add(v); n_via += 1
            i += 5; continue
        i += 1
    return f'배선 {n_tr}, 비아 {n_via}'

def add_gnd(B, BW, BH):
    gnd = B.FindNet('GND')
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(B); z.SetLayer(layer); z.SetNet(gnd)
        z.SetLocalClearance(mm(0.5)); z.SetMinThickness(mm(0.3))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(mm(0.5)); z.SetThermalReliefSpokeWidth(mm(0.6))
        ol = z.Outline(); ol.NewOutline()
        for x, y in [(0.3, 0.3), (BW - 0.3, 0.3), (BW - 0.3, BH - 0.3), (0.3, BH - 0.3)]: ol.Append(mm(O[0] + x), mm(O[1] + y))
        B.Add(z)

if __name__ == '__main__':
    {'place': place, 'finish': finish}[sys.argv[1]]()
