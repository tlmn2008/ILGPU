// Direct CUDA Driver API probe against CoreX libcuda.so
#include <stdio.h>
#include <dlfcn.h>

typedef int (*fn_i)(int);
typedef int (*fn_pi)(int*);
typedef int (*fn_pii)(int*,int);
typedef int (*fn_piii)(int*,int*,int);
typedef int (*fn_ci)(char*,int,int);
typedef int (*fn_attr)(int*,int,int);

int main(){
    void* h = dlopen("libcuda.so", RTLD_NOW);
    if(!h){ printf("dlopen libcuda.so FAILED: %s\n", dlerror()); h = dlopen("libcuda.so.1", RTLD_NOW); }
    if(!h){ printf("dlopen libcuda.so.1 FAILED: %s\n", dlerror()); return 1; }
    printf("libcuda.so loaded OK\n");

    fn_i cuInit = (fn_i)dlsym(h,"cuInit");
    fn_pi cuDriverGetVersion = (fn_pi)dlsym(h,"cuDriverGetVersion");
    fn_pi cuDeviceGetCount = (fn_pi)dlsym(h,"cuDeviceGetCount");
    fn_pii cuDeviceGet = (fn_pii)dlsym(h,"cuDeviceGet");
    fn_ci cuDeviceGetName = (fn_ci)dlsym(h,"cuDeviceGetName");
    fn_piii cuDeviceComputeCapability = (fn_piii)dlsym(h,"cuDeviceComputeCapability");
    fn_attr cuDeviceGetAttribute = (fn_attr)dlsym(h,"cuDeviceGetAttribute");

    printf("cuInit=%p count=%p ver=%p cc=%p\n",(void*)cuInit,(void*)cuDeviceGetCount,(void*)cuDriverGetVersion,(void*)cuDeviceComputeCapability);

    int rc = cuInit(0);
    printf("cuInit(0) rc=%d\n", rc);

    int ver=0; if(cuDriverGetVersion){ int r=cuDriverGetVersion(&ver); printf("cuDriverGetVersion rc=%d version=%d\n", r, ver);} 

    int n=-1; int rc2 = cuDeviceGetCount(&n);
    printf("cuDeviceGetCount rc=%d n=%d\n", rc2, n);

    for(int i=0;i<n;i++){
        int dev=-1; int r=cuDeviceGet(&dev,i);
        char name[256]={0}; if(cuDeviceGetName) cuDeviceGetName(name,255,dev);
        int major=-1,minor=-1;
        // attribute 75=COMPUTE_CAPABILITY_MAJOR, 76=MINOR
        if(cuDeviceGetAttribute){ cuDeviceGetAttribute(&major,75,dev); cuDeviceGetAttribute(&minor,76,dev);}
        int ccr=-99; if(cuDeviceComputeCapability){ int a=-1,b=-1; ccr=cuDeviceComputeCapability(&a,&b,dev); printf("  dev %d get_rc=%d name='%s' attrCC=%d.%d computeCap_rc=%d cc=%d.%d\n", i, r, name, major, minor, ccr, a, b);} 
        else printf("  dev %d get_rc=%d name='%s' attrCC=%d.%d (no cuDeviceComputeCapability)\n", i, r, name, major, minor);
    }
    return 0;
}
