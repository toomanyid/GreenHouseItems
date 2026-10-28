import sys
from lib import *
LDO='--ldo' in sys.argv  # 3.3V: False = 개발 보드 레귤레이터(빵판과 같음), True = 보드 LDO + JP3
WD,HT=2400,2080
T(24,40,'측창 모터 제어기 · 제어 기판 회로도 (통합본: 빵판 시험 회로 기준, 수작업 납땜 PCB)',24,'ph',bold=True)
T(24,64,'기준: 빵판 시험 회로도(10-18판), 설계 정리, 핀 배정 ESP32-S3 · 2026-10-24 (주 차단 MC 복원, 빵판·제어 기판 회로도 통합) · 부품은 모두 스루홀 · 모터 전류 경로는 보드 밖(단자대·AWG14~16)이며 점선으로만 표시',13,'muted')
NET(1640,38,'IO11 M1_RPWM','sig','r'); T(1772,43,'같은 이름 = 같은 선',12,'muted')
NET(1910,38,'+3V3','pwr','r'); T(1970,43,'전원',12,'muted')
OFF((2020,38),(2060,38)); T(2068,43,'보드 밖 배선',12,'muted'); NC(2170,38); T(2182,43,'연결 안 함',12,'muted')
T(1640,66,'GND 기호는 모두 같은 GND (24V 전력 GND와는 SMPS − 단자 한 곳에서만 만남)',12,'muted')

# ---------- A. ESP32 socket ----------
PANEL(20,84,760,900,'A. U2 ESP32-S3-DevKitC-1 (N16R8) · 소켓 장착','J11(L줄)·J12(R줄) 2.54mm 암 핀헤더 22핀에 꽂음. USB 단자를 아래로, 위에서 본 순서 (핀 배정 4절). '+('3V3 핀은 입력, 5V 핀은 비움' if LDO else '5V 핀으로 전원을 넣고, 3V3 핀의 3.3V를 보드에 씀'))
O.append('<rect class="mod" x="300" y="150" width="200" height="764" rx="6"/>')

L=[('3V3',('p','+3V3_ESP' if LDO else '+3V3')),('3V3',('p','+3V3_ESP' if LDO else '+3V3')),('RST',('n','')),('IO4',('s','IO4 M1_IS')),('IO5',('s','IO5 M2_IS')),('IO6',('s','IO6 MC_SNS')),('IO7',('s','IO7 S1_IN')),('IO15',('s','IO15 M2_LPWM')),('IO16',('s','IO16 M2_EN')),('IO17',('s','IO17 MC_A')),('IO18',('n','여유')),('IO8',('s','IO8 SDA_3V3')),('IO3',('n','부팅')),('IO46',('n','부팅')),('IO9',('s','IO9 SCL_3V3')),('IO10',('s','IO10 S2_IN')),('IO11',('s','IO11 M1_RPWM')),('IO12',('s','IO12 M1_LPWM')),('IO13',('s','IO13 M1_EN')),('IO14',('s','IO14 M2_RPWM')),('5V',('n','비움 (연결 안 함)') if LDO else ('p','+5V')),('GND',('g',''))]
Rr=[('GND',('g','')),('IO43 TX',('n','USB 시리얼')),('IO44 RX',('n','USB 시리얼')),('IO1',('s','IO1 ESTOP_SNS')),('IO2',('s','IO2 TEMP_IN')),('IO42',('s','IO42 ENC_B')),('IO41',('s','IO41 ENC_A')),('IO40',('n','여유')),('IO39',('s','IO39 MODEL')),('IO38',('s','IO38 BUZZ')),('IO37',('n','PSRAM')),('IO36',('n','PSRAM')),('IO35',('n','PSRAM')),('IO0',('n','BOOT')),('IO45',('n','부팅')),('IO48',('n','내장 LED')),('IO47',('s','IO47 ENC_SW')),('IO21',('s','IO21 MC_B')),('IO20',('n','여유 (USB D+)')),('IO19',('n','여유 (USB D−)')),('GND',('g','')),('GND',('g',''))]
for i,(nm,(k,lab)) in enumerate(L):
    y=174+i*34; T(306,y+4,f'L{i+1}',10,'muted'); T(330,y+4,nm,13,'t',bold=True,mono=True)
    if k=='n':
        W((262,y),(300,y)); NC(256,y); T(244,y+4,lab,11,'muted','end')
    elif k=='g':
        W((250,y),(300,y)); GND(250,y,10)
    else:
        W((250,y),(300,y)); NET(250,y,lab,'sig' if k=='s' else 'pwr','l')
for i,(nm,(k,lab)) in enumerate(Rr):
    y=174+i*34; T(494,y+4,f'R{i+1}',10,'muted','end'); T(470,y+4,nm,13,'t','end',True,True)
    if k=='n':
        W((500,y),(538,y)); NC(544,y); T(556,y+4,lab,11,'muted')
    elif k=='g':
        W((500,y),(550,y)); GND(550,y,10)
    else:
        W((500,y),(550,y)); NET(550,y,lab,'sig','r')
T(34,944,'IO38 피에조 / IO48 내장 LED는 정품 기준. 받은 보드의 LED가 38번이면 피에조를 48번으로 옮긴다.',12,'muted')
T(34,962,'J11 L1(3V3) 옆에 100nF (B의 C9). 보드 모델은 8대분과 예비품을 하나로 통일하고, 소켓 간격은 받은 보드로 잰다.',12,'muted')

# ---------- B. Power ----------
PANEL(800,84,760,430,'B. 전원 (24V → 5V → 3.3V)','24V_A·24V_B = 주 퓨즈 뒤 24V (채널마다). 다이오드로 묶어 24V_F를 만든다')
p=TB(830,183,4,34,['24V_A','24V_B','GND','GND'],None)
T(830,175,'J1 나사 단자대 4P',12,'t',bold=True)
(a,ya),(b,yb),(g1,yg1),(g2,yg2)=p
W((a,ya),(905,ya)); D(905,ya,955,ya,'s'); W((955,ya),(1000,ya)); T(930,ya+22,'D1 1N5822',11,'muted','middle')
W((b,yb),(905,yb)); D(905,yb,955,yb,'s'); W((955,yb),(1000,yb),(1000,ya)); T(930,yb+24,'D2 1N5822',11,'muted','middle'); DOT(1000,ya)
W((g1,yg1),(880,yg1),(880,yg2)); W((g2,yg2),(880,yg2)); DOT(880,yg2); GND(880,yg2,10)
NET(1000,ya-30,'24V_F','pwr','u'); W((1000,ya-30),(1000,ya))
W((1000,ya),(1170,ya))
for x,lab,pol in [(1030,'C1|470µF|50V',True),(1075,'C2|22µF|50V',True),(1120,'C3|100nF|',False)]:
    DOT(x,ya); C(x,ya,x,ya+60,pol); GND(x,ya+60); l0,l1,l2=lab.split('|'); T(x+14,ya+30,l0,10,'muted'); T(x+14,ya+42,l1,10,'muted'); T(x+14,ya+54,l2,10,'muted')
BOX(1170,183,120,100,'U1 URB4805S-6WR3','절연형 DC-DC 24V→5V 1.2A')
PIN(1170,ya,'Vin','l',0); g=PIN(1170,263,'GND','l',20); GND(g[0],g[1],8)
PIN(1290,ya,'+Vo','r',0); g=PIN(1290,263,'0V','r',14); GND(g[0],g[1],8)
T(1230,300,'CTRL·CS 비움',10,'muted','middle')
W((1290,ya),(1460,ya))
for x,lab,pol in [(1340,'C4|22µF|25V',True),(1390,'C5|100nF|',False)]:
    DOT(x,ya); C(x,ya,x,ya+60,pol); GND(x,ya+60); l0,l1,l2=lab.split('|'); T(x+14,ya+30,l0,10,'muted'); T(x+14,ya+42,l1,10,'muted'); T(x+14,ya+54,l2,10,'muted')
NET(1460,ya,'+5V','pwr','r')
if LDO:
    # LDO
    yl=372
    NET(1000,yl,'+5V','pwr','l'); W((1000,yl),(1170,yl))
    DOT(1060,yl); C(1060,yl,1060,yl+55,True); GND(1060,yl+55); T(1076,yl+50,'C21 10µF',10,'muted')
    BOX(1170,352,120,70,'U8 LD1117V33 (3.3V LDO)','TO-220 · 1A급')
    PIN(1170,yl,'IN','l',0); g=PIN(1230,422,'GND','b',10); GND(g[0],g[1],6); PIN(1290,yl,'OUT','r',0)
    W((1290,yl),(1395,yl))
    DOT(1320,yl); C(1320,yl,1320,yl+55,True); GND(1320,yl+55); T(1334,yl+50,'C22 22µF',10,'muted')
    DOT(1360,yl); NET(1360,yl-6,'+3V3','pwr','u'); W((1360,yl-6),(1360,yl))
    JMP(1395,yl,1435,yl); T(1415,yl+22,'JP3',11,'t','middle',True)
    W((1435,yl),(1460,yl)); DOT(1450,yl); C(1450,yl,1450,yl+55); GND(1450,yl+55); T(1462,yl+50,'C9 100nF',10,'muted')
    NET(1460,yl,'+3V3_ESP','pwr','r')

else:
    yl=372
    NET(1000,yl,'+3V3','pwr','l'); W((1000,yl),(1460,yl)); T(1010,yl-14,'ESP32 3V3 핀(L1·L2)에서 나옴',11,'muted')
    DOT(1450,yl); C(1450,yl,1450,yl+55); GND(1450,yl+55); T(1462,yl+50,'C9 100nF',10,'muted')
    T(1010,yl+80,'개발 보드의 레귤레이터가 만든 3.3V (빵판과 같음). 보드 쪽 3.3V 부하는 수십 mA다.',11,'muted')
T(814,470,'• 50W는 J1의 24V_A만 잇는다. 300W는 SMPS 두 대를 24V_A, 24V_B에 하나씩 (D1, D2로 묶음). 24V_F는 센서 전원과 비상정지 감지(JP1)에도 쓴다.',11.5,'muted')
T(814,486,'• JP3(2핀 점퍼): 평소 닫음. 개발 보드에 USB를 꽂을 때만 연다. +3V3은 보드 쪽 회로(풀업, 드라이버 VCC, 레벨 변환, 클램프)에 늘 들어간다.' if LDO else '• +5V는 ESP32 5V 핀(L21)으로. 5V 쪽에 다이오드가 없으니 USB를 꽂을 때는 24V를 끈다 (빵판과 같음).',11.5,'muted')
T(814,502,'• D1·D2 1N5822: MC 코일 전류가 더해져 3A급. C9는 소켓 3V3 핀 바로 옆. U1 입출력 콘덴서 값은 데이터시트 권장값 확인.',11.5,'muted')

# ---------- C. Drivers ----------
PANEL(800,520,760,820,'C. 모터 드라이버 연결 (J8, J9 · JST XH 8P)','케이블은 BTS7960 모듈 헤더와 1:1. EN과 IS는 보드 안에서 묶고, 전류 감지 저항은 보드에 둔다')
def chan(y0,n,nets,sel,J):
    rp,lp,en,isn=nets
    b=0 if n==1 else 6
    px=1360
    pts=HDR(px,y0-20,8,40,['RPWM','LPWM','R_EN','L_EN','R_IS','L_IS','VCC','GND'],f'{J} XH 8P → BTS7960 #{n}',w=110)
    P=[pt for pt in pts]
    cx=px-14
    # RPWM
    NET(1000,y0,rp,'sig','l'); W((1000,y0),(cx,y0)); DOT(1040,y0); R(1040,y0,1040,y0+36); GND(1040,y0+36); T(1050,y0+28,f'R{1+b} 10k',10,'muted')
    # LPWM row y0+70 -> pin2 y0+40
    yL=y0+70; NET(1000,yL,lp,'sig','l'); W((1000,yL),(1280,yL),(1280,y0+40),(cx,y0+40)); DOT(1040,yL); R(1040,yL,1040,yL+36); GND(1040,yL+36); T(1050,yL+28,f'R{2+b} 10k',10,'muted')
    # EN row y0+140 -> pins 3,4 (y0+80,y0+120)
    yE=y0+140; NET(1000,yE,en,'sig','l'); W((1000,yE),(1300,yE),(1300,y0+80),(cx,y0+80)); W((1300,y0+120),(cx,y0+120)); DOT(1300,y0+120); DOT(1040,yE); R(1040,yE,1040,yE+36); GND(1040,yE+36); T(1050,yE+28,f'R{3+b} 10k',10,'muted')
    # IS pins 5,6 (y0+160, y0+200) tied, row y0+200
    yI=y0+200; W((cx,y0+160),(1330,y0+160),(1330,yI)); W((cx,yI),(1330,yI)); DOT(1330,yI)
    W((1330,yI),(1160,yI)); R(1160,yI,1100,yI); W((1100,yI),(1000,yI)); NET(1000,yI,isn,'sig','l')
    T(1130,yI-12,f'R{4+b} 10k',10,'muted','middle')
    DOT(1270,yI); R(1270,yI,1270,yI+45); GND(1270,yI+45); T(1260,yI+30,f'R{5+b} 2.2k',10,'t','end',True); T(1260,yI+43,'R_IS',10,'muted','end')
    DOT(1190,yI); R(1190,yI,1190,yI+45); NET(1190,yI+45,sel,'sig','d'); T(1180,yI+30,f'R{6+b} 1.3k 1%',10,'t','end',True)
    DOT(1070,yI); C(1070,yI,1070,yI+45); GND(1070,yI+45); T(1080,yI+36,f'C{10+n-1} 1µF',10,'muted')
    DOT(1030,yI); D(1030,yI,1030,yI+45,'s'); NET(1030,yI+45,'+3V3','pwr','d'); T(1010,yI+30,f'D{4+n-1} BAT85',10,'muted','end')
    # VCC, GND
    W((cx,y0+240),(1345,y0+240)); NET(1345,y0+240,'+3V3','pwr','l')
    W((cx,y0+280),(1340,y0+280)); GND(1340,y0+280,6)
    OFF((px+110,y0+60),(px+150,y0+60)); T(px+114,y0+80,'모듈 쪽',10,'muted'); T(px+114,y0+94,'듀폰 8P',10,'muted')
chan(600,1,('IO11 M1_RPWM','IO12 M1_LPWM','IO13 M1_EN','IO4 M1_IS'),'M1_SEL','J8')
chan(950,2,('IO14 M2_RPWM','IO15 M2_LPWM','IO16 M2_EN','IO5 M2_IS'),'M2_SEL','J9')
T(814,1290,'풀다운 10k: 부팅·리셋 중 모터가 돌지 않게. M1_SEL·M2_SEL은 H의 JP2로: 비우면 2.2k(50W), GND면 2.2k ∥ 1.3k ≈ 817Ω(300W).',11.5,'muted')
T(814,1308,'모듈 위의 원래 IS 저항은 뗀다. BTS7960 모듈은 로직 GND와 전력 B−가 모듈 안에서 이어진 제품이 많으니 테스터로 확인한다.',11.5,'muted')

# ---------- D. E-stop sense + main MC ----------
PANEL(1580,84,800,780,'D. 비상정지 감지 · 주 차단 MC','MC는 보드 밖 (주 퓨즈·큰 입력 콘덴서 뒤, BTS7960 앞). 코일은 FET 2개 직렬로 켜고, MC 뒤 전압은 J3 RLY_OUT으로 읽는다')
p=TB(1610,230,3,40,['ESTOP','RLY_OUT','GND'],sub='J3 나사 단자대 3P · 비상정지 감지, MC 뒤 전압')
(e1,ye),(ro,yr),(gg,ygg)=p
W((e1,ye),(1700,ye)); R(1700,ye,1760,ye); W((1760,ye),(1900,ye)); NET(1900,ye,'IO1 ESTOP_SNS','sig','r'); T(1730,ye-12,'R13 100k',10,'muted','middle')
DOT(1800,ye); R(1800,ye,1800,ye+30); GND(1800,ye+30,4); T(1793,ye+24,'R14 10k',10,'muted','end')
DOT(1850,ye); C(1850,ye,1850,ye+30); GND(1850,ye+30,4); T(1858,ye+24,'C6 100nF',10,'muted')
T(2040,ye+4,'24V → 2.18V',11,'muted')
DOT(1680,ye); W((1680,ye),(1680,ye-50)); JMP(1680,ye-50,1740,ye-50); NET(1740,ye-50,'24V_F','pwr','r'); T(1710,ye-66,'JP1',11,'t','middle',True)
yR=ye+160
W((ro,yr),(1700,yr),(1700,yR),(1720,yR)); R(1720,yR,1780,yR); W((1780,yR),(1900,yR)); NET(1900,yR,'IO6 MC_SNS','sig','r'); T(1750,yR-12,'R15 100k',10,'muted','middle')
DOT(1820,yR); R(1820,yR,1820,yR+36); GND(1820,yR+36); T(1813,yR+28,'R16 10k',10,'muted','end')
DOT(1860,yR); C(1860,yR,1860,yR+36); GND(1860,yR+36); T(1868,yR+28,'C7 100nF',10,'muted')
T(2040,yR+4,'24V → 2.18V',11,'muted')
W((gg,ygg),(1660,ygg)); GND(1660,ygg,6)
T(1594,490,'J3 ESTOP: 300W는 비상 버튼 뒤(코일 쪽) 선. 전압이 없으면 비상정지.',11.5,'muted')
T(1594,508,'JP1: 50W는 닫음 (버튼이 없으니 ESTOP_SNS를 24V_F에 바로, 항상 정상). 300W는 연다.',11.5,'muted')
T(1594,526,'J3 RLY_OUT: MC 주접점 뒤(드라이버 쪽) 24V. 300W는 두 극 뒤를 1N4148 2개로 묶어 이 핀에 (보드 밖, MC 옆).',11.5,'muted')
# MC coil drive
pc=TB(1610,560,2,40,['24V_F','COIL−'],sub='J2 나사 단자대 2P · MC 코일 (A1+, A2−)')
(c1,yc1),(c2,yc2)=pc
W((c1,yc1),(1760,yc1)); NET(1760,yc1,'24V_F','pwr','r')
DOT(1720,yc1); DOT(1720,yc2); D(1720,yc2,1720,yc1,'d'); T(1710,yc1+26,'D3 1N4007',10,'muted','end')
d1,s1=NMOS(1820,680,'Q1 IRLZ44N','FET A (코일 쪽)')
W((c2,yc2),(d1[0],yc2),d1)
d2,s2=NMOS(1820,780,'Q2 IRLZ44N','FET B (GND 쪽)')
W(s1,d2); DOT(d1[0],730); W((d1[0],730),(1990,730)); R(1990,730,1990,766); GND(1990,766,4); T(2000,756,'R21 100k',10,'muted')
GND(s2[0],s2[1],4)
for gy,lab,ra,rb in [(680,'IO17 MC_A','R17','R18'),(780,'IO21 MC_B','R19','R20')]:
    W((1820,gy),(1790,gy)); R(1790,gy,1740,gy); W((1740,gy),(1700,gy)); NET(1700,gy,lab,'sig','l'); T(1765,gy-12,f'{ra} 100Ω',10,'muted','middle')
    DOT(1805,gy); R(1805,gy,1805,gy+30); GND(1805,gy+30,4); T(1797,gy+26,f'{rb} 10k',10,'muted','end')
T(2080,640,'두 FET가 모두 켜져야 MC가 붙는다.',11.5,'muted')
T(2080,658,'FET 하나가 합선돼도 저절로 붙지 않음.',11.5,'muted')
T(2080,690,'구동: MC 붙임 → 0.1초 → EN.',11.5,'muted')
T(2080,708,'정지: EN 끔 → 0.1초 → MC 뗌',11.5,'muted')
T(2080,726,'(두 창 정지 뒤 10초 유지).',11.5,'muted')
T(1594,852,'MC: DC 24V 코일, 50W 1극 / 300W 3극(채널마다 1극). MC 뒤 방전 4.7kΩ 1W와 100µF 이하 콘덴서는 보드 밖 드라이버 쪽.',11.5,'muted')

# ---------- E. Sensors ----------
PANEL(1580,880,800,560,'E. 원점 센서 입력 2개 (24V on/off → 포토커플러 → GPIO)','센서 ON(접점 닫힘 또는 24V 출력) → 약 10mA → GPIO LOW. 센서 전원 +24V는 PTC 0.1A를 거쳐 나간다')
def sens(y0,J,lab,U,k):
    rs,rp,cf,dd,ta,tb,ff=[('R22','R23','C16','D7','D9','D10','F1'),('R24','R25','C17','D8','D11','D12','F2')][k]
    p=TB(1610,y0,4,50,['+24V','SIG','GND','예비']); T(1610,y0-10,f'{J} 나사 단자대 4P · {lab}',12,'t',bold=True)
    (a,ya),(s,ys),(g,yg),(sp,ysp)=p
    W((a,ya),(1720,ya)); R(1720,ya,1780,ya,True); W((1780,ya),(1800,ya)); NET(1800,ya,'24V_F','pwr','r'); T(1750,ya-14,f'{ff} PTC 0.1A',10,'muted','middle')
    DOT(1665,ya); D(1665,ya+26,1665,ya,'z'); GND(1665,ya+26,4); T(1675,ya+22,f'{ta} P6KE30A',10,'muted')
    W((s,ys),(1740,ys)); R(1740,ys,1800,ys); W((1800,ys),(1900,ys)); T(1770,ys-12,f'{rs} 2.2k 1/2W',10,'muted','middle')
    DOT(1700,ys); D(1700,ys+26,1700,ys,'z'); GND(1700,ys+26,4); T(1710,ys+22,f'{tb} P6KE30A',10,'muted')
    DOT(1840,ys); D(1840,ys+36,1840,ys,'d'); GND(1840,ys+36,4); T(1830,ys+26,f'{dd} 1N4148',10,'muted','end')
    O.append(f'<rect class="mod" x="1900" y="{ys-20}" width="100" height="80" rx="4"/>')
    T(1950,ys-28,f'{U} PC817',12,'t','middle',True)
    O.append(f'<polygon class="symf" points="1922,{ys+12} 1938,{ys+12} 1930,{ys+26}"/><path class="w" d="M1920,{ys+27} H1940 M1930,{ys} V{ys+12} M1930,{ys+27} V{ys+40} H1900 M1946,{ys+16} l8,0 M1950,{ys+12} l4,4 l-4,4"/>')
    O.append(f'<path class="w" d="M1966,{ys+6} V{ys+34} M1966,{ys+14} L1984,{ys} H2000 M1966,{ys+26} L1984,{ys+40} H2000"/>')
    W((1900,ys),(1930,ys)); T(1906,ys-6,'1',9,'muted'); T(1906,ys+54,'2',9,'muted'); T(1994,ys-6,'4',9,'muted','end'); T(1994,ys+54,'3',9,'muted','end')
    W((1900,ys+40),(1885,ys+40)); GND(1885,ys+40,6)
    W((2000,ys+40),(2015,ys+40)); GND(2015,ys+40,6)
    W((2000,ys),(2170,ys)); NET(2170,ys,f'{lab}','sig','r')
    DOT(2080,ys); R(2080,ys,2080,ys-40); NET(2080,ys-40,'+3V3','pwr','u'); T(2090,ys-14,f'{rp} 10k',10,'muted')
    DOT(2130,ys); C(2130,ys,2130,ys+36); GND(2130,ys+36); T(2140,ys+28,f'{cf} 10nF',10,'muted')
    W((g,yg),(1650,yg)); GND(1650,yg,6)
    W((sp,ysp),(1650,ysp)); NC(1656,ysp)
sens(960,'J5','IO7 S1_IN','U4',0)
sens(1200,'J6','IO10 S2_IN','U5',1)
T(1594,1426,'단자의 +24V, SIG에 P6KE30A. 1N4148은 포토커플러 LED 역전압 보호.',11.5,'muted')

# ---------- F. Temp ----------
PANEL(20,1000,760,300,'F. 온도 센서 입력 (선택, PT100 + 4~20mA 변환기)','변환기는 2선식. +에 24V(PTC 0.05A), −에서 온 전류를 120Ω으로 읽는다')
p=TB(50,1080,2,70,['T+','T−']); T(50,1070,'J7 나사 단자대 2P',12,'t',bold=True)
(tp,ytp),(tm,ytm)=p
W((tp,ytp),(160,ytp)); R(160,ytp,220,ytp,True); W((220,ytp),(300,ytp)); NET(300,ytp,'24V_F','pwr','r'); T(190,ytp-14,'F3 PTC 0.05A',10,'muted','middle')
DOT(110,ytp); D(110,ytp+30,110,ytp,'z'); GND(110,ytp+30,4); T(120,ytp+26,'D13 P6KE30A',10,'muted')
W((tm,ytm),(240,ytm)); R(240,ytm,300,ytm); W((300,ytm),(470,ytm)); NET(470,ytm,'IO2 TEMP_IN','sig','r'); T(270,ytm-12,'R27 10k',10,'muted','middle')
DOT(170,ytm); R(170,ytm,170,ytm+45); GND(170,ytm+45); T(180,ytm+30,'R26 120Ω 1% 1W',10,'muted')
DOT(350,ytm); C(350,ytm,350,ytm+40); GND(350,ytm+40); T(340,ytm+30,'C8 100nF',10,'muted','end')
DOT(420,ytm); D(420,ytm,420,ytm+40,'s'); NET(420,ytm+40,'+3V3','pwr','d'); T(410,ytm+28,'D6 BAT85',10,'muted','end')
T(560,1210,'4mA → 0.48V',11,'muted'); T(560,1226,'20mA → 2.4V (21mA 2.52V)',11,'muted'); T(560,1242,'센서 없으면 0V → "없음"',11,'muted')

# ---------- H. encoder pullups, model jumper, piezo ----------
PANEL(20,1320,760,520,'H. 엔코더 풀업 · 모델 점퍼(JP2) · 피에조','엔코더 본체는 패널 보드(I)에. 풀업과 10nF은 제어 기판에 둔다')
for i,lab in enumerate(['IO41 ENC_A','IO42 ENC_B','IO47 ENC_SW']):
    y=1450+i*80
    W((50,y),(200,y)); NET(200,y,lab,'sig','r'); T(46,y+4,'J10 쪽',10,'muted','end') if False else None
    DOT(90,y); R(90,y,90,y-36); NET(90,y-36,'+3V3','pwr','u'); T(100,y-14,f'R{28+i} 10k',10,'muted')
    DOT(150,y); C(150,y,150,y+34); GND(150,y+34); T(160,y+26,f'C{12+i} 10nF',10,'muted')
    W((50,y),(50,y)); 
# JP2
gx=[620,650,680]; gy=[1430,1500,1570]
T(635,1385,'50W',11,'t','middle',True); T(665,1385,'300W',11,'t','middle',True)
O.append('<path class="nc" d="M650,1392 V1595" stroke-dasharray="3 3"/>')
for r in range(3):
    O.append(f'<rect class="shunt" x="{gx[0]-8}" y="{gy[r]-8}" width="46" height="16" rx="5"/>')
    for c in range(3):
        O.append(f'<rect class="pin" x="{gx[c]-5}" y="{gy[r]-5}" width="10" height="10"/>')
T(650,1612,'JP2 3×3 핀헤더 (그림은 50W 위치)',11,'t','middle',True)
# row1: C=IO39, L=GND, R=NC
y=gy[0]; W((650,y-5),(650,y-22),(470,y-22)); NET(470,y-22,'IO39 MODEL','sig','l')
DOT(520,y-22); R(520,y-22,520,y-58); NET(520,y-58,'+3V3','pwr','u'); T(530,y-36,'R31 10k',10,'muted')
DOT(555,y-22); C(555,y-22,555,y+14); GND(555,y+14); T(563,y+10,'C15 10nF',10,'muted')
W((615,y),(595,y)); GND(595,y,10); NC(696,y); W((685,y),(691,y))
# row2/3: C=SEL, L=NC, R=GND
for r,sel in [(1,'M1_SEL'),(2,'M2_SEL')]:
    y=gy[r]; W((650,y-5),(650,y-20),(560,y-20)); NET(560,y-20,sel,'sig','l')
    NC(604,y); W((609,y),(615,y)); W((685,y),(710,y)); GND(710,y,8)
T(722,1434,'1줄',10,'muted'); T(722,1504,'2줄',10,'muted'); T(722,1574,'3줄',10,'muted')
T(470,1640,'세 줄 모두 왼쪽(L-C) = 50W, 모두 오른쪽(C-R) = 300W.',11,'muted')
T(470,1656,'1줄: L=GND, R=비움 → 50W에서 IO39 LOW.',11,'muted')
T(470,1672,'2·3줄: L=비움, R=GND → 300W에서 1.3k가 붙음.',11,'muted')
T(470,1688,'빠지면 IO39 HIGH(300W)에 2.2k만 남아',11,'muted')
T(470,1704,'전류를 크게 읽는다 (안전 쪽).',11,'muted')
T(470,1720,'점퍼 캡은 빠지지 않게 실리콘으로 고정.',11,'muted')
# piezo
NET(290,1690,'+5V','pwr','u'); W((290,1690),(290,1710)); DOT(290,1710)
O.append('<rect class="sym" x="274" y="1724" width="32" height="30" rx="3"/>'); T(290,1744,'BZ1',11,'t','middle',True)
W((290,1710),(290,1724)); W((290,1754),(290,1772)); W((290,1710),(350,1710)); R(350,1710,350,1772); W((350,1772),(290,1772)); DOT(290,1772); T(360,1745,'R32 1k',10,'muted')
T(265,1734,'수동형',10,'muted','end'); T(265,1748,'피에조',10,'muted','end')
d,s=NMOS(260,1802,'Q3 2N7000','TO-92: S G D')
W((290,1772),d)
NET(140,1802,'IO38 BUZZ','sig','l'); W((140,1802),(170,1802)); R(170,1802,220,1802); W((220,1802),(260,1802)); T(195,1790,'R33 100Ω',10,'muted','middle')
DOT(235,1802); R(235,1802,235,1832); GND(235,1832,4); T(220,1826,'R34 10k',10,'muted','end')
GND(s[0],s[1],2)

# ---------- G. Level shift + panel connector ----------
PANEL(800,1360,760,480,'G. I2C 레벨 변환 · 패널 연결 (J10 · JST XH 7P)','LCD는 5V, ESP32 쪽은 3.3V. 패널 보드까지 케이블 하나 (약 30cm)')
# Q4·Q5: I2C 레벨 변환 (게이트 +3V3, 소스 ESP32 쪽, 드레인 LCD 쪽)
pts=HDR(1340,1440,7,40,['+5V','GND','SDA_5V','SCL_5V','ENC_A','ENC_B','ENC_SW'],'J10 XH 7P → 패널 보드',w=120)
(p1,y1),(p2,y2),(p3,y3),(p4,y4),(p5,y5),(p6,y6),(p7,y7)=pts
W((1250,y1),(p1,y1)); NET(1250,y1,'+5V','pwr','l')
W((p2,y2),(1300,y2)); GND(1300,y2,8)
for gy,q,io,r3,r5,xv,yp,rd in [(1530,'Q4','IO8 SDA_3V3','R35','R37',1200,y3,1105),(1650,'Q5','IO9 SCL_3V3','R36','R38',1180,y4,1160)]:
    d,s_=NMOS(1025,gy,q+' 2N7000','TO-92: S G D')
    W((1025,gy),(1000,gy)); NET(1000,gy,'+3V3','pwr','u')
    W(s_,(s_[0],gy+45),(905,gy+45)); NET(905,gy+45,io,'sig','l')
    DOT(945,gy+45); R(945,gy+45,945,gy-15); NET(945,gy-15,'+3V3','pwr','u'); T(935,gy+18,r3+' 4.7k',10,'muted','end')
    W(d,(d[0],gy-45),(xv,gy-45),(xv,yp),(p3 if q=='Q4' else p4,yp)); DOT(rd,gy-45)
    if q=='Q4': R(rd,gy-45,rd,gy-85); NET(rd,gy-85,'+5V','pwr','u'); T(rd+10,gy-62,r5+' 4.7k',10,'muted')
    else: R(rd,gy-45,rd,gy+15); NET(rd,gy+15,'+5V','pwr','d'); T(rd+10,gy-12,r5+' 4.7k',10,'muted')
for (pp,yv),lab in [((p5,y5),'IO41 ENC_A'),((p6,y6),'IO42 ENC_B'),((p7,y7),'IO47 ENC_SW')]:
    W((1290,yv),(pp,yv)); NET(1290,yv,lab,'sig','l')
T(814,1760,'Q4·Q5: I2C 표준 레벨 변환. 어느 쪽이 LOW로 당겨도 반대쪽이 따라 내려간다. 풀업 4.7k를 양쪽에 둔다. LCD 주소 0x27 또는 0x3F.',11.5,'muted')
T(814,1778,'J10은 드라이버 8P와 핀 수가 달라 잘못 꽂히지 않는다. 엔코더 선은 풀업이 제어 기판에 있으니 3.3V가 필요 없다.',11.5,'muted')

# ---------- I. Panel board ----------
PANEL(1580,1460,800,380,'I. 패널 보드 (뚜껑 쪽)','LCD와 엔코더를 한 기판에. 능동 부품 없음')
pts=HDR(1620,1530,7,40,['+5V','GND','SDA_5V','SCL_5V','ENC_A','ENC_B','ENC_SW'],'P1 XH 7P ← J10',side='r',w=120)
(q1,z1),(q2,z2),(q3,z3),(q4,z4),(q5,z5),(q6,z6),(q7,z7)=pts
lp=HDR(1960,1530,4,40,['VCC','GND','SDA','SCL'],'LCD I2C 변환 기판 (4P)',w=110)
for (a,b),(c,d) in zip([(q1,z1),(q2,z2),(q3,z3),(q4,z4)],lp): W((a,b),(c,d))
DOT(1830,z1); DOT(1830,z2); C(1830,z1,1830,z2); T(1845,z1+26,'100nF',10,'muted')
T(2090,1556,'1602 LCD',12,'t',bold=True); T(2090,1572,'(5V, PCF8574)',10,'muted')
T(2090,1600,'변환 기판 핀 순서는',10,'muted'); T(2090,1614,'제품마다 다르니 보고 맞춘다',10,'muted')
O.append('<rect class="mod" x="1960" y="1700" width="110" height="120" rx="4"/>'); T(2120,1712,'U9 PEC16 엔코더',12,'t',bold=True)
for (a,b),(nm,yy) in zip([(q5,z5),(q6,z6),(q7,z7)],[('A',1710),('B',1750),('SW',1790)]):
    W((a,b),(1900,b),(1900,yy),(1960,yy)) if b!=yy else W((a,b),(1960,yy)); T(1966,yy+4,nm,11,'t',mono=True)
for nm,yy in [('C',1730),('SW',1808)]:
    W((2070,yy),(2100,yy)); T(2064,yy+4,nm,11,'t','end',mono=True); GND(2100,yy,6)
T(2120,1752,'C(공통)와 SW 반대쪽은',10,'muted'); T(2120,1766,'GND (P1 2번)',10,'muted')

# ---------- Notes ----------
O.append('<rect class="panel" x="20" y="1856" width="2360" height="210" rx="8"/>')
T(34,1880,'J. 보드 밖 배선 · 모델 설정 · 부품 메모',16,'ph',bold=True)
notes=['나사 단자대(5.08mm): J1 24V 입력 4P, J2 MC 코일 2P, J3 비상정지·MC 뒤 전압 3P, J5·J6 원점 센서 4P, J7 온도 2P. 잠금 커넥터(JST XH 2.5mm): J8·J9 드라이버 8P, J10 패널 7P. 플러거블 단자대면 J1만 고정형이나 색 표기로 구분한다.',
'모터 전류 경로(보드 밖, AWG14~16, 30A 단자대): 24V → 주 퓨즈 → 입력 콘덴서 + 1.5KE30A → MC 주접점(뒤 24V를 J3 RLY_OUT으로) → BTS7960 B+ / M+·M− → 자동 소켓 → 모터 플러그 → 모터 퓨즈 → (300W: 비상정지 접점) → 모터 단자(1.5KE33CA).',
'50W 설정: JP2 세 줄 모두 왼쪽, JP1 닫음, J1은 24V_A만. 300W 설정: JP2 세 줄 모두 오른쪽, JP1 열고 J3 ESTOP에 비상 버튼 뒤 선, J1에 24V_A·24V_B. 모델 구분과 IS 저항이 같은 점퍼 블록에서 함께 바뀐다.',
'접지: 보드 GND(ESP32·변환 모듈·센서)는 한 줄로 모으고, SMPS − 단자 한 곳에서만 24V 전력 GND와 잇는다. U1의 0V와 24V GND도 이 점에서 묶는다.',
('빵판 시험 회로와 다른 점: 3V3 핀으로 전원 입력(U8 + JP3), 5V 핀 비움, ' if LDO else '빵판 시험 회로와 다른 점: ')+'24V 입력 2개 다이오드 OR, 드라이버를 XH 8P로 받고 IS 저항을 보드에(2.2k + 1.3k), 모델 점퍼 3×3, LCD·엔코더를 J10 하나로 패널 보드에, 주 차단 MC(J2, J3 RLY_OUT). 부품은 모두 스루홀(손납땜).']
for i,n in enumerate(notes): T(34,1906+i*30,'• '+n,12.5,'t')
open(sys.argv[1] if len(sys.argv)>1 and not sys.argv[1].startswith('--') else '제어기_회로도.svg','w',encoding='utf-8').write(svg(WD,HT,'측창 모터 제어기 제어 기판 회로도'))
print(len(O))
