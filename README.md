# 4G LTE Connected Smart Wildlife Camera

![](https://static.developer.sony.com/images/image/v6/s3/uploads/2018/03/Spresense-main-LTE-HDR-camera.jpg?size=1920xAUTO&v=817703&jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJmb2N1cyI6eyJ4IjowLjUsInkiOjAuNX0sInNyYyI6Ii91cGxvYWRzLzIwMTgvMDMvU3ByZXNlbnNlLW1haW4tTFRFLUhEUi1jYW1lcmEuanBnIiwiYWx0X3RleHQiOiJTcHJlc2Vuc2UgTWFpbiBMVEUgSERSIENhbWVyYSIsImltYWdlX2lkIjo4MTc3MDMsInNlcnZpY2UiOiJhd3MtczMiLCJhd3NfYnVja2V0Ijoic29ueWR3LXByb2Qtc3RhdGljLWFzc2V0cyIsImhlaWdodF9hc3BlY3RfcmF0aW8iOjAuNjQ5MDMzNTcwNzAxOTMyOSwib3JpZ2luYWxfaGVpZ2h0Ijo2MzgsIm9yaWdpbmFsX3dpZHRoIjo5ODMsIndoaXRlbGlzdFNpemVzIjpbIjEyMDB4QVVUTyIsIjEwMHg2NSIsIjEwMHhBVVRPIiwiMTEweDgyIiwiNzUweEFVVE8iLCIxMDAweEFVVE8iLCIxMjAweEFVVE8iLCIxMjAweDgwMCIsIjE5MjB4QVVUTyIsIjE5MjB4MTAwMCIsIjIwMHgxNTAiLCIyMjV4MTcwIiwiMjI1eEFVVE8iLCIyMjl4MzAwIiwiMzY4eEFVVE8iLCI0NTB4MzQwIiwiNTAweEFVVE8iLCI3NTB4NTAwIiwiN3gxMCIsIjd4MTIiLCI3eDE2IiwiN3gzIiwiN3g0IiwiN3g1IiwiN3g2IiwiN3g3IiwiN3g4IiwiN3g5Il0sImltYWdlVmVyc2lvbiI6InY2IiwiZXhwIjoxOTA0Njg4MDAwMDAwfQ.kCmu7hnLI-6z-nj2J8MTRi9p6AOaS-WjC0I1KUd4UkQ)

This repository provides an example application using Edge Impulse and the Sony Spresense to create an always connected smart camera designed for capturing wildlife photos in remote areas

## Features
* GPS positioning and metadata capture
* On-device animal classification algorithm, only save photos of animals
* Local storage, automatically saves wildlife photos to SD card with GPS data
* 4G LTE connectivity, automatically, if able, transmits wildlife photos to Edge Impulse projects for advanced data collection, continuous learning algorithms, or species level classification

## Hardware Requirements
* [Sony Spresense 4G LTE board](https://developer.sony.com/develop/spresense/spresense-lte/)
* [Sony Spresense main board](https://developer.sony.com/develop/spresense/)
* Sony Spresense [5MP camera](https://developer.sony.com/develop/spresense/docs/introduction_en.html#_camera_board) or [HDR camera](https://developer.sony.com/develop/spresense/docs/introduction_en.html#_hdr_camera_board) board
* SD Card
* A Spresense supported [SIM Card](https://developer.sony.com/develop/spresense/specifications/spresense-lte-sim-list/), e.g. [Soracom Global IoT ecoSIM](https://www.soracom.io/products/iot-sim/)

## Setup
1. Follow the [Spresense Board getting started guide](https://developer.sony.com/develop/spresense/docs/introduction_en.html#_how_to_use_spresense_board), connecting an SD card, SIM card, camera board, and 4G LTE base board together, and connecting the device to your computer
2. Create an [Edge Impulse](www.edgeimpulse.com) account
3. Install the [Arduino IDE](https://www.arduino.cc/en/software)
4. Install the [Spresense Arduino Library](https://developer.sony.com/develop/spresense/docs/arduino_set_up_en.html#_install_spresense_arduino_library)
5. *IMPORTANT* In `Tools->Board` select the `Spresense` device, then under `Tools->Memory` select `1536(kB)`. We do not need the audio or multi-core memory layouts for this project, but we do need as much memory available as possible for NN and image storage

## Using this project
1. Clone this repository locally
2. Clone the [Wildlife Camera: On Device - North America](https://studio.edgeimpulse.com/public/105811/latest) Edge Impulse project 
3. Navigate to the Deployment tab, select `Arduino Library` and click `build`. Follow the instructions to add this library to your Arduino IDE
4. In the Arduino IDE, `File->Open` and then navigate to this repository, and open the `wildlife-camera-sketch/wildlife-camera-sketch.ino` file
5. Build the project to verify it compiles correctly
6. Now - user modifications begin by additionally cloning the [Wildlife Camera: Sprecies Classification - North America v2](https://studio.edgeimpulse.com/public/107393/latest) Edge Impulse Project
7. Navigate to `Dashboard -> Keys` in your project, and copy the API key into the `4g_camera.ino` tab of the Arduino sketch in the `EI_API_KEY` define
8. Also in the `4g_camera.ino` file, configure the APN defines depending on your sim card. The defaults are configured for `Soracom` sim devices
9. Now, rebuild and upload the sketch. Observe the `Serial Monitor` output, which should appear like so:

```
INFO: wildlife_camera initializing on wakeup...
INFO: gnss started
INFO: attempted LTE modem startup
INFO: started wildlife camera recording
gnss update failed, data:
2116-02-11_03-02-09_lat33.225112_long-87.581260
INFO: new frame processing...
Predictions (DSP: 2 ms., Classification: 717 ms., Anomaly: 0 ms.): 
    animal: 0.035156
    car: 0.007813
    empty: 0.019531
    person: 0.937500
INFO: new frame processing..
```

The device will now continuously stream data from the camera to the Edge Impulse classifier. 

Any images determined to contain an animal will be stored to the device with detailed GPS metadata, and additionally streamed over 4G to the species specific classifer project. This larger species specific classifier cannot fit on the spresense device, but can be used on streamed images via the Edge Impulse API.

Note that GNSS updates will likely fail if you are indoors, and you will need to move outside to receive positioning data.

Additionally if no 4G LTE connection is available, the device will just store data to the local SIM card for later retrival.

10. Test that the application works properly with a debug connector attached. Once confirmed, you can optionally rebuild with the define `DEBUG` set to `false` to save on UART RX/TX current consumption
