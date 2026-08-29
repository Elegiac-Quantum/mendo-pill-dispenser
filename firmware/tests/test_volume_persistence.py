"""Exercise the production volume functions with NVS key limits and a reboot."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'components/pill_ai_transport/transport_ws.c').read_text(encoding='utf-8-sig')
functions = source[source.index('uint8_t pill_ai_volume(void)'):source.index('uint16_t pill_ai_vad_threshold(void)')]
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef int nvs_handle_t;
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define NVS_READWRITE 1
static uint8_t s_volume = 60;
static bool s_volume_loaded;
static char keys[8][16];
static uint8_t values[8];
static int count;
static int nvs_open(const char *n, int m, nvs_handle_t *h) {(void)n;(void)m;*h=1;return 0;}
static int nvs_get_u8(nvs_handle_t h,const char *k,uint8_t *v) {
 (void)h; if(strlen(k)>15)return 1;
 for(int i=0;i<count;i++)if(!strcmp(k,keys[i])){*v=values[i];return 0;} return 1;
}
static int nvs_set_u8(nvs_handle_t h,const char *k,uint8_t v) {
 (void)h; if(strlen(k)>15)return 1;
 for(int i=0;i<count;i++)if(!strcmp(k,keys[i])){values[i]=v;return 0;}
 assert(count<8);strcpy(keys[count],k);values[count++]=v;return 0;
}
static int nvs_commit(nvs_handle_t h){(void)h;return 0;}
static void nvs_close(nvs_handle_t h){(void)h;}
'''
checks = r'''
int main(void) {
 assert(pill_ai_volume()==100);
 assert(pill_ai_set_volume(35)==ESP_OK);
 s_volume_loaded=false;
 assert(pill_ai_volume()==35);
 assert(pill_ai_set_volume(0)==ESP_OK);
 s_volume_loaded=false;
 assert(pill_ai_volume()==0);
 assert(pill_ai_set_volume(101)==ESP_ERR_INVALID_ARG);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
    path = pathlib.Path(folder)
    (path/'volume.c').write_text(harness+functions+checks)
    subprocess.run(['C:/Strawberry/c/bin/gcc.exe', '-std=c17', str(path/'volume.c'), '-o', str(path/'volume.exe')], check=True)
    subprocess.run([str(path/'volume.exe')], check=True)
print('PASS: lowered volume and mute persist across simulated reboot')
