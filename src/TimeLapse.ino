/**
 * This project helps to create a "timelapse" video by saving a camera picture on SD card at a configurable interval
 *
 * Select board guideline:
 * - ESP32-S3-WROOM1 CAM (resembels "Freenove ESP32-Wrover CAM") -> ESP32S3 Dev Module
 * - ESP32-S3-CAM - new version -> ESP32S3 Dev Module - needs new serial adaptor to see anything on console; however flashing works with old ESP32-CAM adapter, just need to guess the camera IP without logs
 * - XIAO_ESP32S3 -> XIAO_ESP32S3 !! to activate SD might need to solder jumper, see https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/ !!
 * - ESP32-CAM (old ESP32 camera board) -> AI Thinker ESP32-CAM
 * - for other boards, select the appropriate board in camera.h
 *
 * Startup:
 * First you need to connect to the camera URL (check console) and adjust the resolution and other knobs
 * Then you can start the timelapse capture from the same web interface
 * The SD card must be formatted FAT or FAT32
 * After incomplete writing the SD card mounting might fail
 * Big SD cards (>32GB) might not be supported.
 *
 * This is a fork from https://github.com/bitluni/ESP32CamTimeLapse. AFAIK the ESP32CamTimeLapse project is based on CameraWebServer.ino project
 *
 * I fixed the timelapse for the board ESP32-S3-WROOM1 CAM camera, that resembles "Freenove ESP32-Wrover CAM".
 * Should still work with the original ESP32-CAM - tested with one.
 *
 * Steps:
 *  - format a SD card up to 32GB (not more) as FAT or FA32
 *  - insert the SD card in the board
 *  - Set the Port and Board in Arduino IDE
 *      - For ESP32-S3-WROOM1 CAM, set the board to "ESP32S3 Dev Module"
 *      - For ESP32-CAM (old version) set the board to "AI Thinker ESP32-CAM"
 *  - only if you have a camera other than the 2 above
 *     - set the camera in camera.cpp
 *     - change the SD card gpios in file.cpp -> initFileSystem()
 *  - configure the WiFi with ssid and password - only 2.4Ghz works usually
 *  - activate Tools/PSRAM/OPI PSRAM - if available
 *  - burn the image to the board; you might need to keep the Boot/IO0 button pushed before connecting the USB cable to the board.
 *  - boot/reset the ESP32 board
 *  - check the console for Camera Ready! Use 'http://xx.xx.xx.xx/' to connect (IP assigned by WiFi)
 *  - connect to the URL
 *  - check Still/Stream
 *  - increase resolution to the maximum that does not freeze
 *  - start the Time-Lapse from the same web menu
 *  - you can now disconnect from the web interface
 *  - after board restart, you need to re-start Time-Lapse
 *  - if you want to autostat Time-Lapse, you can edit lapse.cpp and comment if(!lapseRunning) return false; - however you might need to change defaults for resolution also
 *
 *
 * No warranties.
 * I am a noob in ESP32, hope this help. Feel free to suggest more elegant solutions.
 *
 */

#include "file.h"
#include "camera.h"
#include "time.h"
#include "esp_sleep.h"
#include "FS.h"
#include "SD_MMC.h"

#define LED_BUILTIN 38

// RTC_DATA_ATTR static unsigned long imgNumber = 0; // This will persist in RTC memory
// last seq number filename
#define LAST_SEQ_FILE "/last.txt"
uint32_t getNextSeqNumber() {
  // get the last sequence number from filesystem
  File seqfile = SD_MMC.open(LAST_SEQ_FILE, FILE_READ);
  if (!seqfile) {
    Serial.println("Failed to open last sequence file");
    return 0;
  } else {
    char buf[32];
    int len = seqfile.readBytes(buf, 32);
    uint32_t imgNumber = 0;
    if (len > 0) {
      buf[len] = 0;
      Serial.printf("Read %d bytes from last sequence file: %s\n", len, buf);
      imgNumber = atol(buf);
    } else Serial.println("Empty last sequence file");
    seqfile.close();
    return imgNumber;
  }
}
void setNextSeqNumber(uint32_t imgNumber) {
  // save the last sequence number to filesystem in a new file
  File seqfile = SD_MMC.open(LAST_SEQ_FILE, FILE_WRITE);
  if (seqfile) {
    char buf[32];
    sprintf(buf, "%d", imgNumber);
    seqfile.write((const unsigned char *)buf, strlen(buf));
    seqfile.close();
    Serial.printf("Saved last sequence number: %d\n", imgNumber);
  }
}

void takePicture() {
  // take 1 picture
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  fb = esp_camera_fb_get();
  if (fb) {
    char path[32];
    auto imgNumber = getNextSeqNumber();
    sprintf(path, "/IMG_%05d.jpg", imgNumber);
    Serial.printf("Saving image to: %s\n", path);

    if (writeFile(path, (const unsigned char *)fb->buf, fb->len)) {
      esp_camera_fb_return(fb);
      Serial.println("Capture complete");
      setNextSeqNumber(++imgNumber);
    } else {
      Serial.println("Failed to save image");
    }
  } else {
    Serial.println("Camera capture failed");
  }
  // Prepare for deep sleep
  esp_camera_deinit();
  digitalWrite(PWDN_GPIO_NUM, HIGH); // power off camera
  Serial.println("Going to deep sleep for 10 seconds...");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(10 * 1000000);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nStarting ESP32 timelapse");
  Serial.flush();
  Serial.setDebugOutput(true);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Flash ON");

  initFileSystem();
  Serial.println("After initFileSystem SD");
  print_SD_free_space();

  initCamera();
  takePicture();

  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Flash OFF");
}

void loop() {
  delay(1000); // This won't be reached due to deep sleep
}
