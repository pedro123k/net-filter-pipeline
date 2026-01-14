#include <nfp/DigitalFilter.hpp>
#include <array>
#include <math.h>
#include <algorithm>
#include <complex>

using nfp::DigitalFilter;
using nfp::BiquadFilter;
using std::vector;
using std::array;
using std::sort;
using std::complex;
using std::exp;

const int PI = 3.14159;


void DigitalFilter::normalize_ggain(DigitalFilter::Normalization norm) {
    float a0 = this->biquad_cascate.front().coeffs()[0];
    float a1 = this->biquad_cascate.front().coeffs()[1];
    float a2 = this->biquad_cascate.front().coeffs()[2];
    float b0 = this->biquad_cascate.front().coeffs()[3];
    float b1 = this->biquad_cascate.front().coeffs()[4];
    float b2 = this->biquad_cascate.front().coeffs()[5];

    float global_gain = 1.0f;

    for (auto f : this->biquad_cascate) {

        float f_b0 = f.coeffs()[3], f_b1 = f.coeffs()[4], f_b2 = f.coeffs()[5];
        float f_a0 = coeffs()[0], f_a1 = coeffs()[1], f_a2 = coeffs()[2];

        float filter_gain = 1.0f;

        if (norm == DigitalFilter::Normalization::DC) {
            float num = f_b0 + f_b1 + f_b2;
            float den = f_a0 + f_a1 + f_a2;
            filter_gain *= num / (den + 1e-8);
        } else if (norm == DigitalFilter::Normalization::FS) {
            float num = f_b0 - f_b1 + f_b2;
            float den = f_a0 - f_a1 + f_a2;
            filter_gain *= num / (den + 1e-8);
        } else {
            auto num = complex<float> {f_b0, 0.0} + f_b1 * exp(complex<float>{0.0, -this->w0}) + f_b2 * exp(complex<float>{0.0, -2 * this->w0});
            auto den = complex<float> {f_a0, 0.0} + f_a1 * exp(complex<float>{0.0, -this->w0}) + f_a2 * exp(complex<float>{0.0, -2 * this->w0});
            filter_gain *= abs(num / (den + complex<float>(1e-8f, 0)));
        }

        
        global_gain *= 1 / filter_gain;
    }

        b0 *= global_gain;
        b1 *= global_gain;
        b2 *= global_gain;

    this->biquad_cascate[0] = BiquadFilter(a0, a1, a2, b0, b1, b2);
}

float DigitalFilter::eval(float x) {
    float z = x;

    for (auto & f : this->biquad_cascate)
        z = f.eval(z);

    return z;
}

void DigitalFilter::reset() {
    for (auto & f : this->biquad_cascate) 
        f.reset();
}

void DigitalFilter::processBlock(const vector<float> & input, vector<float> & output) {
    output.clear();
    output.reserve(input.size());

    for (const auto & x : input)
        output.push_back(this->eval(x)); 

}

vector<float> DigitalFilter::coeffs() {
    vector<float> cs;
    cs.reserve(this->biquad_cascate.size() * 6);

    for (BiquadFilter f : this->biquad_cascate) {
        array<float, 6> f_coeffs = f.coeffs();
        cs.insert(cs.end(), f_coeffs.begin(), f_coeffs.end());
    }

    return cs;
}

DigitalFilter DigitalFilter::low_pass_filter(float w0, int ord, float Q) {
    DigitalFilter ret{w0, Q};
    
    if (ord==2)
        ret.biquad_cascate.push_back(BiquadFilter::lpf(w0, Q));

    else {
        int n = ord / 2;

        for (auto k = 0; k < n; k++) {
            float inv_Qk = 2 * cos((2*k + 1) * PI / (2 * ord));
            ret.biquad_cascate.push_back(BiquadFilter::lpf(w0, 1 / inv_Qk));
        }

        sort(ret.biquad_cascate.begin(), ret.biquad_cascate.end(), [](BiquadFilter &a, BiquadFilter &b){
            return a.get_Q() > b.get_Q();
        });

    }

    ret.normalize_ggain(DigitalFilter::Normalization::DC);
    return ret;
}

DigitalFilter DigitalFilter::high_pass_filter(float w0, int ord, float Q) {
    DigitalFilter ret{w0, Q};
    
    if (ord==2)
        ret.biquad_cascate.push_back(BiquadFilter::hpf(w0, Q));

    else {
        int n = ord / 2;

        for (auto k = 0; k < n; k++) {
            float inv_Qk = 2 * cos((2*k + 1) * PI / (2 * ord));
            ret.biquad_cascate.push_back(BiquadFilter::hpf(w0, 1 / inv_Qk));
        }

        sort(ret.biquad_cascate.begin(), ret.biquad_cascate.end(), [](BiquadFilter &a, BiquadFilter &b){
            return a.get_Q() > b.get_Q();
        });
    }

    ret.normalize_ggain(DigitalFilter::Normalization::FS);
    return ret;
}

DigitalFilter DigitalFilter::notch_filter(float w0, float BW) {
    DigitalFilter ret {w0, 0};
    ret.biquad_cascate.push_back(BiquadFilter::notch(w0, BW));
    ret.normalize_ggain(DigitalFilter::Normalization::DC);
    return ret;
}

DigitalFilter DigitalFilter::band_pass_filter(float w0, float BW) {
    DigitalFilter ret {w0, 0};
    ret.biquad_cascate.push_back(BiquadFilter::bpf(w0, BW));
    ret.normalize_ggain(DigitalFilter::Normalization::FC);
    return ret;
}