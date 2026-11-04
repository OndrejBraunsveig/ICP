#pragma once

#include <vector>

#include <opencv2/core.hpp>

namespace jpeg {

struct EncodedCandidate {
    std::vector<uchar> bytes;
    double encode_ms = 0.0;
    int quality = 0;
};

EncodedCandidate encode_at_quality(const cv::Mat& image, int quality);

} // namespace jpeg
