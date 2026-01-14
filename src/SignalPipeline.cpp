#include <nfp/SignalPipeline.hpp>

using std::vector;
using nfp::SignalPipeline;

void SignalPipeline::PipelineElement::processBlock(const vector<float> & input, vector<float> & output) {
    for (const auto & x : input)
        output.push_back(this->eval(x));
}

float SignalPipeline::process(float x) {
    float z = x;
    for (auto & element : this->elements)
        z = element->eval(z);
    return z;
}

void SignalPipeline::processBlock(const vector<float> & input, vector<float> & output) {
    for (const auto & x : input)
        output.push_back(this->process(x));
}

vector<float> SignalPipeline::coeffs() const {
    vector<float> as;

    for (const auto & element : this->elements) {
        auto cs = element->coeffs();
        as.insert(as.end(), cs.begin(), cs.end());
    }

    return as;
}