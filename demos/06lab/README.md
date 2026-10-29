# LAB: Video

Create new project similar to camera processing.

## Task 1: Explore the source code for lossy compression

The (partially implemented) program requires a camera. It produces continuous stream of compressed JPEG images (this is similar to MJPEG compression). Each frame must  fit within the required size ratio (compared to the original image). Use 'q' or 'a' key to increase or decrease available bandwidth. Use ESC key to terminate.

## Task 2: Get basic info

- measure time needed to compress single image
- measure time needed to decompress single image
- compare times: what is the assymetry ratio of the JPEG compression?

## Task 3: Modify the code, so that it use limitation by bandwith in mbit/sec

Allow user to change mbit limit by key press (use callback). This will simulate limited network speed. Observe the final output quality.

## Task 4: Keep it realtime

- Try to guess correct JPEG quality based on history (value from previous frame or average of few frames).
- Use multiple threads, for example:
  - main + grabber/encoder/decoder
  - main + grabber/encoder + decoder
  - main + grabber + encoder + decoder
  - encoder itself can be multithreaded...
  - thread pool...

Try to achieve realtime compression (time to compress single image is lower than 1/FPS of the camera).
