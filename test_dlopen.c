#include <dlfcn.h>
#include <stdio.h>
int main() {
    void* handle = dlopen("/mnt/Trash/@home/angeloo123/Games/steamapps/common/Half-Life/ag/cl_dlls/client.so", RTLD_NOW);
    if (!handle) {
        printf("Error: %s\n", dlerror());
        return 1;
    }
    printf("Success!\n");
    return 0;
}
