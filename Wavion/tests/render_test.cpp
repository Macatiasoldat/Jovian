// Test du moteur Wavion : compilation, rendu sans NaN, niveaux, repliement (aliasing), modulation, modes de voix.
#include <cstdio>
#include <chrono>
#include "../Source/Engine.h"
#include "../Source/Presets.h"
using namespace wv;

static bool finite(const std::vector<float>& v){ for(float x:v) if(!std::isfinite(x)) return false; return true; }
static float peak(const std::vector<float>& v){ float p=0; for(float x:v) p=std::max(p,std::fabs(x)); return p; }
static float rms(const std::vector<float>& v){ double s=0; for(float x:v) s+=x*x; return (float)std::sqrt(s/v.size()); }

static void renderNotes(Engine& e,const std::vector<int>& notes,double,int total,std::vector<float>& L,std::vector<float>& R,int relAt=-1)
{
    L.assign(total,0); R.assign(total,0);
    for(int n:notes) e.handle({0,EV_NOTE_ON,n,0.9f});
    int done=0; const int blk=256;
    while(done<total){
        int c=std::min(blk,total-done);
        if(relAt>=0 && done<=relAt && relAt<done+c) for(int n:notes) e.handle({0,EV_NOTE_OFF,n,0});
        e.render(&L[done],&R[done],c); done+=c;
    }
}

int main()
{
    int fails=0;
    auto check=[&](bool ok,const char* what){ std::printf("%s  %s\n",ok?"OK  ":"FAIL",what); if(!ok) ++fails; };
    const double sr=48000;

    // 0. cohérence de la table des paramètres : nombre d'entrées de chaque liste = max + 1
    {
        bool ok=true; const auto& d=defs();
        for(int i=0;i<P_COUNT;++i) if(d[i].kind==K_CHOICE){
            int n=1; for(char c:d[i].choices) if(c=='|') ++n;
            if(n!=(int)d[i].mx+1){ std::printf("     KO %s : %d entrées, max=%g\n",d[i].id.c_str(),n,d[i].mx); ok=false; }
        }
        for(int i=0;i<P_COUNT;++i) for(int j=i+1;j<P_COUNT;++j) if(d[i].id==d[j].id){ std::printf("     id en double %s\n",d[i].id.c_str()); ok=false; }
        check(ok,"table des paramètres cohérente (listes, identifiants uniques)");
    }

    // 1. tables intégrées
    {
        auto t0=std::chrono::steady_clock::now();
        const auto& lib=WavetableLibrary::get();
        double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
        bool ok=true;
        for(int i=0;i<10;++i){ const Wavetable* t=lib.table(i); float pk=t->peak(); bool f=true; for(float x:t->data) if(!std::isfinite(x)) f=false;
            std::printf("     table %d %-16s frames=%3d peak=%.3f\n",i,t->name.c_str(),t->frames,pk); ok = ok && f && pk>0.9f && pk<1.01f; }
        std::printf("     construction : %.0f ms\n",ms);
        check(ok,"10 tables intégrées finies et normalisées");
    }

    // 2. rendu de chaque combinaison table / warp / filtre
    {
        bool ok=true; Engine e; e.prepare(sr,512);
        std::vector<float> L,R;
        for(int tb=0;tb<10;++tb) for(int w=0;w<12;++w) for(int ft=-1;ft<11;++ft){
            Params p; p[pOsc(0,O_TABLE)]=tb; p[pOsc(0,O_POS)]=0.43f; p[pOsc(0,O_W1M)]=w; p[pOsc(0,O_W1A)]=0.7f;
            p[pOsc(0,O_UNI)]=5; p[P_FILT+F_ON]=ft>=0; p[P_FILT+F_TYPE]=ft<0?0:ft; p[P_FILT+F_CUT]=1500; p[P_FILT+F_RES]=0.8f;
            e.prepare(sr,512); e.setParams(p);
            renderNotes(e,{48,60,67},sr,6000,L,R,3000);
            if(!finite(L)||!finite(R)||peak(L)>1.0f||peak(R)>1.0f||rms(L)<1e-4f){ std::printf("     KO tb=%d warp=%d filt=%d pk=%.2f rms=%.5f\n",tb,w,ft,peak(L),rms(L)); ok=false; }
        }
        check(ok,"tables x warps x filtres : fini, borné, non silencieux");
    }

    // 3. repliement : onde en dent de scie, note haute, le spectre doit rester harmonique
    {
        Engine e; e.prepare(sr,512);
        Params p; p[pOsc(0,O_TABLE)]=0; p[pOsc(0,O_POS)]=0.5f;   // dent de scie (frame 2/4 de la morphologie)
        p[pOsc(0,O_RAND)]=0; e.setParams(p);
        const int note=96; const int total=1<<16;
        std::vector<float> L,R; renderNotes(e,{note},sr,total+4096,L,R);
        std::vector<cd> a(total);
        for(int i=0;i<total;++i){ double w=0.5-0.5*std::cos(2*kPi*i/total); a[i]=cd(L[4096+i]*w,0); }
        fft(a,-1);
        const double f0=440.0*std::pow(2.0,(note-69)/12.0); // 2093 Hz
        double harm=0, tot=0;
        for(int k=1;k<total/2;++k){ double pw=std::norm(a[k]); tot+=pw; double fr=k*sr/total; double h=fr/f0; if(std::fabs(h-std::round(h))*f0<45.0) harm+=pw; }
        double nonH=10*std::log10(std::max(1e-30,(tot-harm)/tot));
        std::printf("     énergie hors harmoniques : %.1f dB\n",nonH);
        check(nonH<-45.0,"pas de repliement audible (note 96, mip-maps)");
    }

    // 4. matrice de modulation + LFO + enveloppe : le timbre doit varier
    {
        Engine e; e.prepare(sr,512);
        Params p; p[pOsc(0,O_TABLE)]=0; p[pOsc(0,O_POS)]=0.5f; p[P_FILT+F_ON]=1; p[P_FILT+F_CUT]=400; p[P_FILT+F_RES]=0.05f;
        p[pEnv(1,E_A)]=1; p[pEnv(1,E_D)]=600; p[pEnv(1,E_S)]=0;
        p[pMod(0,0)]=MS_ENV2; p[pMod(0,1)]=MD_CUT; p[pMod(0,2)]=0.6f;
                e.setParams(p);
        std::vector<float> L,R; renderNotes(e,{55},sr,48000,L,R);
        auto seg=[&](int a,int n){ std::vector<float> s(L.begin()+a,L.begin()+a+n); return rms(s); };
        float early=seg(2000,2000), late=seg(40000,2000);
        std::printf("     rms début=%.3f fin=%.3f\n",early,late);
        check(finite(L)&&early>late*1.15f,"enveloppe -> cutoff assombrit le son");
    }

    // 5. modes de voix, glide, sustain, polyphonie complète + unison 16 + réverb/delay/chorus/drive
    {
        Engine e; e.prepare(sr,512);
        Params p; p[P_MODE]=2; p[P_GLIDE]=120; p[P_REV+R_ON]=1; p[P_DLY+Y_ON]=1; p[P_CHO+C_ON]=1; p[P_DRV+D_ON]=1; p[P_DRV+D_TYPE]=4;
        p[pOsc(0,O_UNI)]=16; p[pOsc(1,O_ON)]=1; p[pOsc(1,O_UNI)]=16; p[P_SUB+S_ON]=1; p[P_NOISE+N_LEVEL]=0.3f; p[P_NOISE+N_TYPE]=1;
        e.setParams(p);
        std::vector<float> L(48000),R(48000);
        e.handle({0,EV_NOTE_ON,60,0.8f}); e.handle({100,EV_NOTE_ON,64,0.8f});
        e.render(L.data(),R.data(),24000); e.handle({0,EV_NOTE_OFF,64,0}); e.handle({0,EV_NOTE_OFF,60,0});
        e.render(L.data()+24000,R.data()+24000,24000);
        check(finite(L)&&finite(R)&&peak(L)<1.0f,"mode legato + glide + tous les effets");
        // polyphonie saturée
        Params q; q[pOsc(0,O_UNI)]=16; q[pOsc(1,O_ON)]=1; q[pOsc(1,O_UNI)]=16; q[P_FILT+F_ON]=1; q[P_FILT+F_TYPE]=8;
        e.prepare(sr,512); e.setParams(q);
        for(int n=36;n<36+24;++n) e.handle({0,EV_NOTE_ON,n,1.0f});
        auto t0=std::chrono::steady_clock::now();
        const int secs=2; std::vector<float> a(512),b(512);
        for(int i=0;i<secs*48000/512;++i) e.render(a.data(),b.data(),512);
        double dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
        std::printf("     pire cas (16 voix x 2 osc x 16 unison + échelle) : %.2f x temps réel (%d voix)\n",dt/secs,e.activeVoices());
        check(e.activeVoices()==16&&finite(a),"16 voix, vol de voix sans plantage");
    }

    // 6. charge typique
    {
        Engine e; e.prepare(sr,512);
        Params p; p[pOsc(0,O_UNI)]=7; p[pOsc(1,O_ON)]=1; p[pOsc(1,O_UNI)]=3; p[pOsc(0,O_W1M)]=1; p[pOsc(0,O_W1A)]=0.4f;
        p[P_FILT+F_ON]=1; p[P_FILT+F_TYPE]=1; p[P_REV+R_ON]=1; p[P_CHO+C_ON]=1; e.setParams(p);
        for(int n:{48,55,60,64,67,72}) e.handle({0,EV_NOTE_ON,n,0.9f});
        auto t0=std::chrono::steady_clock::now(); std::vector<float> a(512),b(512);
        for(int i=0;i<2*48000/512;++i) e.render(a.data(),b.data(),512);
        double dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
        std::printf("     accord de 6 notes, unison 7+3, warp, filtre, reverb, chorus : %.3f x temps réel\n",dt/2);
        check(dt/2<0.5,"charge CPU raisonnable sur un cas typique");
    }

    // 7. presets d'usine
    {
        bool ok=true; std::vector<float> L,R;
        for(const auto& pr:presets()){
            Params p; for(auto& kv:pr.v) p[kv.first]=kv.second;
            Engine e; e.prepare(sr,512); e.setParams(p);
            const int mode=(int)p[P_MODE];
            renderNotes(e,mode?std::vector<int>{48}:std::vector<int>{48,55,64},sr,48000,L,R,30000);
            float pk=std::max(peak(L),peak(R)); float r=rms(L);
            bool good=finite(L)&&finite(R)&&pk<=1.0f&&pk>0.05f;
            std::printf("     %-20s pic=%.2f rms=%.3f %s\n",pr.name,pk,r,good?"":"<-- KO"); ok=ok&&good;
        }
        check(ok,"presets d'usine : sonores, finis, sans saturation");
    }

    // 8. fuzz : paramètres et notes aléatoires (détecte NaN, débordements, instabilités)
    {
        Rng rg; rg.s=4242u; bool ok=true; Engine e; e.prepare(sr,512);
        const auto& d=defs(); std::vector<float> L(512),R(512);
        for(int it=0;it<300&&ok;++it){
            Params p;
            for(int i=0;i<P_COUNT;++i){ if(d[i].id.rfind("reserved",0)==0) continue;
                float u=rg.uni(); float v=d[i].mn+u*(d[i].mx-d[i].mn);
                if(d[i].kind!=K_FLOAT) v=std::floor(v+(d[i].kind==K_BOOL?0.5f:0.f));
                p[i]=v; }
            e.prepare(sr,512); e.setParams(p);
            for(int b=0;b<30;++b){
                if(rg.uni()<0.4f) e.handle({0,EV_NOTE_ON,24+(int)(rg.uni()*80),rg.uni()});
                if(rg.uni()<0.3f) e.handle({0,EV_NOTE_OFF,24+(int)(rg.uni()*80),0});
                if(rg.uni()<0.1f) e.handle({0,EV_BEND,0,rg.bi()});
                if(rg.uni()<0.1f) e.handle({0,EV_SUSTAIN,0,rg.uni()});
                if(b==10) { for(int i=0;i<P_COUNT;++i) if(d[i].kind==K_FLOAT&&rg.uni()<0.3f) p[i]=d[i].mn+rg.uni()*(d[i].mx-d[i].mn); e.setParams(p); }
                e.render(L.data(),R.data(),512);
                if(!finite(L)||!finite(R)||peak(L)>1.01f||peak(R)>1.01f){ ok=false; std::printf("     KO itération %d bloc %d pic=%.3f\n",it,b,std::max(peak(L),peak(R))); break; }
            }
        }
        check(ok,"fuzz : 300 configurations aléatoires, sorties finies et bornées");
    }

    std::printf("\n%s (%d échec%s)\n",fails?"ÉCHEC":"TOUT PASSE",fails,fails>1?"s":"");
    return fails?1:0;
}
