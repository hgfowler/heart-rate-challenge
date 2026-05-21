#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

// change to true for debugging and seeing more data
constexpr bool showRoiDebug = false;
constexpr bool showDiagnosticOutput = false;

struct FrequencyPower {
    double frequencyHz;
    double bpm;
    double power;
};

struct GreenSignalResult {
    std::vector<double> signal;
    double fps;
    cv::Rect roi;
};

double computeMean(const std::vector<double>& values) {
    if (values.empty()) {
        throw std::runtime_error("Cannot compute mean of empty signal.");
    }

    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / static_cast<double>(values.size());
}

double computeStdDev(const std::vector<double>& values, double mean) {
    if (values.size() < 2) {
        throw std::runtime_error("Signal is too short to compute standard deviation.");
    }

    double variance = 0.0;

    for (double value : values) {
        double diff = value - mean;
        variance += diff * diff;
    }

    variance /= static_cast<double>(values.size() - 1);
    return std::sqrt(variance);
}

std::vector<double> normalizeSignal(const std::vector<double>& signal) {
    double mean = computeMean(signal);
    double stdDev = computeStdDev(signal, mean);

    if (stdDev < 1e-8) {
        throw std::runtime_error("Signal has near-zero variance.");
    }

    std::vector<double> normalized;
    normalized.reserve(signal.size());

    for (double value : signal) {
        normalized.push_back((value - mean) / stdDev);
    }

    return normalized;
}

void applyHannWindow(std::vector<double>& signal) {
    const int n = static_cast<int>(signal.size());

    if (n <= 1) {
        return;
    }

    for (int i = 0; i < n; ++i) {
        double windowValue = 0.5 * (1.0 - std::cos((2.0 * CV_PI * i) / (n - 1)));
        signal[i] *= windowValue;
    }
}

cv::Rect clampRectToFrame(const cv::Rect& rect, const cv::Size& frameSize) {
    int x = std::max(0, rect.x);
    int y = std::max(0, rect.y);

    int width = std::min(rect.width, frameSize.width - x);
    int height = std::min(rect.height, frameSize.height - y);

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("ROI is outside the frame.");
    }

    return cv::Rect(x, y, width, height);
}

double averageGreenChannel(const cv::Mat& frame, const cv::Rect& roi) {
    cv::Mat forehead = frame(roi);

    cv::Scalar meanBGR = cv::mean(forehead);

    // OpenCV uses BGR instead of RGB
    return meanBGR[1];
}

GreenSignalResult extractGreenSignal(const std::string& videoPath) {
    cv::VideoCapture capture(videoPath);

    if (!capture.isOpened()) {
        throw std::runtime_error("Could not open video: " + videoPath);
    }

    double fps = capture.get(cv::CAP_PROP_FPS);

    if (fps <= 0.0) {
        throw std::runtime_error("Invalid FPS read from video.");
    }

    std::vector<double> greenSignal;
    cv::Rect usedRoi;
    cv::Mat frame;
    bool roiInitialized = false;

    while (capture.read(frame)) {
        if (frame.empty()) {
            break;
        }

        if (!roiInitialized) {
            int frameWidth = frame.cols;
            int frameHeight = frame.rows;

            // Fixed forehead ROI for challenge simplicity
            // Would need to be adjusted for different videos
            // Stay on the central forehead, avoiding any hair or background
            cv::Rect approximateForehead(
                static_cast<int>(frameWidth * 0.38),
                static_cast<int>(frameHeight * 0.21),
                static_cast<int>(frameWidth * 0.10),
                static_cast<int>(frameHeight * 0.1)
            );

            usedRoi = clampRectToFrame(approximateForehead, frame.size());
            roiInitialized = true;
        }

        double greenMean = averageGreenChannel(frame, usedRoi);
        greenSignal.push_back(greenMean);

        // DEBUGGING - view forehead section selection
        if (showRoiDebug) {
            cv::Mat debugFrame = frame.clone();

            cv::rectangle(
                debugFrame,
                usedRoi,
                cv::Scalar(0, 255, 0),
                2
            );

            cv::imshow("Forehead ROI Debug", debugFrame);

            int key = cv::waitKey(1);
            if (key == 27) { // Esc key
                break;
            }
        }
    }

    // Basic sanity check. A longer video is ideal
    if (greenSignal.size() < 30) {
        throw std::runtime_error("Not enough frames to estimate heart rate.");
    }

    return GreenSignalResult{greenSignal, fps, usedRoi};
}

std::vector<FrequencyPower> computePowerSpectrum(const std::vector<double>& signal,
                                                    double fps) {
    int originalSize = static_cast<int>(signal.size());
    int dftSize = cv::getOptimalDFTSize(originalSize);

    cv::Mat padded = cv::Mat::zeros(dftSize, 1, CV_64F);

    for (int i = 0; i < originalSize; ++i) {
        padded.at<double>(i, 0) = signal[i];
    }

    cv::Mat complexSpectrum;
    cv::dft(padded, complexSpectrum, cv::DFT_COMPLEX_OUTPUT);

    std::vector<FrequencyPower> spectrum;

    for (int k = 1; k <= dftSize / 2; ++k) {
        cv::Vec2d value = complexSpectrum.at<cv::Vec2d>(k, 0);

        double real = value[0];
        double imag = value[1];

        double power = real * real + imag * imag;
        double frequencyHz = static_cast<double>(k) * fps / static_cast<double>(dftSize);
        double bpm = frequencyHz * 60.0;

        spectrum.push_back({frequencyHz, bpm, power});
    }

    return spectrum;
}

FrequencyPower findStrongestHeartRatePeak(const std::vector<FrequencyPower>& spectrum,
                                            double minBpm, double maxBpm) {
    FrequencyPower bestPeak{0.0, 0.0, -1.0};

    for (const FrequencyPower& bin : spectrum) {
        if (bin.bpm < minBpm || bin.bpm > maxBpm) {
            continue;
        }

        if (bin.power > bestPeak.power) {
            bestPeak = bin;
        }
    }

    if (bestPeak.power < 0.0) {
        throw std::runtime_error("No valid peak found in heart rate range.");
    }

    return bestPeak;
}

void printDiagnostics(const std::string& videoPath, const GreenSignalResult& result,
                        double durationSeconds, double bpmResolution) {
    std::cout << "Video: " << videoPath << '\n';
    std::cout << "FPS: " << result.fps << '\n';
    std::cout << "Frames processed: " << result.signal.size() << '\n';
    std::cout << "Duration: " << durationSeconds << " seconds\n";
    std::cout << "Approx BPM resolution: " << bpmResolution << " BPM\n";

    std::cout
        << "Forehead ROI: x=" << result.roi.x
        << ", y=" << result.roi.y
        << ", width=" << result.roi.width
        << ", height=" << result.roi.height
        << '\n';
}

void printTopPeaks(const std::vector<FrequencyPower>& spectrum, double minBpm,
                    double maxBpm, int count) {
    std::vector<FrequencyPower> candidates;

    for (const FrequencyPower& bin : spectrum) {
        if (bin.bpm >= minBpm && bin.bpm <= maxBpm) {
            candidates.push_back(bin);
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const FrequencyPower& a, const FrequencyPower& b) {
            return a.power > b.power;
        }
    );

    int peaksToPrint = std::min(count, static_cast<int>(candidates.size()));

    std::cout << "\nTop peaks in heart-rate range:\n";

    for (int i = 0; i < peaksToPrint; ++i) {
        std::cout
            << i + 1
            << ". BPM: " << candidates[i].bpm
            << ", Frequency: " << candidates[i].frequencyHz << " Hz"
            << ", Power: " << candidates[i].power
            << '\n';
    }
}

int main(int argc, char** argv) {
    try {
        std::string videoPath = "codingtest.mov";

        if (argc >= 2) {
            videoPath = argv[1];
        }

        GreenSignalResult greenSignalResult = extractGreenSignal(videoPath);

        std::vector<double> processedSignal = normalizeSignal(greenSignalResult.signal);
        applyHannWindow(processedSignal);

        std::vector<FrequencyPower> spectrum = computePowerSpectrum(processedSignal, greenSignalResult.fps);

        constexpr double minBpm = 45.0;
        constexpr double maxBpm = 180.0;

        FrequencyPower bestPeak = findStrongestHeartRatePeak(spectrum, minBpm, maxBpm);

        double durationSeconds = static_cast<double>(greenSignalResult.signal.size()) / greenSignalResult.fps;
        double bpmResolution = 60.0 / durationSeconds;

        if (showDiagnosticOutput) {
            printDiagnostics(videoPath, greenSignalResult, durationSeconds, bpmResolution);

            printTopPeaks(spectrum, minBpm, maxBpm, 5);
        }

        std::cout << "\nEstimated heart rate: " << bestPeak.bpm << " BPM\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}