#pragma once

#include <vector>
#include <nfp/BiquadFilter.hpp>

namespace nfp {

    class DigitalFilter {
    private:
        float w0;
        float Q;
        std::vector<nfp::BiquadFilter> biquad_cascate;
        enum class Normalization {DC, FC, FS};
        void normalize_ggain(Normalization = Normalization::DC);
    public:
        DigitalFilter(float w0, float Q): w0(w0), Q(Q){}
        float eval(float);
        void reset();
        void processBlock(const std::vector<float> &, std::vector<float> &);
        std::vector<float> coeffs();

        static DigitalFilter low_pass_filter(float, int=2, float Q=0.707);
        static DigitalFilter high_pass_filter(float, int=2, float Q=0.707); 
        static DigitalFilter band_pass_filter(float, float);
        static DigitalFilter notch_filter(float, float);

        constexpr float get_Q() const {return this->Q;};
        constexpr float get_w0() const {return this->w0;}
    };
    
}