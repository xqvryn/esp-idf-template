# image_motion

`components/image_motion` is a portable helper with no ESP-IDF includes. `Detector` compares consecutive grayscale frames:

- Differences below `pixel_threshold` are ignored as noise.
- Optional brightness compensation subtracts the mean luma shift so a uniform exposure change is less likely to look like motion.
- Motion is reported when the changed-pixel ratio reaches `changed_permille` (parts per thousand).
- A second trigger inside `cooldown_ms` returns `cooldown`.
- The first frame only seeds the previous image. A size mismatch is `ignored`.

The camera, JPEG 1/8-scale decode, PSRAM ring, and HTTP gallery live in `examples/camera-timelapse`. Host tests are in `tests/host/test_image_motion.cpp`.
