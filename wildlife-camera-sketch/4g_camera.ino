/*
   Image processing and 4G transmit functionality. Derived from:
   https://developer.sony.com/develop/spresense/docs/arduino_developer_guide_en.html#_camera_library
*/

#include <Wildlife_Camera_On_Device_-_North_America_inferencing.h>
#include <LTE.h>
#include <Camera.h>

/* Top level defines. Customized per user */
#define EI_API_KEY "ei_cf83c8...." // replace with your API key
#define APP_LTE_APN "soracom.io"   // replace with your APN
#define APP_LTE_USER_NAME "sora"   // replace with your username
#define APP_LTE_PASSWORD  "sora"   // replace with your password
// replace with authentication method based on APN
#define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_CHAP) // Authentication : CHAP
// #define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_PAP) // Authentication : PAP
// #define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_NONE) // Authentication : NONE

/* Defines to center crop and resize a image to the Impulse image size the speresense hardware accelerator

   NOTE: EI_CLASSIFIER_INPUT width and height must be less than RAW_HEIGHT * SCALE_FACTOR, and must
   simultaneously meet the requirements of the spresense api:
   https://developer.sony.com/develop/spresense/developer-tools/api-reference/api-references-arduino/group__camera.html#ga3df31ea63c3abe387ddd1e1fd2724e97
*/
#define SCALE_FACTOR 4
#define RAW_WIDTH CAM_IMGSIZE_QVGA_H
#define RAW_HEIGHT CAM_IMGSIZE_QVGA_V
#define CLIP_WIDTH (EI_CLASSIFIER_INPUT_WIDTH * SCALE_FACTOR)
#define CLIP_HEIGHT (EI_CLASSIFIER_INPUT_HEIGHT * SCALE_FACTOR)
#define OFFSET_X  ((RAW_WIDTH - CLIP_WIDTH) / 2)
#define OFFSET_Y  ((RAW_HEIGHT - CLIP_HEIGHT) / 2)

// enable for very verbose logging from edge impulse sdk
#define DEBUG_NN false

#define CLASSIFIER_THRESHOLD 0.7
#define CLASSIFIER_ANIMAL_INDEX 0

// APN IP type
#define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V4V6) // IP : IPv4v6
// #define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V4) // IP : IPv4
// #define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V6) // IP : IPv6

/* RAT to use
 * Refer to the cellular carriers information
 * to find out which RAT your SIM supports.
 * The RAT set on the modem can be checked with LTEModemVerification::getRAT().
 */

#define APP_LTE_RAT (LTE_NET_RAT_CATM) // RAT : LTE-M (LTE Cat-M1)
// #define APP_LTE_RAT (LTE_NET_RAT_NBIOT) // RAT : NB-IoT

/* static variables */
static CamImage sized_img;
static ei_impulse_result_t ei_result = { 0 };

static LTE lteAccess;
static LTEClient client;
// URL, path & port (for example: arduino.cc)
static char server[] = "ingestion.edgeimpulse.com";
static char path[] = "/api/testing/files";
static int port = 80; // port 80 is the default for HTTP

/* prototypes */
void printError(enum CamErr err);
void CamCB(CamImage img);

/**
   @brief      Convert monochrome data to rgb values

   @param[in]  mono_data  The mono data
   @param      r          red pixel value
   @param      g          green pixel value
   @param      b          blue pixel value
*/
static inline void mono_to_rgb(uint8_t mono_data, uint8_t *r, uint8_t *g, uint8_t *b) {
  uint8_t v = mono_data;
  *r = *g = *b = v;
}


int ei_camera_cutout_get_data(size_t offset, size_t length, float *out_ptr) {
  size_t bytes_left = length;
  size_t out_ptr_ix = 0;

  uint8_t *buffer = sized_img.getImgBuff();

  // read byte for byte
  while (bytes_left != 0) {

    // grab the value and convert to r/g/b
    uint8_t pixel = buffer[offset];

    uint8_t r, g, b;
    mono_to_rgb(pixel, &r, &g, &b);

    // then convert to out_ptr format
    float pixel_f = (r << 16) + (g << 8) + b;
    out_ptr[out_ptr_ix] = pixel_f;

    // and go to the next pixel
    out_ptr_ix++;
    offset++;
    bytes_left--;
  }

  // and done!
  return 0;
}

/**
   Print error message
*/
void printError(enum CamErr err)
{
  Serial.print("Error: ");
  switch (err)
  {
    case CAM_ERR_NO_DEVICE:
      Serial.println("No Device");
      break;
    case CAM_ERR_ILLEGAL_DEVERR:
      Serial.println("Illegal device error");
      break;
    case CAM_ERR_ALREADY_INITIALIZED:
      Serial.println("Already initialized");
      break;
    case CAM_ERR_NOT_INITIALIZED:
      Serial.println("Not initialized");
      break;
    case CAM_ERR_NOT_STILL_INITIALIZED:
      Serial.println("Still picture not initialized");
      break;
    case CAM_ERR_CANT_CREATE_THREAD:
      Serial.println("Failed to create thread");
      break;
    case CAM_ERR_INVALID_PARAM:
      Serial.println("Invalid parameter");
      break;
    case CAM_ERR_NO_MEMORY:
      Serial.println("No memory");
      break;
    case CAM_ERR_USR_INUSED:
      Serial.println("Buffer already in use");
      break;
    case CAM_ERR_NOT_PERMITTED:
      Serial.println("Operation not permitted");
      break;
    default:
      break;
  }
}

/**
   @brief run inference on the static sized_image buffer using the provided impulse
*/
static void ei_wildlife_camera_classify(bool debug) {
  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data = &ei_camera_cutout_get_data;

  EI_IMPULSE_ERROR err = run_classifier(&signal, &ei_result, DEBUG_NN);
  if (err != EI_IMPULSE_OK) {
    ei_printf("ERROR: Failed to run classifier (%d)\n", err);
    return;
  }
  // print the predictions
  if (debug) {
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
              ei_result.timing.dsp, ei_result.timing.classification, ei_result.timing.anomaly);
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    bool bb_found = ei_result.bounding_boxes[0].value > 0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; ix++) {
      auto bb = ei_result.bounding_boxes[ix];
      if (bb.value == 0) {
        continue;
      }

      ei_printf("    %s (", bb.label);
      ei_printf_float(bb.value);
      ei_printf(") [ x: %u, y: %u, width: %u, height: %u ]\n", bb.x, bb.y, bb.width, bb.height);
    }

    if (!bb_found) {
      ei_printf("    No objects found\n");
    }
#else
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      ei_printf("    %s: ", ei_result.classification[ix].label);
      ei_printf_float(ei_result.classification[ix].value);
      ei_printf("\n");
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: ");
    ei_printf_float(ei_result.anomaly);
    ei_printf("\n");
#endif
#endif
  }

  return;
}

/**
 * @brief callback that checks for the presence of an animal in the camera preview window, and then 
 *   executes ei_wildlife_camera_snapshot() if found
 */
void CamCB(CamImage img)
{
  if (!img.isAvailable()) return; // fast path if image is no longer ready
  CamErr err;
  Serial.println("INFO: new frame processing...");
  // resize and convert image to grayscale to prepare for inferencing
  err = img.clipAndResizeImageByHW(sized_img
                                   , OFFSET_X, OFFSET_Y
                                   , OFFSET_X + CLIP_WIDTH - 1
                                   , OFFSET_Y + CLIP_HEIGHT - 1
                                   , EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
  if (err) printError(err);

  err = sized_img.convertPixFormat(CAM_IMAGE_PIX_FMT_GRAY);
  if (err) printError(err);

  // get inference results on resized grayscale image
  ei_wildlife_camera_classify(true);

  if (ei_result.classification[CLASSIFIER_ANIMAL_INDEX].value >= CLASSIFIER_THRESHOLD) {
    Serial.println("INFO: Animal detected!");
    ei_wildlife_camera_snapshot(true);
    // if an animal snapshot is captured, pause to check for connectivity and GNSS prior to taking followup photos
    err = theCamera.startStreaming(false, CamCB);
  }
}

/**
   @brief initialize the wildlife camera for continuous monitoring of video feed
*/
void ei_wildlife_camera_start_continuous(bool debug) {
  CamErr err;

  err = theCamera.begin(1, CAM_VIDEO_FPS_5, RAW_WIDTH, RAW_HEIGHT);
  if (err && debug) printError(err);

  // start streaming the preview images to the classifier
  err = theCamera.startStreaming(true, CamCB);
  if (err && debug) printError(err);
    
  // still image format must be jpeg to allow for compressed storage/transmit
  err = theCamera.setStillPictureImageFormat(
    RAW_WIDTH,
    RAW_HEIGHT,
    CAM_IMAGE_PIX_FMT_JPG);
  if (err && debug) printError(err);

  if (debug) Serial.println("INFO: started wildlife camera recording");
}

/**
   @brief connect to cellular network for communication
*/
void ei_wildlife_camera_connect_cellular(bool debug) {
  if ((lteAccess.begin() != LTE_SEARCHING) && debug) {
      Serial.println("ERROR: Could not start LTE modem to LTE_SEARCHING.");
      Serial.println("Please check the status of the LTE board.");
  }
  if (!(lteAccess.attach(APP_LTE_RAT,
                         APP_LTE_APN,
                         APP_LTE_USER_NAME,
                         APP_LTE_PASSWORD,
                         APP_LTE_AUTH_TYPE,
                         APP_LTE_IP_TYPE) == LTE_READY) && debug) {
      Serial.println("ERROR: Failed to attach to LTE network");
      Serial.println("Check SIM card, APN settings, and coverage in current area");
  }

  if (debug) Serial.println("INFO: attempted LTE modem startup");
}

/**
   @brief take a jpeg picture, save to SD card, and transmit over 4G LTE if available
*/
void ei_wildlife_camera_snapshot(bool debug)
{
  char filename[400];

  // snapshot and save a jpeg
  CamImage img = theCamera.takePicture();
  if (theSD.begin() && img.isAvailable()) {
    sprintf(filename, "ANIMAL.%s.jpg", sprintNavData());
    if (debug) ei_printf("INFO: saving %s to SD card...", filename);
    theSD.remove(filename);
    File myFile = theSD.open(filename, FILE_WRITE);
    myFile.write(img.getImgBuff(), img.getImgSize());
    myFile.close();
  } else if (debug) {
    Serial.println("failed to compress and save image, check that camera and SD card are connected properly");
  }
  
  // if connected, transmit using 4G LTE
  if (client.connect(server, port)) {
    if (debug) Serial.println("INFO: connected to network, attempting to transmit image....");
    // Make a HTTP request:
    client.print("POST ");
    client.print(path);
    client.print(" HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(server);
    client.print("\r\n");
    client.print("User-Agent: curl/7.79.1\r\n");
    client.print("Accept: */*\r\n");
    client.print("x-api-key: ");
    client.print(EI_API_KEY);
    client.print("\r\n");
    client.print("Content-Length: ");
    client.print(186 + img.getImgSize(), DEC); // base content is 186 bytes + image jpg bytes
    client.print("\r\n"); // base content is 186 bytes + image jpg bytes

    client.print("Content-Type: multipart/form-data; boundary=------------------------50fe370e0890b48f\r\n");
    client.print("--------------------------50fe370e0890b48f\r\n");
    client.print("Content-Disposition: form-data; name=\"data\"; filename=\"animal.jpg\"\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("\r\n");
    for (int i = 0; i < img.getImgSize(); i++) {
      client.write(img.getImgBuff()[i]);
    }
    client.print("\r\n--------------------------50fe370e0890b48f--\r\n");
  }

  // if there are incoming bytes available
  // from the server, read them and print them:
  if (int len = client.available() && debug) {
    char buff[len + 1];
    buff[len] = '\0';
    client.read((uint8_t*)buff, len);
    Serial.print(buff);
  }
}
