//
// Created by hhh on 2023/5/18.
//

#ifndef TRITUM_M_FILTER_H
#define TRITUM_M_FILTER_H

class FirstOrder : public ScalarFilter {
public:
    FirstOrder() : wc_(0.0),
                   T_(0.0),
                   out_(0.0),
                   out_last_(0.0),
                   in_(0.0),
                   in_last_(0.0),
                   is_initialized_(false) {
    }

    /**
     *  @param[in] cutoff_freq          rad/sec
     *  @param[in] timestep             sec
     */
    FirstOrder(double cutoff_freq, double timestep) : FirstOrder() {
        initialize(cutoff_freq, timestep);
    }

    // pole must be initialized to use it
    void initialize(double cutoff_freq, double timestep) {
        assert(!is_initialized_ && "ERROR: FirstOrder was already initialized.");

        T_ = timestep;

        setCutoffFreq(cutoff_freq);

        out_ = 0.0;
        out_last_ = 0.0;
        in_ = 0.0;
        in_last_ = 0.0;
        is_initialized_ = true;
    }

    void offsetState(const double v) {
        out_ += v;
        out_last_ += v;
        in_ += v;
        in_last_ += v;
    }

    double step(double input) override {
        assert(is_initialized_ && "ERROR: FirstOrder is not initialized.");

        in_last_ = in_;
        in_ = input;
        out_last_ = out_;

        // see https://en.wikipedia.org/wiki/Bilinear_transform for details

        // if no prewarping, K = 2.0 / T_
        const double K = wc_ / std::tan(wc_ * T_ * 0.5);  // prewarping

        out_ = ((K - wc_) * out_last_ + wc_ * (in_ + in_last_)) / (K + wc_);

        return out_;
    }

    /** Set cutoff frequency (rad/sec). */
    void setCutoffFreq(const double cutoff_freq) {
        // In a DT system the highest frequency f_c that does not suffer from aliasing is
        // f_c < f_s/2, where f_s is the sampling frequency.
        // The case of tan(pi/2) corresponds to f_c = f_s/2.
        // Hence, we should not allow the filter to be initialized with anything, where
        // pole*timestep/2.0 >= pi/2
        assert(std::abs(cutoff_freq * T_) < M_PI &&
               "ERROR: cannot handle pole*timestep/2 >= PI/2");

        wc_ = cutoff_freq;
    }

    double getOutput() const {
        return out_;
    }

    double getCutoffFreq() const {
        return wc_;
    }

    double getTimestep() const {
        return T_;
    }

private:
    double wc_;  ///< cutoff frequency(rad/sec)
    double T_;   ///< timestep (sec)
    double out_;
    double out_last_;
    double in_;
    double in_last_;
    bool is_initialized_;
};
}

#endif //TRITUM_M_FILTER_H
