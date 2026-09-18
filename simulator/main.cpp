#include <chrono>
#include <ctime>
#include <thread>

#include <lvgl.h>
#include <sdl/sdl.h>

#include "clock_ui.h"

namespace {
constexpr int ScreenWidth = 1024;
constexpr int ScreenHeight = 600;
constexpr int DrawBufferRows = 100;
lv_color_t drawBufferPixels[ScreenWidth * DrawBufferRows];
}

int main() {
  lv_init();
  sdl_init();

  static lv_disp_draw_buf_t drawBuffer;
  lv_disp_draw_buf_init(&drawBuffer, drawBufferPixels, nullptr, ScreenWidth * DrawBufferRows);
  static lv_disp_drv_t displayDriver;
  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = ScreenWidth;
  displayDriver.ver_res = ScreenHeight;
  displayDriver.flush_cb = sdl_display_flush;
  displayDriver.draw_buf = &drawBuffer;
  lv_disp_drv_register(&displayDriver);

  static lv_indev_drv_t inputDriver;
  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = sdl_mouse_read;
  lv_indev_drv_register(&inputDriver);

  createClockUi();
  const std::time_t now = std::time(nullptr);
  tm localTime = {};
  localtime_r(&now, &localTime);
  setClockUiDateTime(localTime);

  auto previous = std::chrono::steady_clock::now();
  auto previousClockUpdate = previous;
  while (true) {
    const auto current = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current - previous);
    if (elapsed.count() > 0) {
      lv_tick_inc(static_cast<uint32_t>(elapsed.count()));
      previous = current;
    }
    if (current - previousClockUpdate >= std::chrono::seconds(1)) {
      updateClockUi();
      previousClockUpdate = current;
    }
    lv_timer_handler();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}
