/* Low power GNSS utility functions, derived from:
 *  https://developer.sony.com/file/download/spresense-low-power-demo
 */

#include <GNSS.h>
#include <LowPower.h>
#include <stdlib.h>

#define GNSS_UPDATE_MAX_TRIES 5

#define STRING_BUFFER_SIZE   200

/* static variables */
static SpGnss  Gnss;
static SpNavData data;
static RtcTime ref_time(2019, 1, 1, 0, 0, 0, 0);
static char buffer[STRING_BUFFER_SIZE];

/* prototypes */
bool updateRtc(SpGnssTime *satTime);


/* Print navigation data contents on serial output*/
char * sprintNavData() {
  Gnss.getNavData(&data);
  if (data.posDataExist) {
    SpGnssTime time = data.time;

    snprintf(buffer, STRING_BUFFER_SIZE,
            "%d-%02d-%02d_%02d-%02d-%02d_lat%.6f_long%.6f",
            time.year, time.month, time.day,
            time.hour, time.minute, time.sec,
            data.latitude, data.longitude);
  } else {
    snprintf(buffer, STRING_BUFFER_SIZE, "[no data available]   Satellites:%d\n",
             data.numSatellites);
  }

  return buffer;
}

/* Update RTC with GNSS time if it is valid and they are not in sync */
bool updateRtc(SpGnssTime *satTime) {
  RtcTime rtc = RTC.getTime();
  RtcTime gps(satTime->year, satTime->month, satTime->day,
              satTime->hour, satTime->minute, satTime->sec,
              satTime->usec * 1000);

  /* Sanity check */
  if (gps.unixtime() < ref_time.unixtime()) {
    return false;
  }

  /* Sync RTC with GPS time */
  if (abs((int)gps - (int)rtc) >= 1) {
    RTC.setTime(gps);
  }

  /* Allow sleep if we have a valid time */
  if (rtc.unixtime() > 100) {
    return true;
  }
  return false;
}

bool getGnssUpdate() {
  if (Gnss.waitUpdate()) {
    Gnss.getNavData(&data);

    /* Check that we got data that is not cached etc. but from GNSS (type 1) */
    if (data.posDataExist && data.posFixMode && data.type == 1) {
      return updateRtc(&data.time);
    }
  }
  return false;
}

/* Check if RTC has been set by comparing to a reference time/date */
bool isRtcValid() {
  RtcTime rtc = RTC.getTime();
  return rtc.unixtime() > ref_time.unixtime();
}

/* Set GNSS time from RTC if RTC is valid */
void setGnssTime() {
  if (isRtcValid()) {
    SpGnssTime gnssTime;
    RtcTime rtc = RTC.getTime();
    gnssTime.year = rtc.year();
    gnssTime.month = rtc.month();
    gnssTime.day = rtc.day();
    gnssTime.hour = rtc.hour();
    gnssTime.minute = rtc.minute();
    gnssTime.sec = rtc.second();
    gnssTime.usec = rtc.nsec() / 1000;

    Gnss.setTime(&gnssTime);
  }
}

/*
 * Initialize GNSS 
 */
void setupGnss() {
  Gnss.begin();
  Gnss.select(GPS);
  Gnss.select(GLONASS);
  setGnssTime();
  Gnss.start(HOT_START);
}

/*
 * Fetch updates until we reach GNSS_MIN_UPDATE_CNT valid ones
 */
bool loopGnss() {
  if (getGnssUpdate()) {
    Gnss.saveEphemeris();
    return true;
  } else {
    return false;
  }
}
