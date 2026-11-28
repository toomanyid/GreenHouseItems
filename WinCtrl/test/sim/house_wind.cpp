// 개발용 하우스 모형 (가중치, 판정 값을 맞출 때만 쓴다. 펌웨어에 들어가지 않는다)
//   g++ -std=c++11 -O1 -I. test/sim/house_wind.cpp app_TempControl.cpp Settings.cpp -o /tmp/hw && /tmp/hw
// 바람이 세면 환기가 세지고(× wind) 효과가 빨리 온다(120초 ÷ wind). 해는 30분마다 ±30% (구름)
// 2026-11-28 결과 (5시간 중 뒤 4시간, 28도에서 벗어난 RMS / 움직인 횟수):
//   지금 dev(고정 단계):       60초 2.8~4.1도 / 160~180번, 300초 2.1~3.3도 / 40~60번
//   효과 기다림 + 효과의 세기: 60초 1.1~1.3도 / 41~86번,  300초 1.1~1.3도 / 26~29번
//   해가 일정하면 300초에서 0.04~0.38도 / 3~9번. 구름이 있으면 효과의 세기 측정이 섞인다(해는 따로 다룰 것)
#include <cstdio>
#include <cmath>
#include <initializer_list>
#include "app_TempControl.h"
#include "Settings.h"
// 바람만 바꾸는 하우스 모형: 해는 일정. 바람이 세면 환기가 세지고(× wind) 효과가 빨리 온다(120초 ÷ wind)
struct H {
    TempControl tc; TempSettings t; TempControl::WinView v[2];
    double T=30, out=15, sun=1.25, leak=0.05, vent=1.0, wind; uint32_t delayMs, now=0; cs_t hist[4000]={};
    unsigned jobs=0; double se=0, mx=-1e9, mn=1e9; unsigned n=0; unsigned seed=1;
    H(double w, uint16_t period): wind(w) {
        delayMs = (uint32_t)(120000 / w);
        SettingsRules::defaultsTemp(t); t.sensorAuto=true; t.periodS=period; tc.setSettings(t); tc.begin(0);
        for(int i=0;i<2;i++){v[i].eligible=true;v[i].posState=PosState::VALID;v[i].pos=1000;v[i].home=1000;v[i].limit=15000;}
    }
    double noise(){ seed = seed*1103515245u+12345u; return ((seed>>16)%3) - 1.0; }
    void run(uint32_t ms){ for(uint32_t e=0;e<ms;e+=1000){ uint32_t sec=now/1000; hist[sec%4000]=v[0].pos;
        uint32_t b=delayMs/1000; cs_t f= sec>=b? hist[(sec-b)%4000]:1000;
        double frac = f <= 1000 ? 0 : (f-1000)/14000.0;
        double s2 = sun * (1 + 0.3*sin(now/1800000.0*6.283));
        T += (s2 - (leak + vent*frac*wind)*(T-out)) / 60.0;
        Job j[2]; uint8_t k=tc.tick(now,true,(int16_t)lround(T*10+noise()),false,v,j);
        for(uint8_t q=0;q<k;q++){auto&w=v[j[q].window]; if(j[q].type==JobType::CLOSE){w.pos=0;w.posState=PosState::CLOSED;} else {w.pos=j[q].target;w.posState=PosState::VALID;}}
        if(k) jobs++;
        if(now>=3600000){ double e2=T-28; se+=e2*e2; n++; if(T>mx)mx=T; if(T<mn)mn=T; }
        now+=1000; } }
};
int main(){
  for (int p : {60, 300, 600}) for (double w : {1.0, 2.0, 4.0}) {
    H h(w, (uint16_t)p); h.run(5*3600*1000u);
    printf("T PERIOD %3ds wind x%.0f (delay %3us): RMS %.2fC  %4.1f~%4.1f  moves %3u  gain %ld n %u\n", p, w, h.delayMs/1000, sqrt(h.se/h.n), h.mn, h.mx, h.jobs, (long)h.tc.gain(), h.tc.gainCount());
  }
}
