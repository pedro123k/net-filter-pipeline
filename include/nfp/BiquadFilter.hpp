#pragma once

#include <vector>
#include <array>

namespace nfp {

    class BiquadFilter {
    private:
        std::array<float, 3> a;
        std::array<float, 3> b;
        std::array<float, 5> norm_coeffs;
        std::vector<float> vs;
        float Q;

    public:
        BiquadFilter(float, float, float, float, float, float);

        static BiquadFilter lpf(float, float=0.707);
        static BiquadFilter hpf(float, float=0.707);
        static BiquadFilter bpf(float, float);
        static BiquadFilter notch(float, float);

        std::array<float, 6> coeffs();
        float eval(float);
        void processBlock(const std::vector<float> &, std::vector<float> &);
        void reset();
        constexpr float get_Q() const {return this->Q;}
    };

}