#pragma once
#include <nfp/DigitalFilter.hpp>
#include <vector>
#include <array>
#include <memory>

namespace nfp {
    class SignalPipeline {
    private:
        class PipelineElement {
        public:
            virtual float eval(float x) = 0;
            virtual void processBlock(const std::vector<float> &, std::vector<float>&);
            virtual std::vector<float> coeffs() const = 0;
            virtual ~PipelineElement() = default;
        };

        class GainElement : public PipelineElement {
        private:
            float gain;
        public:
            GainElement(float gain) : gain(gain) {}
            float eval(float x) override { return this->gain * x; }
            constexpr float get_gain() const { return this->gain; }
            std::vector<float> coeffs() const override { return std::vector<float> {1, 0, 0, this->gain, 0, 0}; }
        };

        class DigitalFilterElement : public PipelineElement {
            private:
                std::unique_ptr<nfp::DigitalFilter> filter;
            public:
                DigitalFilterElement(nfp::DigitalFilter f) : filter(std::make_unique<nfp::DigitalFilter>(std::move(f))) {}
                float eval(float x) override {return this->filter->eval(x); }
                std::vector<float> coeffs() const override { return this->filter->coeffs(); }
        };

        std::vector<std::unique_ptr<PipelineElement>> elements;
        
    public:
        void add_gain(float gain) { elements.push_back(std::make_unique<GainElement>(gain)); }
        void add_digital_filter(nfp::DigitalFilter f) { elements.push_back(std::make_unique<DigitalFilterElement>(std::move(f))); }

        float process(float x);
        void processBlock(const std::vector<float> &, std::vector<float> &);
        std::vector<float> coeffs() const;
        
    };
}