/*
* BSD 3-Clause License
*
* Copyright (c) 2018, Ifsttar
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
*  Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
*  Neither the name of the copyright holder nor the names of its
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

package org.noise_planet.jwarble;

public class OpenWarble {

    public static final double M2PI = Math.PI * 2;
    private Configuration configuration;
    public final static int NUM_FREQUENCIES = 12;
    final double[] frequencies = new double[NUM_FREQUENCIES];
    final int block_length;
    final int word_length;
    final int chirp_length;
    final int messageSamples;

    public OpenWarble(Configuration configuration) {
        this.configuration = configuration;
        block_length = configuration.payloadSize;
        word_length = (int)(configuration.sampleRate * configuration.wordTime);
        chirp_length = word_length;
        messageSamples = chirp_length + block_length * word_length;
        // Precompute pitch frequencies
        for(int i = 0; i < NUM_FREQUENCIES; i++) {
            if(configuration.frequencyIncrement != 0) {
                frequencies[i] = configuration.firstFrequency + i * configuration.frequencyIncrement;
            } else {
                frequencies[i] = configuration.firstFrequency * Math.pow(configuration.frequencyMulti, i * 2);
            }
        }
    }

    private MessageCallback callback = null;

    /**
     * Goertzel algorithm - Compute the RMS power of the selected frequencies for the provided audio signals.
     * http://asp.eurasipjournals.com/content/pdf/1687-6180-2012-56.pdf
     * ipfs://QmdAMfyq71Fm72Rt5u1qtWM7teReGAHmceAtDN5SG4Pt22
     * Sysel and Rajmic:Goertzel algorithm generalized to non-integer multiples of fundamental frequency. EURASIP Journal on Advances in Signal Processing 2012 2012:56.
     * @param signal Audio signal
     * @param sampleRate Sampling rate in Hz
     * @param freqs Array of frequency search in Hz
     * @return rms Rms power by frequencies
     */
    public static double[] generalized_goertzel(final double[] signal,double sampleRate,final double[] freqs) {
        double[] outFreqsPower = new double[freqs.length];
        // Fix frequency using the sampleRate of the signal
        double samplingRateFactor = signal.length / sampleRate;
        // Computation via second-order system
        for(int id_freq = 0; id_freq < freqs.length; id_freq++) {
            // for a single frequency :
            // precompute the constants
            double pik_term = M2PI * (freqs[id_freq] * samplingRateFactor) / (signal.length);
            double cos_pik_term2 = Math.cos(pik_term) * 2;

            Complex cc = new Complex(pik_term, 0).exp();
            // state variables
            double s0;
            double s1 = 0.;
            double s2 = 0.;
            // 'main' loop
            // number of iterations is (by one) less than the length of signal
            for(int ind=0; ind < signal.length - 1; ind++) {
                s0 = signal[ind] + cos_pik_term2 * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            // final computations
            s0 = signal[signal.length - 1] + cos_pik_term2 * s1 - s2;

            // complex multiplication substituting the last iteration
            // and correcting the phase for (potentially) non - integer valued
            // frequencies at the same time
            Complex parta = new Complex(s0, 0).sub(new Complex(s1, 0).mul(cc));
            Complex partb = new Complex(pik_term * (signal.length - 1.), 0).exp();
            Complex y = parta.mul(partb);
            outFreqsPower[id_freq] = Math.sqrt((y.r * y.r  + y.i * y.i) * 2) / signal.length;

        }
        return outFreqsPower;
    }

    public static double compute_rms(double[] signal) {
        double sum = 0;
        for (double aSignal : signal) {
            sum += aSignal * aSignal;
        }
        return Math.sqrt(sum / signal.length);
    }

    public void pushSamples(double[] samples) {

    }

    public static void generate_pitch(double[] signal_out, final int location,final int length, double sample_rate, double frequency, double power_peak) {
        double t_step = 1 / sample_rate;
        for(int i=location; i < location + length; i++) {
		    final double window = 0.5 * (1 - Math.cos((M2PI * (i - location)) / (length - 1)));
            signal_out[i] += Math.sin(i * t_step * M2PI * frequency) * power_peak * window;
        }
    }

    public static void generate_chirp(double[] signal_out, final int location,final int length, double sample_rate, double frequencyStart, double frequencyEnd, double power_peak) {
        double f1_div_f0 = frequencyEnd / frequencyStart;
        double chirp_time = length / sample_rate;
        for (int i = location; i < location + length; i++) {
            // Generate log chirp into cache
            // low frequency to high frequency
            final double window = 0.5 * (1 - Math.cos((M2PI * (i - location)) / (length - 1)));
            signal_out[i] += window * Math.sin(M2PI * chirp_time / Math.log(f1_div_f0) * frequencyStart * (Math.pow(f1_div_f0, ((double) (i - location) / sample_rate) / chirp_time) - 1.0));
        }
    }

    public double[] generate_signal(double powerPeak, byte[] words) {
        double[] signal = new double[messageSamples];
        int location = 0;
        generate_chirp(signal, 0, chirp_length, configuration.sampleRate, configuration.firstFrequency, frequencies[frequencies.length - 1], powerPeak);
        location += chirp_length;
        for(int idword = 0; idword < block_length; idword++) {
            int code = Hamming12_8.encode(words[idword]);
            for(int idfreq = 0; idfreq < frequencies.length; idfreq++) {
                if((code & (1 << idfreq)) != 0) {
                    generate_pitch(signal, location, word_length ,configuration.sampleRate, frequencies[idfreq], powerPeak);
                }
            }
            location+=word_length;
        }
        return signal;
    }

    public MessageCallback getCallback() {
        return callback;
    }

    public void setCallback(MessageCallback callback) {
        this.callback = callback;
    }

    private static final class Complex {
        public final double r;
        public final double i;

        public Complex(double r, double i) {
            this.r = r;
            this.i = i;
        }

        Complex add(Complex c2) {
            return new Complex(r + c2.r, i + c2.i);
        }

        Complex sub(Complex c2) {
            return new Complex(r - c2.r, i - c2.i);
        }

        Complex mul(Complex c2) {
            return new Complex(r * c2.r - i * c2.i, r * c2.i + i * c2.r);
        }

        Complex exp() {
            return new Complex(Math.cos(r), -Math.sin(r));
        }
    }
}
