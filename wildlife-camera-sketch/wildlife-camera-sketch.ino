#include <SDHCI.h>
#include <RTC.h>

#define DEBUG true
static SDClass  theSD;

void setup() {
  if (DEBUG) {
    Serial.begin(115200); 
    Serial.println("INFO: wildlife_camera initializing on wakeup...");
  }

  /* Initialize SD */
  while (!theSD.begin()) 
  {
    /* wait until SD card is mounted. */
    if (DEBUG) Serial.println("Insert SD card.");
  }

  RTC.begin();
  
  setupGnss();
  if (DEBUG) Serial.println("INFO: gnss started");

  sleep(5000);
  // device attempts to connect to cellular
  ei_wildlife_camera_connect_cellular(DEBUG);

  //camera starts continuously classifying video feed at 5fps
  ei_wildlife_camera_start_continuous(DEBUG);
}

void loop() {
  // this routine is used to validate when we have valid GNSS data
  // camera video feed events have generally been paused if this is running
  if (!loopGnss()) {

    if (DEBUG) {
      Serial.println("gnss update failed, data:");
      Serial.println(sprintNavData());
    }
  } else if (DEBUG) {
    Serial.println("gnss update:");
    Serial.println(sprintNavData());
  }

  // retry cellular connection
  ei_wildlife_camera_connect_cellular(DEBUG);
  // restart continuously classifying video feed at 5fps
  sleep(5000);
  ei_wildlife_camera_start_continuous(DEBUG);
}
