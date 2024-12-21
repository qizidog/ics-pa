#include <am.h>
#include <nemu.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {
  /* uint32_t wh = inl(VGACTL_ADDR); */
  /* int i; */
  /* int w = (wh >> 16); */
  /* int h = (wh & 0xffff); */
  /* uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR; */
  /* for (i = 0; i < w * h; i ++) fb[i] = i; */
  /* outl(SYNC_ADDR, 1); */
}

// AM显示控制器信息, 可读出屏幕大小信息width和height
void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  uint32_t wh = inl(VGACTL_ADDR);
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = (wh >> 16), .height = (wh & 0xffff),
    .vmemsz = 0
  };
}

// AM帧缓冲控制器, 可写入绘图信息, 向屏幕(x, y)坐标处绘制w*h的矩形图像
// x向右，y向下
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
  int x = ctl->x, y = ctl->y;
  int w = ctl->w, h = ctl->h;
  uint32_t * pixels = ctl->pixels;

  uint32_t sw = inl(VGACTL_ADDR) >> 16; // screen width
  uint32_t *fb = (uint32_t *)(uintptr_t) FB_ADDR;  // force cast

  for (int j = y; j < y + h; j++) {
    for (int i = x; i < x + w; i++) {
      fb[sw*j+i] = pixels[w*(j-y)+(i-x)];
      /* outl(FB_ADDR+(j*sw+i)*4, pixels[w*(j-y)+(i-x)]); */  // 和上一行效果一致
    }
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
