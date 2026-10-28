/* Independent default DRAEC reconstruction from checksum-pinned native
 * assembly and stage/state comparisons; no official library is used here.
 *
 * Fixed supported configuration: two microphones, one reference, 320 bins,
 * four previous microphone frames (8 taps), five reference frames (5 taps),
 * 6 dB output gain, forget=.995, cyclic 20-bin updates, diagonal=100.
 *
 * State layout (float32 scalar counts): current 320*4, history 320*26,
 * Cholesky diagonal 320*13, lower triangle 320*156, filters 320*52,
 * update subset 20*26, transformed vector 26, feedback vector 26.
 * control[0] is the frequency cursor; control[1] the 20-bit RNG state.
 * Compile with -ffp-contract=off and without fast-math.
 */
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
typedef struct {float re,im;} cx;
#define BINS 320
#define TAPS 13
#define SUBBINS 20
#define LOWER 78
#define SIZE (BINS*251+SUBBINS*26+52)

size_t fsmn_kws_draec_state_size(void) { return SIZE; }
/* Twelve uniform draws approximate N(0,1); preserve sequential float32 sums. */
static float randn(uint32_t *state){
    float value=0;
    for(int i=0;i<12;i++){
        *state=(*state*2045u+1)&0xfffff;
        value+=(float)*state*0x1p-20f;
    }
    return value-6.0f;
}
void fsmn_kws_draec_init(float *storage,uint32_t *control){
    memset(storage,0,SIZE*sizeof(float));control[0]=control[1]=0;
    float *diag=storage+BINS*(4+26);
    for(int i=0;i<BINS*TAPS;i++)diag[i]=100;
}
void fsmn_kws_draec_step(float *storage,uint32_t *control,const cx *mic,const cx *ref,cx *output,cx *echo){
    cx *current=(cx*)storage;
    cx *history=(cx*)(storage+BINS*4);
    float *diagonal=storage+BINS*(4+26);
    cx *lower=(cx*)(storage+BINS*(4+26+13));
    cx *filter=(cx*)(storage+BINS*(4+26+13+156));
    cx *subset=(cx*)(storage+BINS*251);
    cx *v=subset+SUBBINS*TAPS;
    cx *feedback=v+TAPS;
    const float gain=1.9952623844146729f;
    const float root=sqrtf(1.0f-.995f),invforget=1.0f/sqrtf(.995f);
    /* Emit with old filters, then update a contiguous frequency subset. */
    for(int b=0;b<BINS;b++){
        cx *h=history+b*TAPS;
        memmove(h+2,h,6*sizeof(cx));memcpy(h,current+b*2,2*sizeof(cx));
        memmove(h+9,h+8,4*sizeof(cx));h[8]=ref[b];
        for(int c=0;c<2;c++){
            cx x=mic[c*BINS+b];current[b*2+c]=x;
            cx near={0,0},far={0,0};cx *f=filter+(b*2+c)*TAPS;
            for(int k=0;k<8;k++){
                near.re+=h[k].re*f[k].re-h[k].im*f[k].im;
                near.im+=f[k].re*h[k].im+f[k].im*h[k].re;
            }
            for(int k=8;k<TAPS;k++){
                far.re+=f[k].re*h[k].re-f[k].im*h[k].im;
                far.im+=h[k].im*f[k].re+h[k].re*f[k].im;
            }
            echo[c*BINS+b]=far;
            output[c*BINS+b]=(cx){((x.re-near.re)-far.re)*gain,((x.im-near.im)-far.im)*gain};
        }
    }
    int cursor=control[0];
    memcpy(subset,history+cursor*TAPS,SUBBINS*TAPS*sizeof(cx));
    /* One real jitter value per tap, shared by all 20 updated frequencies. */
    for(int k=0;k<TAPS;k++){
        float noise=randn(control+1)*1e-4f;
        for(int b=0;b<SUBBINS;b++)subset[b*TAPS+k].re+=noise;
    }
    for(int sb=0;sb<SUBBINS;sb++){
        int b=cursor+sb;
        float energy=0;
        for(int c=0;c<2;c++){
            cx x=current[b*2+c],y=output[c*BINS+b];
            float a=x.re*x.re+x.im*x.im;
            float e=(y.re*y.re+y.im*y.im)*(1.0f/(gain*gain));
            energy+=e<=a?e:a;
        }
        energy+=.01f;
        float weight=powf(energy,-.45f);
        float scaled=root*weight;
        float negative=-invforget*scaled;
        float *diag=diagonal+b*TAPS;cx *l=lower+b*LOWER,*h=subset+sb*TAPS;
        for(int k=0;k<TAPS;k++){
            cx sum={0,0};
            for(int j=0;j<k;j++){
                cx a=h[j],q=l[k*(k-1)/2+j];
                sum.re+=a.re*q.re-a.im*q.im;
                sum.im+=q.re*a.im+q.im*a.re;
            }
            v[k]=(cx){(diag[k]*h[k].re+sum.re)*negative,(h[k].im*diag[k]+sum.im)*negative};
        }
        /* Sequential complex rotations update the triangular factor. */
        memset(feedback,0,TAPS*sizeof(cx));
        float norm=1,previous=1,inverse=1;
        for(int k=0;k<TAPS;k++){
            norm+=v[k].re*v[k].re+v[k].im*v[k].im;
            float next=sqrtf(norm);inverse=1.0f/next;
            float c=v[k].re*inverse,s=-v[k].im*inverse,alpha=previous*inverse;
            for(int j=0;j<=k;j++){
                int index=k*(k-1)/2+j;
                cx r=j==k?(cx){diag[k]*invforget,0}:(cx){l[index].re*invforget,l[index].im*invforget};
                cx t=feedback[j];
                feedback[j]=(cx){(c*r.re-s*r.im)+alpha*t.re,(c*r.im+s*r.re)+alpha*t.im};
                cx update={r.re*alpha+((-c)*t.re-s*t.im),r.im*alpha+(t.im*(-c)+t.re*s)};
                if(j==k)diag[k]=update.re;else l[index]=update;
            }
            previous=next;
        }
        float factor=scaled*inverse;
        for(int c=0;c<2;c++){
            cx e=output[c*BINS+b],*f=filter+(b*2+c)*TAPS;
            for(int k=0;k<TAPS;k++){
                cx a=feedback[k];
                f[k].re-=((e.re*a.re-e.im*a.im)*(1.0f/gain))*factor;
                f[k].im-=((a.im*e.re+a.re*e.im)*(1.0f/gain))*factor;
            }
        }
    }
    control[0]=(cursor+SUBBINS)%BINS;
}
