#include <stdio.h>
#include <dlfcn.h>

int main() {
    void* handle = dlopen("/mnt/Trash/@home/angeloo123/Games/steamapps/common/Half-Life/ag/cl_dlls/client.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    printf("Success!\n");
    dlclose(handle);
    return 0;
}
