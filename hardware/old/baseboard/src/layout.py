from lib import *
S=17; X0=110; Y0=130; NC_,NR=59,39
def X(c): return X0+c*S
def Y(r): return Y0+r*S
def zone(c1,r1,c2,r2,fill,label):
    O.append(f'<rect x="{X(c1)-S/2}" y="{Y(r1)-S/2}" width="{(c2-c1+1)*S}" height="{(r2-r1+1)*S}" rx="6" fill="{fill}" fill-opacity=".45" stroke="none"/>')
    T(X(c1)-S/2+5,Y(r1)-S/2+13,label,11,'muted',bold=True)
def part(c1,r1,c2,r2,ref,sub=None,cls='mod',size=11):
    x=X(c1)-S/2+2; y=Y(r1)-S/2+2; w=(c2-c1+1)*S-4; h=(r2-r1+1)*S-4
    O.append(f'<rect class="{cls}" x="{x}" y="{y}" width="{w}" height="{h}" rx="3"/>')
    cy=y+h/2+(0 if sub else 4)
    T(x+w/2,cy-1 if sub else cy,ref,size,'t','middle',True)
    if sub: T(x+w/2,cy+12,sub,9.5,'muted','middle')
def holes(pts,sq=False):
    for c,r in pts:
        if sq: O.append(f'<rect class="pin" x="{X(c)-4}" y="{Y(r)-4}" width="8" height="8"/>')
        else: O.append(f'<circle class="pin" cx="{X(c)}" cy="{Y(r)}" r="3.5"/>')
def circ(cx,cy,rad,ref,sub=None):
    O.append(f'<circle class="mod" cx="{X(cx)}" cy="{Y(cy)}" r="{rad*S}"/>'); T(X(cx),Y(cy)+(0 if sub else 4),ref,11,'t','middle',True)
    if sub: T(X(cx),Y(cy)+13,sub,9.5,'muted','middle')
def term(c1,n,ref,names):
    c2=c1+2*n
    O.append(f'<rect class="term" x="{X(c1)-S/2+2}" y="{Y(34)}" width="{(c2-c1)*S-4+S}" height="{4*S}" rx="3"/>')
    pins=[(c1+1+2*i,36) for i in range(n)]; holes(pins,True)
    T((X(c1)+X(c2))/2,Y(35)-2,ref,11,'t','middle',True)
    for (c,r),nm in zip(pins,names): T(X(c),Y(38)-3,nm,8.5,'muted','middle')
    OFF(((X(c1)+X(c2))/2,Y(38)+S/2),((X(c1)+X(c2))/2,Y(38)+S/2+28))
def xh(r1,n,ref,names):
    O.append(f'<rect class="term" x="{X(56)-S/2}" y="{Y(r1)-S/2+1}" width="{3*S+6}" height="{n*S-2}" rx="3"/>')
    pins=[(57,r1+i) for i in range(n)]; holes(pins,True)
    T(X(55)-6,Y(r1)+4,ref,11,'t','end',True)
    for (c,r),nm in zip(pins,names): T(X(58)+4,Y(r)+3,nm,8.5,'muted')

T(24,40,'측창 모터 제어기 · 베이스 보드 부품 배치도 (만능기판 10×15cm, 초기 모델)',24,'ph',bold=True)
T(24,64,'위에서 본 그림 · 구멍 간격 2.54mm · 59×39 구멍 기준 · 회로도 baseboard/베이스보드_회로도의 부품 번호를 따름 · 자리 잡기용이며 배선 경로는 표시하지 않음',13,'muted')
T(24,84,'아래쪽 = 현장 배선(나사 단자대), 오른쪽 = 함 안 케이블(XH 커넥터), 왼쪽 위 = ESP32 (안테나는 기판 밖으로)',13,'muted')
# board
O.append(f'<rect x="{X(0)-S/2}" y="{Y(0)-S/2}" width="{NC_*S}" height="{NR*S}" fill="#f7f3e8" stroke="#a16207" stroke-width="2" rx="4"/>')
dots=''.join(f'<circle cx="{X(c)}" cy="{Y(r)}" r="1.6" fill="#d6c7a1"/>' for c in range(NC_) for r in range(NR))
O.append(dots)
for c in range(0,NC_,5): T(X(c),Y(0)-S/2-6,str(c),9,'muted','middle')
for r in range(0,NR,5): T(X(0)-S/2-6,Y(r)+3,str(r),9,'muted','end')
# zones
zone(0,15,8,33,'#fde68a','전원 (24V→5V→3.3V)')
zone(14,22,20,33,'#fed7aa','비상정지 감지 (J3)')
zone(21,22,39,33,'#bbf7d0','원점 센서 2개')
zone(40,22,43,33,'#bae6fd','온도')
zone(43,0,54,20,'#ddd6fe','드라이버 신호 · IS 저항')
zone(44,22,54,33,'#e0e7ff','레벨 변환 · 엔코더 풀업')
zone(25,15,39,21,'#f5f5f4','피에조')
zone(25,1,40,13,'#f5f5f4','모델 점퍼')
# ESP32
O.append(f'<rect x="{X(-3.6)}" y="{Y(1.5)}" width="{28.2*S}" height="{12*S}" rx="4" fill="#eef2ff" fill-opacity=".55" stroke="#3730a3" stroke-width="1.6" stroke-dasharray="6 3"/>')
O.append(f'<rect x="{X(-3.6)}" y="{Y(3.5)}" width="{3.1*S}" height="{8*S}" fill="url(#hat)" stroke="#3730a3" stroke-width="1"/>')
T(X(-2),Y(8)+4,'안테나',10,'tsig','middle',True)
holes([(c,3) for c in range(2,24)],True); holes([(c,12) for c in range(2,24)],True)
T(X(12.5),Y(7)+2,'ESP32-S3-DevKitC-1 (소켓: 암 핀헤더 1×22 두 줄)',13,'t','middle',True)
T(X(12.5),Y(8.5)+2,'줄 간격 9구멍(22.86mm)은 예시. 받은 보드로 재서 맞춘다',10.5,'muted','middle')
T(X(12.5),Y(10)+2,'안테나 끝을 기판 왼쪽 밖으로 내민다 · 그 밑과 주변에 구리, 부품 없음',10.5,'muted','middle')
T(X(24.2),Y(5)+4,'USB →',10,'t','end',True)
for c,lab in [(2,'R1 GND'),(10,'R9 IO39'),(19,'R18 IO21')]:
    T(X(c),Y(2.2),lab,8,'muted','middle')
for c,lab in [(2,'L1 3V3'),(10,'L9 IO17'),(17,'L16 IO11')]:
    T(X(c),Y(13.3)+4,lab,8,'muted','middle')
T(X(23),Y(13.3)+4,'L21 5V×  L22 GND',8,'muted','end')
# power parts
term(0,4,'J1 24V 입력',['24V_A','24V_B','GND','GND'])
part(1,32,4,32,'D1','1N5819'); part(5,32,8,32,'D2','1N5819')
circ(1.5,28.5,1.9,'C1','470µ'); circ(5,28.5,1,'C2',None); T(X(5),Y(30)+6,'22µ',9,'muted','middle'); part(7,28,7,29,'C3',None,size=8)
part(0,25,8,25,'U1 URB4805S (SIP, 세워 꽂음)',None,size=10)
part(0,22,1,23,'C4',None,size=8); part(2,22,2,23,'C5',None,size=8)
part(3,19,6,21,'U2','LD1117 TO-220')
part(0,19,1,20,'C7',None,size=8); part(7,19,8,20,'C8',None,size=8)
part(1,16,1,17,'C9',None,size=8); part(3,16,5,16,'JP3',None,'shunt',9)
T(X(4),Y(17.6)+4,'↑ L1(3V3)로 짧게',9,'muted','middle')
O.append(f'<path d="M{X(5)},{Y(33.6)} l5,-10 l5,10 z" fill="#b91c1c"/>'); T(X(5)+14,Y(33.6)-1,'GND 한 점',9,'tpwr',bold=True)
# sense J3
term(14,2,'J3 비상정지',['ESTOP','GND'])
part(14,29,17,29,'100k',None,size=9)
part(18,28,18,31,'10k',None,size=8); part(19,28,20,31,'100n',None,size=8)
part(14,24,16,25,'JP1','50W 닫음','shunt',10)
# sensors
term(22,4,'J5 센서1',['+24V','SIG','GND','예비'])
term(31,4,'J6 센서2',['+24V','SIG','GND','예비'])
for c0,u,n in [(21,'U4',1),(30,'U5',2)]:
    part(c0+1,31,c0+4,31,'PTC 0.1A',None,size=8); part(c0+5,31,c0+8,31,'P6KE30A',None,size=8)
    part(c0+5,29,c0+8,29,'P6KE30A',None,size=8); part(c0+1,29,c0+4,29,'2.2k ½W',None,size=8)
    part(c0+1,27,c0+3,27,'1N4148',None,size=8)
    part(c0+4,25,c0+6,27,u,'PC817'); holes([(c0+4,25),(c0+6,25),(c0+4,27),(c0+6,27)])
    part(c0+7,23,c0+8,27,'10k',None,size=8); T(X(c0+7.5),Y(25)+14,'10n',8,'muted','middle')
# temp
term(40,2,'J7 온도',['T+','T−'])
part(40,31,43,31,'PTC 0.05A',None,size=8); part(40,29,43,29,'P6KE30A',None,size=8)
part(40,26,43,27,'120Ω 1W',None,size=9); part(40,23,43,24,'10k·100n','BAT85',size=9)
# drivers
xh(1,8,'J8 XH 8P',['RPWM','LPWM','R_EN','L_EN','R_IS','L_IS','VCC','GND'])
xh(11,8,'J9 XH 8P',['RPWM','LPWM','R_EN','L_EN','R_IS','L_IS','VCC','GND'])
xh(23,7,'J10 XH 7P',['+5V','GND','SDA5','SCL5','ENC_A','ENC_B','ENC_SW'])
for r0,n in [(1,1),(11,2)]:
    part(44,r0+1,49,r0+3,f'풀다운 10k ×3',f'M{n} PWM·EN',size=10)
    part(44,r0+5,47,r0+8,'2.2k·1.3k',f'R_IS M{n}',size=10)
    part(48,r0+5,51,r0+8,'10k·1µF','BAT85',size=10)
part(26,4,28,6,'JP2','3×3','shunt',11); T(X(27),Y(7.4)+4,'50W ← | → 300W',9,'muted','middle')
part(30,4,32,6,'10k·10n','IO39',size=9)
T(X(26),Y(10)+4,'IO39 · M1_SEL · M2_SEL이 모이는 자리',9.5,'muted','start')
# level shifter, enc pullups
part(44,24,49,28,'U7','I2C 레벨 변환 모듈')
part(50,24,53,25,'+5V 100n',None,size=8)
part(44,30,53,32,'엔코더 풀업 10k ×3 · 10nF ×3',None,size=9)
# piezo
circ(28,18,2.4,'BZ','피에조 12mm')
part(32,17,34,19,'Q3','2N7000'); part(35,17,38,19,'100Ω·10k·1k',None,size=8)
# mounting holes
for c,r in [(0,0),(58,0),(0,38),(58,38)]:
    pass
O.append(f'<defs><pattern id="hat" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="8" stroke="#3730a3" stroke-width="2" opacity=".35"/></pattern></defs>')
# notes
nx=X(0)-S/2; ny=Y(38)+S/2+60
O.append(f'<rect class="panel" x="{nx-10}" y="{ny-24}" width="{NC_*S+20}" height="250" rx="8"/>')
notes=['자리 잡는 원칙: 현장 선이 들어오는 나사 단자대는 모두 아래쪽 한 줄, 함 안 케이블(드라이버 2개, 패널)은 오른쪽 XH 커넥터. 위쪽 절반은 3.3V 로직, 아래쪽 절반은 24V가 지나는 회로.',
'24V 선(J1 → D1·D2 → 24V_F → JP1, J5·J6 PTC, J7 PTC)은 기판 아래쪽에서만 돌리고, ESP32와 드라이버 신호선 밑으로 지나가지 않게 한다. 굵게(0.5sq 이상 점퍼선 또는 납 땜 덧댐).',
'GND: 아래쪽에 굵은 GND 줄을 하나 깔고 J1 옆 GND(▲ 표시) 한 곳에서 SMPS −로 나간다. 드라이버 J8·J9의 GND 핀과 ESP32 GND(L22, R1)는 이 줄에 따로 잇는다.',
'ESP32: 안테나 끝(L1·R1 쪽)이 기판 왼쪽 밖으로 나가게 둔다. 줄 간격은 받은 보드로 재고, 소켓을 꽂은 채로 납땜해 간격을 맞춘다. 3V3(L1) 옆에 C9, JP3를 가깝게.',
'크기: U1(URB4805S)과 C1(470µF), 피에조, 레벨 변환 모듈은 실물 크기로 다시 맞춘다. 남는 자리는 수정용 여유로 두고, 고정 구멍은 함에 맞춰 빈 모서리에 낸다.',
'JP2(모델 점퍼)는 IO39, M1_SEL, M2_SEL 세 선이 모이는 가운데에 두었다. 위치를 바꿔도 되지만 실크나 라벨로 "50W ←  → 300W"를 꼭 적는다.']
T(nx,ny,'메모',14,'ph',bold=True)
for i,n in enumerate(notes): T(nx,ny+26+i*34,'• '+n,12,'t')
open('베이스보드_부품배치도.svg','w',encoding='utf-8').write(svg(1180,1090,'측창 모터 제어기 베이스 보드 부품 배치도'))
