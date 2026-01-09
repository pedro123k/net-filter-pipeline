#include <nfp/BiquadFilter.hpp>
#include <math.h>

using nfp::BiquadFilter;
using std::vector;
using std::array;

BiquadFilter::BiquadFilter(float a0, float a1, float a2, float b0, float b1, float b2) {
    this->a = {a0, a1, a2};
    this->b = {b0, b1, b2};
    this->vs = {0.0f, 0.0f};
    this->norm_coeffs = {a1/a0, a2/a0, b0/a0, b1/a0, b2/a0};
    this->Q = 0;
}


BiquadFilter BiquadFilter::lpf(float w0, float Q) {
    float alpha = sin(w0) / (2 * Q);

    float a0 = 1 + alpha;
    float a1 = -2 * cos(w0);
    float a2 = 1 - alpha;

    float b1 = (1 - cos(w0));
    float b0 = b1 / 2;
    float b2 = b0;

    auto ret = BiquadFilter {a0, a1, a2, b0, b1, b2};
    ret.Q = Q;
    
    return ret;
}

BiquadFilter BiquadFilter::hpf(float w0, float Q) {
    float alpha = sin(w0) / (2 * Q);

    float a0 = 1 + alpha;
    float a1 = -2 * cos(w0);
    float a2 = 1 - alpha;

    float b1 = -(1 + cos(w0));
    float b0 = -b1 / 2;
    float b2 = b0;

    auto ret = BiquadFilter(a0, a1, a2, b0, b1, b2);
    ret.Q = Q;
    return ret;
}

BiquadFilter BiquadFilter::bpf(float w0, float BW) {
    float alpha = sin(w0) * sinh(log(2) * BW * w0 / (2 * sin(w0)));

    float a0 = 1 + alpha;
    float a1 = -2 * cos(w0);
    float a2 = 1 - alpha;

    float b0 = alpha;
    float b1 = 0;
    float b2 = -alpha;
    
    return BiquadFilter(a0, a1, a2, b0, b1, b2);
}

BiquadFilter BiquadFilter::notch(float w0, float BW) {
    float alpha = sin(w0) * sinh(log(2) * BW * w0 / (2 * sin(w0)));

    float a0 = 1 + alpha;
    float a1 = -2 * cos(w0);
    float a2 = 1 - alpha;

    float b0 = 1;
    float b1 = -2 * cos(w0);
    float b2 = 1;

    return BiquadFilter(a0, a1, a2, b0, b1, b2);
}

array<float, 6> BiquadFilter::coeffs() {
    return array<float, 6> {this->a[0], this->a[1], this->a[2], this->b[0], this->b[1], this->b[2]};
}

float BiquadFilter::eval(float x) {
    float _a1 = this->norm_coeffs[0];
    float _a2 = this->norm_coeffs[1];
    float _b0 = this->norm_coeffs[2];
    float _b1 = this->norm_coeffs[3];
    float _b2 = this->norm_coeffs[4];

    float v = x - _a1 * this->vs[0] - _a2 * this->vs[1];
    float y =  _b0 * v + _b1 * this->vs[0] + _b2 * this->vs[1];

    this->vs[1] = this->vs[0];
    this->vs[0] = v;

    return y;
}

void BiquadFilter::processBlock(const vector<float> & input, vector<float> & output) {
    output.clear();
    output.reserve(input.size());

    for (float x : input) {
        float y = this->eval(x);
        output.push_back(y);
    }

}

void BiquadFilter::reset() {
    this->vs = {0.0f, 0.0f};
}
