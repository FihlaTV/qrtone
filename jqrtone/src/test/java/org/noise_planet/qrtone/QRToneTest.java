package org.noise_planet.qrtone;

import org.junit.Test;

import java.util.Arrays;
import java.util.concurrent.atomic.AtomicLong;

import static org.junit.Assert.*;

public class QRToneTest {

    @Test
    public void crcTest() {
        byte[] expectedPayload = new byte[]{18, 32, -117, -93, -50, 2, 52, 26, -117, 93, 119, -109, 39, 46, 108, 4, 31, 36, -100, 95, -9, -70, -82, -93, -75, -32, -63, 42, -44, -100, 50, 83, -118, 114};
        byte base = QRTone.crc8(expectedPayload, 0, expectedPayload.length);
        AtomicLong next = new AtomicLong(1337);
        for(int i=0; i < expectedPayload.length; i++) {
            byte[] alteredPayload = Arrays.copyOf(expectedPayload, expectedPayload.length);
            alteredPayload[i] = (byte) (QRTone.warbleRand(next) % 255);
            assertNotEquals(base, QRTone.crc8(alteredPayload, 0, alteredPayload.length));
        }
    }


    @Test
    public void generalized_goertzel() throws Exception {
        double sampleRate = 44100;
        double powerRMS = 500; // 90 dBspl
        float signalFrequency = 1000;
        double powerPeak = powerRMS * Math.sqrt(2);

        float[] audio = new float[4410];
        for (int s = 0; s < audio.length; s++) {
            double t = s * (1 / sampleRate);
            audio[s] = (float)(Math.cos(QRTone.M2PI * signalFrequency * t) * (powerPeak));
        }

        double[] phase = new double[1];
        double[] rms = QRTone.generalizedGoertzel(audio,0, audio.length, sampleRate, new double[]{1000.0}, phase, false);

        double signal_rms = QRTone.computeRms(audio);

        assertEquals(signal_rms, rms[0], 0.1);
        assertEquals(0, phase[0], 1e-8);
    }


}