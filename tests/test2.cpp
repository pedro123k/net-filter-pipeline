#include <nfp/DigitalFilter.hpp>
#include <nfp/SignalPipeline.hpp>
#include <memory>
#include <iostream>
#include <math.h>
#include <vector>

using namespace std;

const float PI = 3.14159;

int main(int argc, char ** argv) {
    float f0 = 2000.0f;
    float f1 = 1000.0f;
    float fs = 10000.0f;

    float w0 = 2 * PI * (f0 / fs);
    float w1 = 2 * PI * (f1 / fs);

    vector<float> input;
    vector<float> output;

    for (int i = 0; i < 1000; i++) {
        float t = static_cast<float>(i) / fs;
        input.push_back(sin(2 * PI * 100 * t));
    }

    nfp::SignalPipeline pipeline;
    pipeline.add_gain(4);
    pipeline.add_digital_filter(nfp::DigitalFilter::high_pass_filter(w1, 6, sqrt(2) / 2));
    pipeline.add_digital_filter(nfp::DigitalFilter::low_pass_filter(w0, 6, sqrt(2) / 2));
    pipeline.processBlock(input, output);

    for (auto & c : pipeline.coeffs())
        cout << c << endl;
        
}