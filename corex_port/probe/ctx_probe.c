// Direct CUDA Driver API context + module probe against CoreX libcuda.so
#include <stdio.h>
#include <dlfcn.h>
#include <time.h>

typedef int (*fn_i)(int);
typedef int (*fn_pi)(int*);
typedef int (*fn_pii)(int*,int);
typedef int (*fn_ctx)(void**,unsigned int,int);
typedef int (*fn_p)(void*);

static double now(){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec + t.tv_nsec/1e9; }

int main(){
    void* h = dlopen("libcuda.so.1", RTLD_NOW);
    if(!h) h = dlopen("libcuda.so", RTLD_NOW);
    if(!h){ printf("dlopen FAILED: %s\n", dlerror()); return 1; }
    fn_i cuInit = (fn_i)dlsym(h,"cuInit");
    fn_pii cuDeviceGet = (fn_pii)dlsym(h,"cuDeviceGet");
    fn_ctx cuCtxCreate = (fn_ctx)dlsym(h,"cuCtxCreate_v2");
    fn_p cuCtxDestroy = (fn_p)dlsym(h,"cuCtxDestroy_v2");

    printf("cuInit rc=%d\n", cuInit(0)); fflush(stdout);
    int dev=-1; printf("cuDeviceGet rc=%d dev=%d\n", cuDeviceGet(&dev,0), dev); fflush(stdout);

    printf("calling cuCtxCreate_v2(flags=0) ...\n"); fflush(stdout);
    void* ctx=0; double t0=now();
    int rc = cuCtxCreate(&ctx, 0, dev);
    double t1=now();
    printf("cuCtxCreate_v2 rc=%d ctx=%p took=%.2fs\n", rc, ctx, t1-t0); fflush(stdout);
    if(rc==0 && cuCtxDestroy){ printf("cuCtxDestroy rc=%d\n", cuCtxDestroy(ctx)); }
    return 0;
}
