#include "app_tasks.h"

#include "robot/cnde_client.h"
#include "robot/fairino_udp.h"
#include "robot/safe_motion.h"
#include "wifi_manager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static TaskHandle_t s_cndeTask = nullptr;
static TaskHandle_t s_motionTask = nullptr;
static volatile int s_motionError = FR_OK;
static portMUX_TYPE s_errorMux = portMUX_INITIALIZER_UNLOCKED;

static void cndeTaskMain(void* parameter) {
  (void)parameter;
  Serial.printf("[TASK] CNDE started on core %d\n", xPortGetCoreID());
  for (;;) {
    if (wifiMgrConnected() && !wifiMgrScanBusy()) {
      // CNDE connect can block for up to three seconds. Keeping it in this
      // task prevents that timeout from starving LVGL and the touch driver.
      g_cnde.tick();
      vTaskDelay(pdMS_TO_TICKS(2));
    } else {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

static void motionTaskMain(void* parameter) {
  (void)parameter;
  Serial.printf("[TASK] ServoJ started on core %d\n", xPortGetCoreID());
  for (;;) {
    if (g_safeMotion.active()) {
      const int result = g_safeMotion.tick(true);
      if (result != FR_OK) {
        portENTER_CRITICAL(&s_errorMux);
        s_motionError = result;
        portEXIT_CRITICAL(&s_errorMux);
      }
      vTaskDelay(pdMS_TO_TICKS(15));
    } else {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

bool appTasksStart() {
  if (s_cndeTask && s_motionTask) return true;

  const BaseType_t motionResult = xTaskCreatePinnedToCore(
      motionTaskMain, "servo-net", 6144, nullptr, 4, &s_motionTask, 0);
  if (motionResult != pdPASS) {
    s_motionTask = nullptr;
    Serial.println("[TASK] ERROR: ServoJ task creation failed");
    return false;
  }

  const BaseType_t cndeResult = xTaskCreatePinnedToCore(
      cndeTaskMain, "cnde-net", 8192, nullptr, 2, &s_cndeTask, 0);
  if (cndeResult != pdPASS) {
    Serial.println("[TASK] ERROR: CNDE task creation failed");
    vTaskDelete(s_motionTask);
    s_motionTask = nullptr;
    s_cndeTask = nullptr;
    return false;
  }

  Serial.printf("[TASK] background tasks ready; heap=%u\n", ESP.getFreeHeap());
  return true;
}

int appTasksTakeMotionError() {
  portENTER_CRITICAL(&s_errorMux);
  const int result = s_motionError;
  s_motionError = FR_OK;
  portEXIT_CRITICAL(&s_errorMux);
  return result;
}

void appTasksLogStats() {
  const UBaseType_t loopFree = uxTaskGetStackHighWaterMark(nullptr);
  const UBaseType_t cndeFree = s_cndeTask ? uxTaskGetStackHighWaterMark(s_cndeTask) : 0;
  const UBaseType_t motionFree = s_motionTask ? uxTaskGetStackHighWaterMark(s_motionTask) : 0;
  Serial.printf("[TASK] heap=%u psram=%u stack-free loop=%u cnde=%u servo=%u\n",
                ESP.getFreeHeap(), ESP.getFreePsram(), (unsigned)loopFree,
                (unsigned)cndeFree, (unsigned)motionFree);
}
