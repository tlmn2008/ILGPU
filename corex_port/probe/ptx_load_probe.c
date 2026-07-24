// Try to JIT-load NV-style PTX text on the CoreX driver via cuModuleLoadDataEx.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef int (*fn_i)(int);
typedef int (*fn_pii)(int*,int);
typedef int (*fn_ctx)(void**,unsigned int,int);
typedef int (*fn_p)(void*);
typedef int (*fn_load)(void**, const void*, unsigned int, int*, void**);

static char* slurp(const char* p, size_t* n){
    FILE* f=fopen(p,"rb"); if(!f){printf("cannot open %s\n",p);return NULL;}
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    char* b=malloc(sz+1); fread(b,1,sz,f); b[sz]=0; if(n)*n=sz; fclose(f); return b;
}

static void try_load(void* h, const char* label, const char* ptx){
    fn_load cuModuleLoadDataEx = (fn_load)dlsym(h,"cuModuleLoadDataEx");
    fn_load cuModuleLoadData2 = 0;
    typedef int (*fn_load2)(void**, const void*);
    fn_load2 cuModuleLoadData = (fn_load2)dlsym(h,"cuModuleLoadData");

    char errbuf[4096]; memset(errbuf,0,sizeof(errbuf));
    char infobuf[4096]; memset(infobuf,0,sizeof(infobuf));
    // JIT options: 5=ERROR_LOG_BUFFER,6=ERROR_LOG_BUFFER_SIZE,3=INFO_LOG_BUFFER,4=INFO_LOG_BUFFER_SIZE
    int opts[4]={5,6,3,4};
    void* vals[4]; vals[0]=errbuf; vals[1]=(void*)(long)sizeof(errbuf);
    vals[2]=infobuf; vals[3]=(void*)(long)sizeof(infobuf);
    void* mod=0;
    int rc = cuModuleLoadDataEx(&mod, ptx, 4, opts, vals);
    printf("[%s] cuModuleLoadDataEx rc=%d mod=%p\n", label, rc, mod);
    if(errbuf[0]) printf("    JIT_ERR: %s\n", errbuf);
    if(infobuf[0]) printf("    JIT_INFO: %s\n", infobuf);
    void* mod2=0; int rc2 = cuModuleLoadData(&mod2, ptx);
    printf("[%s] cuModuleLoadData rc=%d mod=%p\n", label, rc2, mod2);
}

int main(int argc, char** argv){
    void* h = dlopen("libcuda.so.1", RTLD_NOW); if(!h) h=dlopen("libcuda.so",RTLD_NOW);
    if(!h){ printf("dlopen FAILED: %s\n", dlerror()); return 1; }
    fn_i cuInit=(fn_i)dlsym(h,"cuInit");
    fn_pii cuDeviceGet=(fn_pii)dlsym(h,"cuDeviceGet");
    fn_ctx cuCtxCreate=(fn_ctx)dlsym(h,"cuCtxCreate_v2");
    printf("cuInit=%d\n", cuInit(0));
    int dev=-1; cuDeviceGet(&dev,0);
    void* ctx=0; int rc=cuCtxCreate(&ctx,0,dev);
    printf("cuCtxCreate rc=%d\n", rc);
    if(rc!=0){ printf("no ctx, abort\n"); return 2; }

    // 1) ILGPU-generated PTX as-is (target sm_71)
    if(argc>1){ size_t n; char* p=slurp(argv[1],&n); if(p){ try_load(h,"ILGPU-sm_71", p); free(p);} }

    // 2) same PTX retargeted to sm_70
    if(argc>1){ size_t n; char* p=slurp(argv[1],&n); if(p){
        char* q=strstr(p,".target sm_71"); if(q){ memcpy(q,".target sm_70",13);} 
        try_load(h,"ILGPU-sm_70", p); free(p);} }

    // 3) minimal hand-written PTX (empty kernel), several targets/versions
    const char* mini_templates[] = {
      ".version 6.5\n.target sm_70\n.address_size 64\n.visible .entry k(){\nret;\n}\n",
      ".version 6.0\n.target sm_70\n.address_size 64\n.visible .entry k(){\nret;\n}\n",
      ".version 7.0\n.target sm_72\n.address_size 64\n.visible .entry k(){\nret;\n}\n",
      ".version 6.5\n.target sm_62\n.address_size 64\n.visible .entry k(){\nret;\n}\n",
    };
    const char* labels[]={"mini-6.5-sm70","mini-6.0-sm70","mini-7.0-sm72","mini-6.5-sm62"};
    for(int i=0;i<4;i++) try_load(h, labels[i], mini_templates[i]);
    return 0;
}
