# hiRecorder (High-Performance Intel Recorder)
hiRecorder is a screen capture tool that prioritizes speed and efficiency and is made exclusively for Intel 11th gen (Tiger Lake) graphics processors or higher, see [here](https://github.com/intel/libvpl#dispatcher-behavior-when-targeting-intel-gpus).
It utilizes Intel processor's video encoding capabilities through [Intel's Video Processing Library](https://github.com/intel/libvpl).

This recorder was first made to solve a specific issue. Most recording applications do not allow to encode video with 4:4:4 subsampling, but instead they opt for 4:2:0.
This is fine for most scenarios, but for screen capture it might not be enough. This tool solves it by utilizing a special range extension mode on HEVC encoder modules in the graphics card. 
This recorder is meant to capture high-quality video and audio that can later be used in a video editing software.

Features
-
* Fast high-quality screen and audio capture.
* ICQ Bitrate control
* Video and audio track seperation
* Crash-proof stream container (Matroska)
* Tray icon

Limitations
-
These features are not yet supported and might be added in future updates:
* Output file options
* Multiplexing options
* Multi-monitor support
* Audio quality options
* Advanced video and audio encoding and processing options
* Overlay and shapes
* Multiple video input support
* Multiple input and system audio device capture
* Streaming capability

If you want to see these features and more projects please show some support on my YouTube Channel: [EmilyOrSomething](https://www.youtube.com/@EmilyOrSomethingSomething).

License
-
This project is licensed under the MIT License. See the [LICENSE](/LICENSE.txt) file for details.

Legal Notice
-
Intel and Intel Video Processing Library (Intel VPL) are registered trademarks of Intel. hiRecorder is designed to integrate/work with Intel VPL, but is an independent tool and is not officially affiliated with, endorsed, or sponsored by Intel.
