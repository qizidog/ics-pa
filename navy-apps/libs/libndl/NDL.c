#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static struct timeval ndl_tv;

uint32_t NDL_GetTicks() {
  // 以毫秒为单位返回系统时间
  // Get the number of milliseconds since the SDL library initialization
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    exit(-1);
  }
  return (tv.tv_sec*1000+tv.tv_usec/1000) - (ndl_tv.tv_sec*1000+ndl_tv.tv_usec/1000);
}

// 读出一条事件信息, 将其写入`buf`中, 最长写入`len`字节
// 若读出了有效的事件, 函数返回1, 否则返回0
int NDL_PollEvent(char *buf, int len) {
  int fd = open("/dev/events", 0);
  int ret = read(fd, buf, len);
  close(fd);
  return ret;
}

static int canvas_w = 0, canvas_h = 0;

// 打开一张(*w) X (*h)的画布
// 如果*w和*h均为0, 则将系统全屏幕作为画布, 并将*w和*h分别设为系统屏幕的大小
void NDL_OpenCanvas(int *w, int *h) {
  int fd = open("/proc/dispinfo", 0);
  char dispinfo_buf[64];
  read(fd, dispinfo_buf, sizeof(dispinfo_buf));
  sscanf(dispinfo_buf, "WIDTH:%d\nHEIGHT:%d\n", &screen_w, &screen_h);
  close(fd);

  printf("screen width: %d, height: %d\n", screen_w, screen_h);

  if (*w == 0 && *h == 0) {
    *w = screen_w;
    *h = screen_h;
  }
  if (*w <= 0 || *w > screen_w) *w = screen_w;
  if (*h <= 0 || *h > screen_h) *h = screen_h;
  canvas_w = *w;
  canvas_h = *h;

  printf("canvas width: %d, height: %d\n", canvas_w, canvas_h);

  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w; screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1) {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0) continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0) break;
    }
    close(fbctl);
  }
}

// 向画布`(x, y)`坐标处绘制`w*h`的矩形图像, 并将该绘制区域同步到屏幕上
// 图像像素按行优先方式存储在`pixels`中, 每个像素用32位整数以`00RRGGBB`的方式描述颜色
void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  // x, y 为像素相对画布的坐标
  int fd = open("/dev/fb", 0);

  // 得到使画布在屏幕上居中的左上角点坐标（画布相对屏幕的坐标）
  int canvas_x = (screen_w - canvas_w) / 2;
  int canvas_y = (screen_h - canvas_h) / 2;

  for (int row=0; row<h; row++) {  // 一次写一行
    // lseek 实现canvas坐标到screen坐标的转换
    lseek(fd, ((canvas_y+y+row)*screen_w+canvas_x+x)*sizeof(uint32_t), SEEK_SET);
    write(fd, &pixels[row*w], (w<canvas_w-x?w:canvas_w-x)*sizeof(uint32_t));  // 像素转换为字节
  }
  close(fd);
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  // init time
  if (gettimeofday(&ndl_tv, NULL) != 0) exit(-1);

  if (getenv("NWM_APP")) {
    evtdev = 3;
  }
  return 0;
}

void NDL_Quit() {
}
