# 제어 기판 연결표 (회로도 schematic.py를 그대로 옮김). 기호는 회로도·배치도·BOM과 같다.
# FP: 기호 → (라이브러리, 풋프린트), NETS: 기호 → {패드 번호: 넷 이름}
# 다이오드 풋프린트는 1번 패드가 캐소드(K), 2번이 애노드(A). TO-92 2N7000은 1 S, 2 G, 3 D. TO-220 IRLZ44N은 1 G, 2 D, 3 S.

RV  = ('Resistor_THT', 'R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical')
RL  = ('Resistor_THT', 'R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal')
RH  = ('Resistor_THT', 'R_Axial_DIN0309_L9.0mm_D3.2mm_P5.08mm_Vertical')
R1W = ('Resistor_THT', 'R_Axial_DIN0414_L11.9mm_D4.5mm_P15.24mm_Horizontal')
CD  = ('Capacitor_THT', 'C_Disc_D5.0mm_W2.5mm_P2.50mm')
D35 = ('Diode_THT', 'D_DO-35_SOD27_P2.54mm_Vertical_AnodeUp')
D35L= ('Diode_THT', 'D_DO-35_SOD27_P7.62mm_Horizontal')
D41 = ('Diode_THT', 'D_DO-41_SOD81_P5.08mm_Vertical_AnodeUp')
D201= ('Diode_THT', 'D_DO-201AD_P5.08mm_Vertical_AnodeUp')
D15 = ('Diode_THT', 'D_DO-15_P5.08mm_Vertical_AnodeUp')
PTC = ('Fuse', 'Fuse_Bourns_MF-RG300')
T92 = ('Package_TO_SOT_THT', 'TO-92_Inline_Wide')
T220= ('Package_TO_SOT_THT', 'TO-220-3_Vertical')

FP, NETS = {}, {}
def part(ref, fp, **pads):
    FP[ref] = fp; NETS[ref] = {k.lstrip('p'): v for k, v in pads.items()}
def R(ref, a, b, fp=RV): part(ref, fp, p1=a, p2=b)
def C(ref, a, b, fp=CD): part(ref, fp, p1=a, p2=b)
def D(ref, anode, kathode, fp=D35): part(ref, fp, p1=kathode, p2=anode)

# ---- A. ESP32 소켓: J11 L줄, J12 R줄 (1번이 안테나 쪽)
L = ['+3V3','+3V3',None,'IO4','IO5','IO6','IO7','IO15','IO16','IO17',None,'IO8',None,None,'IO9','IO10','IO11','IO12','IO13','IO14','+5V','GND']
Rr = ['GND',None,None,'IO1','IO2','IO42','IO41',None,'IO39','IO38',None,None,None,None,None,None,'IO47','IO21',None,None,'GND','GND']
SOCK = ('Connector_PinSocket_2.54mm', 'PinSocket_1x22_P2.54mm_Vertical')
FP['J11'] = SOCK; NETS['J11'] = {str(i+1): n for i, n in enumerate(L) if n}
FP['J12'] = SOCK; NETS['J12'] = {str(i+1): n for i, n in enumerate(Rr) if n}

# ---- B. 전원
part('J1', ('TerminalBlock_Phoenix', 'TerminalBlock_Phoenix_MKDS-1,5-4-5.08_1x04_P5.08mm_Horizontal'), p1='24V_A', p2='24V_B', p3='GND', p4='GND')
D('D1', '24V_A', '24V_F', D201); D('D2', '24V_B', '24V_F', D201)
C('C1', '24V_F', 'GND', ('Capacitor_THT', 'CP_Radial_D10.0mm_P5.00mm'))
C('C2', '24V_F', 'GND', ('Capacitor_THT', 'CP_Radial_D5.0mm_P2.00mm'))
C('C3', '24V_F', 'GND')
# URB4805S-6WR3 SIP-8: 1 GND, 2 Vin, 3 CTRL(비움), 6 +Vo, 7 0V (데이터시트로 확인할 것)
part('U1', ('Connector_PinHeader_2.54mm', 'PinHeader_1x08_P2.54mm_Vertical'), p1='GND', p2='24V_F', p6='+5V', p7='GND')
C('C4', '+5V', 'GND', ('Capacitor_THT', 'CP_Radial_D5.0mm_P2.00mm'))
C('C5', '+5V', 'GND'); C('C9', '+3V3', 'GND')

# ---- C. 드라이버 J8, J9
XH8 = ('Connector_JST', 'JST_XH_B8B-XH-A_1x08_P2.50mm_Vertical')
for n, J, rp, lp, en, io, b in [(1, 'J8', 'IO11', 'IO12', 'IO13', 'IO4', 0), (2, 'J9', 'IO14', 'IO15', 'IO16', 'IO5', 6)]:
    isn, sel = f'M{n}_ISN', f'M{n}_SEL'
    part(J, XH8, p1=rp, p2=lp, p3=en, p4=en, p5=isn, p6=isn, p7='+3V3', p8='GND')
    R(f'R{1+b}', rp, 'GND', RL); R(f'R{2+b}', lp, 'GND', RL); R(f'R{3+b}', en, 'GND', RL); R(f'R{4+b}', isn, io, RL)
    R(f'R{5+b}', isn, 'GND'); R(f'R{6+b}', isn, sel)
    C(f'C{9+n}', io, 'GND'); D(f'D{3+n}', io, '+3V3')

# ---- D. 비상정지 감지, 주 차단 MC
part('J3', ('Connector_Phoenix_MSTB', 'PhoenixContact_MSTBVA_2,5_3-G-5,08_1x03_P5.08mm_Vertical'), p1='ESTOP_IN', p2='RLY_OUT', p3='GND')
R('R13', 'ESTOP_IN', 'IO1'); R('R14', 'IO1', 'GND'); C('C6', 'IO1', 'GND')
part('JP1', ('Connector_PinHeader_2.54mm', 'PinHeader_1x02_P2.54mm_Vertical'), p1='ESTOP_IN', p2='24V_F')
R('R15', 'RLY_OUT', 'IO6'); R('R16', 'IO6', 'GND'); C('C7', 'IO6', 'GND')
part('J2', ('Connector_Phoenix_MSTB', 'PhoenixContact_MSTBVA_2,5_2-G-5,08_1x02_P5.08mm_Vertical'), p1='24V_F', p2='COIL-')
D('D3', 'COIL-', '24V_F', D41)
part('Q1', T220, p1='Q1_G', p2='COIL-', p3='Q_MID'); part('Q2', T220, p1='Q2_G', p2='Q_MID', p3='GND')
R('R21', 'Q_MID', 'GND')
R('R17', 'IO17', 'Q1_G'); R('R18', 'Q1_G', 'GND'); R('R19', 'IO21', 'Q2_G'); R('R20', 'Q2_G', 'GND')

# ---- E. 원점 센서 J5, J6
MST4 = ('Connector_Phoenix_MSTB', 'PhoenixContact_MSTBVA_2,5_4-G-5,08_1x04_P5.08mm_Vertical')
for k, J, io, U in [(0, 'J5', 'IO7', 'U4'), (1, 'J6', 'IO10', 'U5')]:
    rs, rp, cf, dd, ta, tb, ff = [('R22','R23','C16','D7','D9','D10','F1'), ('R24','R25','C17','D8','D11','D12','F2')][k]
    s = f'S{k+1}'
    part(J, MST4, p1=f'{s}_P24', p2=f'{s}_SIG', p3='GND')
    part(ff, PTC, p1=f'{s}_P24', p2='24V_F')
    D(ta, 'GND', f'{s}_P24', D15); D(tb, 'GND', f'{s}_SIG', D15)
    R(rs, f'{s}_SIG', f'{s}_LED', RH); D(dd, 'GND', f'{s}_LED')
    part(U, ('Package_DIP', 'DIP-4_W7.62mm'), p1=f'{s}_LED', p2='GND', p3='GND', p4=io)
    R(rp, io, '+3V3', RL); C(cf, io, 'GND')

# ---- F. 온도
part('J7', ('Connector_Phoenix_MSTB', 'PhoenixContact_MSTBVA_2,5_2-G-5,08_1x02_P5.08mm_Vertical'), p1='T_P', p2='T_M')
part('F3', PTC, p1='T_P', p2='24V_F'); D('D13', 'GND', 'T_P', D15)
R('R27', 'T_M', 'IO2', RL); R('R26', 'T_M', 'GND', R1W); C('C8', 'IO2', 'GND'); D('D6', 'IO2', '+3V3', D35L)

# ---- H. 엔코더 풀업, 모델 점퍼, 피에조
for i, io in enumerate(['IO41', 'IO42', 'IO47']):
    R(f'R{28+i}', io, '+3V3', RL); C(f'C{12+i}', io, 'GND')
# JP2 3×3: 패드 = 3×줄 + 칸 + 1 (칸 0 왼쪽 50W, 1 가운데, 2 오른쪽 300W)
FP['JP2'] = ('Connector_PinHeader_2.54mm', 'PinHeader_3x03_P2.54mm_Vertical')
NETS['JP2'] = {'1': 'GND', '2': 'IO39', '5': 'M1_SEL', '6': 'GND', '8': 'M2_SEL', '9': 'GND'}
R('R31', 'IO39', '+3V3', RL); C('C15', 'IO39', 'GND')
part('BZ1', ('Buzzer_Beeper', 'Buzzer_12x9.5RM7.6'), p1='+5V', p2='BZ_D')
R('R32', '+5V', 'BZ_D'); part('Q3', T92, p1='GND', p2='Q3_G', p3='BZ_D')
R('R33', 'IO38', 'Q3_G'); R('R34', 'Q3_G', 'GND')

# ---- G. I2C 레벨 변환, 패널 J10
part('Q4', T92, p1='IO8', p2='+3V3', p3='SDA_5V'); part('Q5', T92, p1='IO9', p2='+3V3', p3='SCL_5V')
R('R35', 'IO8', '+3V3'); R('R36', 'IO9', '+3V3'); R('R37', 'SDA_5V', '+5V'); R('R38', 'SCL_5V', '+5V')
part('J10', ('Connector_JST', 'JST_XH_B7B-XH-A_1x07_P2.50mm_Vertical'), p1='+5V', p2='GND', p3='SDA_5V', p4='SCL_5V', p5='IO41', p6='IO42', p7='IO47')

# ---- 고정 구멍
for i in range(1, 5): FP[f'H{i}'] = ('MountingHole', 'MountingHole_3.2mm_M3'); NETS[f'H{i}'] = {}

POWER = {'24V_A', '24V_B', '24V_F', 'GND', 'COIL-', 'Q_MID'}   # 굵게 (1.5 mm)
MID = {'+5V'}                                                   # 0.8 mm
