// 最小复现:CoreX ivcore11 驱动拒绝 NVIDIA 风格 PTX 文本。
// 手写一个零依赖最小 PTX(空 kernel),经 driver API cuModuleLoadDataEx JIT 加载,
// 期望 rc=0(CUDA_SUCCESS),实际返回 rc=200(CUDA_ERROR_INVALID_IMAGE)。
// 用 dlopen 打开 libcuda,故编译无需 cuda.h,只需 -ldl。
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

typedef int (*fn_i)(int);
typedef int (*fn_pii)(int*, int);
typedef int (*fn_ctx)(void**, unsigned int, int);
typedef int (*fn_load)(void**, const void*, unsigned int, int*, void**);

int main(void) {
  void *h = dlopen("libcuda.so.1", RTLD_NOW);
  if (!h) h = dlopen("libcuda.so", RTLD_NOW);
  if (!h) { printf("dlopen libcuda FAILED: %s\n", dlerror()); return 1; }

  fn_i   cuInit       = (fn_i)dlsym(h, "cuInit");
  fn_pii cuDeviceGet  = (fn_pii)dlsym(h, "cuDeviceGet");
  fn_ctx cuCtxCreate  = (fn_ctx)dlsym(h, "cuCtxCreate_v2");
  fn_load cuModuleLoadDataEx = (fn_load)dlsym(h, "cuModuleLoadDataEx");

  printf("cuInit rc=%d\n", cuInit(0));
  int dev = -1; cuDeviceGet(&dev, 0);
  void *ctx = 0; int rc = cuCtxCreate(&ctx, 0, dev);
  printf("cuCtxCreate rc=%d\n", rc);
  if (rc != 0) { printf("no ctx (try 'ixsmi -r' first), abort\n"); return 2; }

  // 标准 NVIDIA PTX 文本:一个空 kernel。任何合法 NV 驱动都应能 JIT 加载。
  const char *ptx =
      ".version 6.5\n.target sm_70\n.address_size 64\n"
      ".visible .entry k(){\nret;\n}\n";

  char errbuf[4096]; memset(errbuf, 0, sizeof(errbuf));
  int opts[2] = {5 /*ERROR_LOG_BUFFER*/, 6 /*ERROR_LOG_BUFFER_SIZE*/};
  void *vals[2] = {errbuf, (void *)(long)sizeof(errbuf)};
  void *mod = 0;
  int lrc = cuModuleLoadDataEx(&mod, ptx, 2, opts, vals);
  printf("cuModuleLoadDataEx rc=%d mod=%p\n", lrc, mod);
  if (errbuf[0]) printf("JIT_ERR: %s\n", errbuf);

  if (lrc == 200) { printf("REPRODUCED: driver rejects NV PTX (rc=200 CUDA_ERROR_INVALID_IMAGE)\n"); return 0; }
  printf("did NOT reproduce (rc=%d)\n", lrc);
  return 3;
}
