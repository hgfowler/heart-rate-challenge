# Heart Rate Video Challenge

This project estimates a person's heart rate from `codingtest.mov` using OpenCV and C++.

The approach averages the green channel over a manually selected forehead region, then uses a Fourier transform to find the strongest frequency peak within a heart-rate range.

## Requirements

- CMake
- OpenCV
- C++17 compiler

## How to Run

First, open Terminal and move into the project folder:

```bash
cd path/to/HeartRateChallenge
```

For example:

```bash
cd ~/Desktop/HeartRateChallenge
```

Place `codingtest.mov` in the project root before running

### Option 1: Run with Makefile

This is the easiest option:

```bash
make run
```

This will configure, build, and run the program using `codingtest.mov`

### Option 2: Run manually with CMake

```bash
mkdir build
cmake ..
make
./heart_rate ../codingtest.mov
```

The program should print the estimated heart rate in BPM.

## Debugging Options

There are two `constexpr` flags near the top of `main.cpp` that can be changed while testing:

```cpp
constexpr bool showRoiDebug = false;
constexpr bool showDiagnosticOutput = false;
```

Set `showRoiDebug` to `true` to display the video with a green rectangle around the selected forehead region.

Set `showDiagnosticOutput` to `true` to print extra information such as FPS, frame count, video duration, ROI coordinates, BPM resolution, and the top FFT peaks.

## Results

Estimated heart rate: 61.44 BPM