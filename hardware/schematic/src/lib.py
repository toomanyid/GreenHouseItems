import math
FK="'Noto Sans CJK KR','Noto Sans KR','Malgun Gothic','Apple SD Gothic Neo',sans-serif"
FM="'DejaVu Sans Mono','Consolas','Menlo',monospace"
O=[]
def esc(s): return s.replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')
def T(x,y,s,size=12,cls='t',anchor='start',bold=False,mono=False):
    O.append(f'<text class="{cls}" x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" font-weight="{"bold" if bold else "normal"}" font-family="{FM if mono else FK}">{esc(s)}</text>')
def W(*p): O.append('<polyline class="w" points="'+' '.join(f'{a},{b}' for a,b in p)+'"/>')
def OFF(*p): O.append('<polyline class="off" stroke-dasharray="6 4" points="'+' '.join(f'{a},{b}' for a,b in p)+'"/>')
def DOT(x,y): O.append(f'<circle class="dot" cx="{x}" cy="{y}" r="3.2"/>')
def NC(x,y): O.append(f'<path class="nc" d="M{x-5},{y-5} L{x+5},{y+5} M{x-5},{y+5} L{x+5},{y-5}"/>')
def GND(x,y,lead=8):
    W((x,y),(x,y+lead)); y+=lead
    O.append(f'<path class="w" d="M{x-10},{y} H{x+10} M{x-6},{y+4} H{x+6} M{x-2},{y+8} H{x+2}"/>')
def PANEL(x,y,w,h,title,sub=None):
    O.append(f'<rect class="panel" x="{x}" y="{y}" width="{w}" height="{h}" rx="8"/>')
    T(x+14,y+24,title,16,'ph',bold=True)
    if sub: T(x+14,y+43,sub,12,'muted')
def NET(x,y,name,kind='sig',d='r'):
    w=len(name)*7.25+16; c='nsig' if kind=='sig' else 'npwr'; tc='tsig' if kind=='sig' else 'tpwr'
    if d=='r': pts=[(x,y),(x+8,y-10),(x+w,y-10),(x+w,y+10),(x+8,y+10)]; tx,ty=x+4+w/2,y+4.5
    elif d=='l': pts=[(x,y),(x-8,y-10),(x-w,y-10),(x-w,y+10),(x-8,y+10)]; tx,ty=x-4-w/2,y+4.5
    elif d=='u': w=w-4; pts=[(x,y),(x+w/2,y-8),(x+w/2,y-28),(x-w/2,y-28),(x-w/2,y-8)]; tx,ty=x,y-13
    else: w=w-4; pts=[(x,y),(x+w/2,y+8),(x+w/2,y+28),(x-w/2,y+28),(x-w/2,y+8)]; tx,ty=x,y+23
    O.append(f'<polygon class="{c}" points="'+' '.join(f'{a:.1f},{b:.1f}' for a,b in pts)+'"/>')
    T(round(tx,1),round(ty,1),name,12,tc,'middle',True,True)
def _g(x1,y1,x2,y2,inner):
    L=math.hypot(x2-x1,y2-y1); a=math.degrees(math.atan2(y2-y1,x2-x1))
    O.append(f'<g transform="translate({x1},{y1}) rotate({a:.1f})">'+inner(L)+'</g>')
def R(x1,y1,x2,y2,ptc=False):
    def f(L):
        m=L/2; s=f'<path class="w" d="M0,0 H{m-15} M{m+15},0 H{L}"/><rect class="sym" x="{m-15}" y="-6" width="30" height="12"/>'
        if ptc: s+=f'<path class="w" d="M{m-20},10 L{m+14},-12 H{m+20}"/>'
        return s
    _g(x1,y1,x2,y2,f)
def C(x1,y1,x2,y2,pol=False):
    def f(L):
        m=L/2; s=f'<path class="w" d="M0,0 H{m-4} M{m+4},0 H{L}"/><path class="w" d="M{m-4},-11 V11"/>'
        s+= (f'<path class="w" d="M{m+7},-11 Q{m+3},0 {m+7},11"/>' if pol else f'<path class="w" d="M{m+4},-11 V11"/>')
        if pol: s+=f'<path class="w" d="M{m-12},-14 h6 M{m-9},-17 v6"/>'
        return s
    _g(x1,y1,x2,y2,f)
def D(x1,y1,x2,y2,kind='d'):
    # anode x1,y1 -> cathode x2,y2
    def f(L):
        m=L/2; s=f'<path class="w" d="M0,0 H{m-8} M{m+8},0 H{L}"/><polygon class="symf" points="{m-8},-8 {m-8},8 {m+8},0"/>'
        if kind=='d': s+=f'<path class="w" d="M{m+8},-8 V8"/>'
        elif kind=='s': s+=f'<path class="w" d="M{m+12},-8 H{m+8} V8 H{m+4}"/>'
        elif kind=='z': s+=f'<path class="w" d="M{m+12},-11 L{m+8},-8 V8 L{m+4},11"/>'
        return s
    _g(x1,y1,x2,y2,f)
def JMP(x1,y1,x2,y2):
    def f(L):
        m=L/2; return f'<path class="w" d="M0,0 H{m-10} M{m+10},0 H{L}"/><circle class="sym" cx="{m-7}" cy="0" r="3.5"/><circle class="sym" cx="{m+7}" cy="0" r="3.5"/><path class="w" d="M{m-9},-6 Q{m},-16 {m+9},-6"/>'
    _g(x1,y1,x2,y2,f)
def NMOS(gx,gy,label=None,sub=None):
    dx=gx+30
    O.append(f'<path class="w" d="M{gx},{gy} H{gx+14} M{gx+14},{gy-12} V{gy+12} M{gx+20},{gy-16} V{gy-8} M{gx+20},{gy-4} V{gy+4} M{gx+20},{gy+8} V{gy+16} M{gx+20},{gy-12} H{dx} V{gy-30} M{gx+20},{gy+12} H{dx} V{gy+30} M{gx+20},{gy} H{dx} V{gy+12}"/>')
    O.append(f'<polygon class="symf" points="{gx+21},{gy} {gx+28},{gy-4} {gx+28},{gy+4}"/>')
    if label: T(dx+12,gy-4,label,12,'t',bold=True)
    if sub: T(dx+12,gy+12,sub,11,'muted')
    return (dx,gy-30),(dx,gy+30)
def TB(x,y,n,pitch,names,title=None,sub=None):
    # screw terminal; returns connection points on right
    h=n*pitch; O.append(f'<rect class="term" x="{x}" y="{y}" width="28" height="{h}" rx="3"/>')
    pts=[]
    for i in range(n):
        cy=y+pitch*i+pitch/2
        O.append(f'<circle class="sym" cx="{x+14}" cy="{cy}" r="7"/><path class="w" d="M{x+9},{cy+5} L{x+19},{cy-5}"/>')
        OFF((x-36,cy),(x,cy))
        if names[i]: T(x+33,cy-6,names[i],10,'muted')
        pts.append((x+28,cy))
    if title: T(x,y-10,title,12,'t',bold=True)
    if sub: T(x,y+h+16,sub,11,'muted')
    return pts
def HDR(x,y,n,pitch,names,title=None,side='l',w=100,sub=None):
    h=n*pitch; O.append(f'<rect class="mod" x="{x}" y="{y}" width="{w}" height="{h}" rx="4"/>')
    pts=[]
    for i in range(n):
        cy=y+pitch*i+pitch/2
        if side=='l':
            W((x-14,cy),(x,cy)); T(x+6,cy+4,f'{i+1} {names[i]}',11,'t',mono=True); pts.append((x-14,cy))
        else:
            W((x+w,cy),(x+w+14,cy)); T(x+w-6,cy+4,f'{names[i]} {i+1}',11,'t','end',mono=True); pts.append((x+w+14,cy))
    if title: T(x,y-10,title,12,'t',bold=True)
    if sub: T(x,y+h+16,sub,11,'muted')
    return pts
def BOX(x,y,w,h,title=None,sub=None):
    O.append(f'<rect class="mod" x="{x}" y="{y}" width="{w}" height="{h}" rx="5"/>')
    if title: T(x+w/2,y-24 if sub else y-10,title,12,'t','middle',True)
    if sub: T(x+w/2,y-9,sub,11,'muted','middle')
def PIN(x,y,name,side,stub=20):
    if side=='l': W((x-stub,y),(x,y)); T(x+6,y+4,name,11,'t',mono=True); return (x-stub,y)
    if side=='r': W((x,y),(x+stub,y)); T(x-6,y+4,name,11,'t','end',mono=True); return (x+stub,y)
    if side=='b': W((x,y),(x,y+stub)); T(x,y-6,name,11,'t','middle',mono=True); return (x,y+stub)
def svg(wd,ht,title):
    head=f'''<svg xmlns="http://www.w3.org/2000/svg" width="{wd}" height="{ht}" viewBox="0 0 {wd} {ht}">
<title>{title}</title><defs><style>
.bg{{fill:#ffffff}}.panel{{fill:#fbfcfe;stroke:#cbd5e1;stroke-width:1.2}}.w{{fill:none;stroke:#1f2937;stroke-width:1.6;stroke-linejoin:round}}
.off{{fill:none;stroke:#9a3412;stroke-width:2.4}}.sym{{fill:#ffffff;stroke:#1f2937;stroke-width:1.6}}.symf{{fill:#1f2937;stroke:#1f2937;stroke-width:1}}
.dot{{fill:#1f2937}}.mod{{fill:#eef2ff;stroke:#3730a3;stroke-width:1.6}}.term{{fill:#f1f5f9;stroke:#475569;stroke-width:1.4}}
.nsig{{fill:#e0f2fe;stroke:#0369a1;stroke-width:1.2}}.npwr{{fill:#fee2e2;stroke:#b91c1c;stroke-width:1.2}}.nc{{stroke:#94a3b8;stroke-width:1.6;fill:none}}
.t{{fill:#111827}}.ph{{fill:#0f172a}}.muted{{fill:#64748b}}.tsig{{fill:#075985}}.tpwr{{fill:#991b1b}}.pin{{fill:#ffffff;stroke:#1f2937;stroke-width:1.4}}.shunt{{fill:#fde68a;stroke:#92400e;stroke-width:1.4;fill-opacity:.7}}
</style></defs><rect class="bg" x="0" y="0" width="{wd}" height="{ht}"/>
'''
    return head+'\n'.join(O)+'\n</svg>\n'
