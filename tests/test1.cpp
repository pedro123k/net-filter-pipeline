#include <nfp/DigitalFilter.hpp>
#include <memory>
#include <iostream>
#include <math.h>
#include <vector>

using namespace std;

const float PI = 3.14159;

int main(int argc, char ** argv) {
    float f0 = 500.0f;
    float fs = 10000.0f;

    float w0 = 2 * PI * (f0 / fs);

    vector<float> input;
    vector<float> output;

    for (int i = 0; i < 1000; i++) {
        float t = static_cast<float>(i) / fs;
        input.push_back(sin(2 * PI * 600 * t));
    }

    nfp::DigitalFilter filter = nfp::DigitalFilter::band_pass_filter(w0, 1);
    filter.processBlock(input, output);

    for (auto i = 0; i < 100; i++)
        cout << input[i] << ',';
    
    cout << endl;

    for (auto i = 0; i < 100; i++)
        cout << output[i] << ',';
}