#include "JpegEncoder.h"

#include <chrono>
#include <vector>

#include <opencv2/imgcodecs.hpp>

namespace jpeg {

EncodedCandidate encode_at_quality(const cv::Mat& image, int quality)
{
    EncodedCandidate candidate;
    candidate.quality = quality;

    const std::vector<int> compression_params {
        cv::IMWRITE_JPEG_QUALITY,
        quality
    };

    const auto encode_start = std::chrono::steady_clock::now();
    cv::imencode(".jpg", image, candidate.bytes, compression_params);
    const auto encode_end = std::chrono::steady_clock::now();

    candidate.encode_ms =
        std::chrono::duration<double, std::milli>(encode_end - encode_start).count();
    return candidate;
}

} // namespace jpeg
