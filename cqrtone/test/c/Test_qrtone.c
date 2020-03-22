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


#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "qrtone.h"
#include "minunit.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define CHECK(a) if(!a) return -1

MU_TEST(testPolynomial) {

		generic_gf_t field;
		qrtone_generic_gf_init(&field, 0x011D, 256, 0);
		mu_assert_int_eq(0, field.zero.coefficients[0]);

		generic_gf_poly_t poly;
		mu_assert_int_eq(QRTONE_NO_ERRORS, qrtone_generic_gf_build_monomial(&poly, 0, -1));
		mu_assert_int_eq(-1, poly.coefficients[0]);

		qrtone_generic_gf_poly_free(&poly);
		qrtone_generic_gf_free(&field);
}

MU_TEST(testZero) {

	generic_gf_t field;
	qrtone_generic_gf_init(&field, 0x011D, 256, 0);

	generic_gf_poly_t poly;
	mu_assert_int_eq(QRTONE_NO_ERRORS, qrtone_generic_gf_build_monomial(&poly, 1, 0));
	mu_assert_int_eq(field.zero.coefficients_length, poly.coefficients_length);
	mu_assert_int_eq(field.zero.coefficients[0], poly.coefficients[0]);
	qrtone_generic_gf_poly_free(&poly);

	mu_assert_int_eq(QRTONE_NO_ERRORS, qrtone_generic_gf_build_monomial(&poly, 1, 2));
	generic_gf_poly_t res;
	qrtone_generic_gf_poly_multiply(&poly,&field, 0, &res);
	mu_assert_int_eq(field.zero.coefficients_length, res.coefficients_length);
	mu_assert_int_eq(field.zero.coefficients[0], res.coefficients[0]);
	qrtone_generic_gf_poly_free(&poly);
	qrtone_generic_gf_poly_free(&res);


	qrtone_generic_gf_free(&field);
}


MU_TEST(testEvaluate) {

	generic_gf_t field;
	qrtone_generic_gf_init(&field, 0x011D, 256, 0);

	generic_gf_poly_t poly;
	mu_assert_int_eq(QRTONE_NO_ERRORS, qrtone_generic_gf_build_monomial(&poly, 0, 3));

	mu_assert_int_eq(3, qrtone_generic_gf_poly_evaluate_at(&poly, &field, 0));


	qrtone_generic_gf_poly_free(&poly);
	qrtone_generic_gf_free(&field);
}

void testEncoder(reed_solomon_encoder_t* encoder, int32_t* data_words,int32_t data_words_length, int32_t* ec_words, int32_t ec_words_length) {
	int32_t message_expected_length = data_words_length + ec_words_length;
	int32_t* message_expected = malloc(sizeof(int32_t) * message_expected_length);

	int32_t message_length = data_words_length + ec_words_length;
	int32_t* message = malloc(sizeof(int32_t) * message_length);
	memset(message, 0, sizeof(int32_t) * message_length);
	memcpy(message_expected, data_words, sizeof(int32_t) * data_words_length);
	memcpy(message_expected + data_words_length, ec_words, sizeof(int32_t) * ec_words_length);
	memcpy(message, data_words, sizeof(int32_t) * data_words_length);

	qrtone_reed_solomon_encoder_encode(encoder, message, message_length, ec_words_length);

	mu_assert_int_array_eq(message_expected, message_expected_length, message, message_length);
	
	free(message_expected);
	free(message);
}

MU_TEST(testDataMatrix1) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x012D, 256, 1);
	int32_t data_words[] = { 142, 164, 186 };
	int32_t ec_words[] = { 114, 25 , 5, 88, 102};

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}

MU_TEST(testDataMatrix2) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x012D, 256, 1);
	int32_t data_words[] = { 0x69, 0x75, 0x75, 0x71, 0x3B, 0x30, 0x30, 0x64,
		0x70, 0x65, 0x66, 0x2F, 0x68, 0x70, 0x70, 0x68,
		0x6D, 0x66, 0x2F, 0x64, 0x70, 0x6E, 0x30, 0x71,
		0x30, 0x7B, 0x79, 0x6A, 0x6F, 0x68, 0x30, 0x81,
		0xF0, 0x88, 0x1F, 0xB5 };
	int32_t ec_words[] = { 0x1C, 0x64, 0xEE, 0xEB, 0xD0, 0x1D, 0x00, 0x03,
		0xF0, 0x1C, 0xF1, 0xD0, 0x6D, 0x00, 0x98, 0xDA,
		0x80, 0x88, 0xBE, 0xFF, 0xB7, 0xFA, 0xA9, 0x95 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}

MU_TEST(testQRCode1) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x011D, 256, 0);
	int32_t data_words[] = { 0x10, 0x20, 0x0C, 0x56, 0x61, 0x80, 0xEC, 0x11,
		0xEC, 0x11, 0xEC, 0x11, 0xEC, 0x11, 0xEC, 0x11 };
	int32_t ec_words[] = { 0xA5, 0x24, 0xD4, 0xC1, 0xED, 0x36, 0xC7, 0x87,
		0x2C, 0x55 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}


MU_TEST(testQRCode2) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x011D, 256, 0);
	int32_t data_words[] = { 0x72, 0x67, 0x2F, 0x77, 0x69, 0x6B, 0x69, 0x2F,
		0x4D, 0x61, 0x69, 0x6E, 0x5F, 0x50, 0x61, 0x67,
		0x65, 0x3B, 0x3B, 0x00, 0xEC, 0x11, 0xEC, 0x11,
		0xEC, 0x11, 0xEC, 0x11, 0xEC, 0x11, 0xEC, 0x11 };
	int32_t ec_words[] = { 0xD8, 0xB8, 0xEF, 0x14, 0xEC, 0xD0, 0xCC, 0x85,
		0x73, 0x40, 0x0B, 0xB5, 0x5A, 0xB8, 0x8B, 0x2E,
		0x08, 0x62 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}

MU_TEST(testAztec1) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x13, 16, 1);
	int32_t data_words[] = { 0x5, 0x6 };
	int32_t ec_words[] = { 0x3, 0x2, 0xB, 0xB, 0x7 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}

MU_TEST(testAztec2) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x13, 16, 1);
	int32_t data_words[] = { 0x0, 0x0, 0x0, 0x9 };
	int32_t ec_words[] = { 0xA, 0xD, 0x8, 0x6, 0x5, 0x6 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}


MU_TEST(testAztec3) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x13, 16, 1);
	int32_t data_words[] = { 0x2, 0x8, 0x8, 0x7 };
	int32_t ec_words[] = { 0xE, 0xC, 0xA, 0x9, 0x6, 0x8 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}

MU_TEST(testAztec4) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x43, 64, 1);
	int32_t data_words[] = { 0x9, 0x32, 0x1, 0x29, 0x2F, 0x2, 0x27, 0x25, 0x1, 0x1B };
	int32_t ec_words[] = { 0x2C, 0x2, 0xD, 0xD, 0xA, 0x16, 0x28, 0x9, 0x22, 0xA, 0x14 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}

MU_TEST(testAztec5) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x012D, 256, 1);
	int32_t data_words[] = { 0xE0, 0x86, 0x42, 0x98, 0xE8, 0x4A, 0x96, 0xC6,
		0xB9, 0xF0, 0x8C, 0xA7, 0x4A, 0xDA, 0xF8, 0xCE,
		0xB7, 0xDE, 0x88, 0x64, 0x29, 0x8E, 0x84, 0xA9,
		0x6C, 0x6B, 0x9F, 0x08, 0xCA, 0x74, 0xAD, 0xAF,
		0x8C, 0xEB, 0x7C, 0x10, 0xC8, 0x53, 0x1D, 0x09,
		0x52, 0xD8, 0xD7, 0x3E, 0x11, 0x94, 0xE9, 0x5B,
		0x5F, 0x19, 0xD6, 0xFB, 0xD1, 0x0C, 0x85, 0x31,
		0xD0, 0x95, 0x2D, 0x8D, 0x73, 0xE1, 0x19, 0x4E,
		0x95, 0xB5, 0xF1, 0x9D, 0x6F };
	int32_t ec_words[] = { 0x31, 0xD7, 0x04, 0x46, 0xB2, 0xC1, 0x06, 0x94,
		0x17, 0xE5, 0x0C, 0x2B, 0xA3, 0x99, 0x15, 0x7F,
		0x16, 0x3C, 0x66, 0xBA, 0x33, 0xD9, 0xE8, 0x87,
		0x86, 0xBB, 0x4B, 0x15, 0x4E, 0x4A, 0xDE, 0xD4,
		0xED, 0xA1, 0xF8, 0x47, 0x2A, 0x50, 0xA6, 0xBC,
		0x53, 0x7D, 0x29, 0xFE, 0x06, 0x49, 0xF3, 0x73,
		0x9F, 0xC1, 0x75 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}


MU_TEST(testAztec6) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x409, 1024, 1);
	int32_t data_words[] = { 0x15C, 0x1E1, 0x2D5, 0x02E, 0x048, 0x1E2, 0x037, 0x0CD,
		0x02E, 0x056, 0x26A, 0x281, 0x1C2, 0x1A6, 0x296, 0x045,
		0x041, 0x0AA, 0x095, 0x2CE, 0x003, 0x38F, 0x2CD, 0x1A2,
		0x036, 0x1AD, 0x04E, 0x090, 0x271, 0x0D3, 0x02E, 0x0D5,
		0x2D4, 0x032, 0x2CA, 0x281, 0x0AA, 0x04E, 0x024, 0x2D3,
		0x296, 0x281, 0x0E2, 0x08A, 0x1AA, 0x28A, 0x280, 0x07C,
		0x286, 0x0A1, 0x1D0, 0x1AD, 0x154, 0x032, 0x2C2, 0x1C1,
		0x145, 0x02B, 0x2D4, 0x2B0, 0x033, 0x2D5, 0x276, 0x1C1,
		0x282, 0x10A, 0x2B5, 0x154, 0x003, 0x385, 0x20F, 0x0C4,
		0x02D, 0x050, 0x266, 0x0D5, 0x033, 0x2D5, 0x276, 0x1C1,
		0x0D4, 0x2A0, 0x08F, 0x0C4, 0x024, 0x20F, 0x2E2, 0x1AD,
		0x154, 0x02E, 0x056, 0x26A, 0x281, 0x090, 0x1E5, 0x14E,
		0x0CF, 0x2B6, 0x1C1, 0x28A, 0x2A1, 0x04E, 0x0D5, 0x003,
		0x391, 0x122, 0x286, 0x1AD, 0x2D4, 0x028, 0x262, 0x2EA,
		0x0A2, 0x004, 0x176, 0x295, 0x201, 0x0D5, 0x024, 0x20F,
		0x116, 0x0C1, 0x056, 0x095, 0x213, 0x004, 0x1EA, 0x28A,
		0x02A, 0x234, 0x2CE, 0x037, 0x157, 0x0D3, 0x262, 0x026,
		0x262, 0x2A0, 0x086, 0x106, 0x2A1, 0x126, 0x1E5, 0x266,
		0x26A, 0x2A1, 0x0E6, 0x1AA, 0x281, 0x2B6, 0x271, 0x154,
		0x02F, 0x0C4, 0x02D, 0x213, 0x0CE, 0x003, 0x38F, 0x2CD,
		0x1A2, 0x036, 0x1B5, 0x26A, 0x086, 0x280, 0x086, 0x1AA,
		0x2A1, 0x226, 0x1AD, 0x0CF, 0x2A6, 0x292, 0x2C6, 0x022,
		0x1AA, 0x256, 0x0D5, 0x02D, 0x050, 0x266, 0x0D5, 0x004,
		0x176, 0x295, 0x201, 0x0D3, 0x055, 0x031, 0x2CD, 0x2EA,
		0x1E2, 0x261, 0x1EA, 0x28A, 0x004, 0x145, 0x026, 0x1A6,
		0x1C6, 0x1F5, 0x2CE, 0x034, 0x051, 0x146, 0x1E1, 0x0B0,
		0x1B0, 0x261, 0x0D5, 0x025, 0x142, 0x1C0, 0x07C, 0x0B0,
		0x1E6, 0x081, 0x044, 0x02F, 0x2CF, 0x081, 0x290, 0x0A2,
		0x1A6, 0x281, 0x0CD, 0x155, 0x031, 0x1A2, 0x086, 0x262,
		0x2A1, 0x0CD, 0x0CA, 0x0E6, 0x1E5, 0x003, 0x394, 0x0C5,
		0x030, 0x26F, 0x053, 0x0C1, 0x1B6, 0x095, 0x2D4, 0x030,
		0x26F, 0x053, 0x0C0, 0x07C, 0x2E6, 0x295, 0x143, 0x2CD,
		0x2CE, 0x037, 0x0C9, 0x144, 0x2CD, 0x040, 0x08E, 0x054,
		0x282, 0x022, 0x2A1, 0x229, 0x053, 0x0D5, 0x262, 0x027,
		0x26A, 0x1E8, 0x14D, 0x1A2, 0x004, 0x26A, 0x296, 0x281,
		0x176, 0x295, 0x201, 0x0E2, 0x2C4, 0x143, 0x2D4, 0x026,
		0x262, 0x2A0, 0x08F, 0x0C4, 0x031, 0x213, 0x2B5, 0x155,
		0x213, 0x02F, 0x143, 0x121, 0x2A6, 0x1AD, 0x2D4, 0x034,
		0x0C5, 0x026, 0x295, 0x003, 0x396, 0x2A1, 0x176, 0x295,
		0x201, 0x0AA, 0x04E, 0x004, 0x1B0, 0x070, 0x275, 0x154,
		0x026, 0x2C1, 0x2B3, 0x154, 0x2AA, 0x256, 0x0C1, 0x044,
		0x004, 0x23F };
	int32_t ec_words[] = { 0x379, 0x099, 0x348, 0x010, 0x090, 0x196, 0x09C, 0x1FF,
		0x1B0, 0x32D, 0x244, 0x0DE, 0x201, 0x386, 0x163, 0x11F,
		0x39B, 0x344, 0x3FE, 0x02F, 0x188, 0x113, 0x3D9, 0x102,
		0x04A, 0x2E1, 0x1D1, 0x18E, 0x077, 0x262, 0x241, 0x20D,
		0x1B8, 0x11D, 0x0D0, 0x0A5, 0x29C, 0x24D, 0x3E7, 0x006,
		0x2D0, 0x1B7, 0x337, 0x178, 0x0F1, 0x1E0, 0x00B, 0x01E,
		0x0DA, 0x1C6, 0x2D9, 0x00D, 0x28B, 0x34A, 0x252, 0x27A,
		0x057, 0x0CA, 0x2C2, 0x2E4, 0x3A6, 0x0E3, 0x22B, 0x307,
		0x174, 0x292, 0x10C, 0x1ED, 0x2FD, 0x2D4, 0x0A7, 0x051,
		0x34F, 0x07A, 0x1D5, 0x01D, 0x22E, 0x2C2, 0x1DF, 0x08F,
		0x105, 0x3FE, 0x286, 0x2A2, 0x3B1, 0x131, 0x285, 0x362,
		0x315, 0x13C, 0x0F9, 0x1A2, 0x28D, 0x246, 0x1B3, 0x12C,
		0x2AD, 0x0F8, 0x222, 0x0EC, 0x39F, 0x358, 0x014, 0x229,
		0x0C8, 0x360, 0x1C2, 0x031, 0x098, 0x041, 0x3E4, 0x046,
		0x332, 0x318, 0x2E3, 0x24E, 0x3E2, 0x1E1, 0x0BE, 0x239,
		0x306, 0x3A5, 0x352, 0x351, 0x275, 0x0ED, 0x045, 0x229,
		0x0BF, 0x05D, 0x253, 0x1BE, 0x02E, 0x35A, 0x0E4, 0x2E9,
		0x17A, 0x166, 0x03C, 0x007 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}

MU_TEST(testAztec7) {

	reed_solomon_encoder_t encoder;
	qrtone_reed_solomon_encoder_init(&encoder, 0x1069, 4096, 1);
	int32_t data_words[] = { 0x571, 0xE1B, 0x542, 0xE12, 0x1E2, 0x0DC, 0xCD0, 0xB85,
		0x69A, 0xA81, 0x709, 0xA6A, 0x584, 0x510, 0x4AA, 0x256,
		0xCE0, 0x0F8, 0xFB3, 0x5A2, 0x0D9, 0xAD1, 0x389, 0x09C,
		0x4D3, 0x0B8, 0xD5B, 0x503, 0x2B2, 0xA81, 0x2A8, 0x4E0,
		0x92D, 0x3A5, 0xA81, 0x388, 0x8A6, 0xAA8, 0xAA0, 0x07C,
		0xA18, 0xA17, 0x41A, 0xD55, 0x032, 0xB09, 0xC15, 0x142,
		0xBB5, 0x2B0, 0x0CE, 0xD59, 0xD9C, 0x1A0, 0x90A, 0xAD5,
		0x540, 0x0F8, 0x583, 0xCC4, 0x0B4, 0x509, 0x98D, 0x50C,
		0xED5, 0x9D9, 0xC13, 0x52A, 0x023, 0xCC4, 0x092, 0x0FB,
		0x89A, 0xD55, 0x02E, 0x15A, 0x6AA, 0x049, 0x079, 0x54E,
		0x33E, 0xB67, 0x068, 0xAA8, 0x44E, 0x354, 0x03E, 0x452,
		0x2A1, 0x9AD, 0xB50, 0x289, 0x8AE, 0xA28, 0x804, 0x5DA,
		0x958, 0x04D, 0x509, 0x20F, 0x458, 0xC11, 0x589, 0x584,
		0xC04, 0x7AA, 0x8A0, 0xAA3, 0x4B3, 0x837, 0x55C, 0xD39,
		0x882, 0x698, 0xAA0, 0x219, 0x06A, 0x852, 0x679, 0x666,
		0x9AA, 0xA13, 0x99A, 0xAA0, 0x6B6, 0x9C5, 0x540, 0xBCC,
		0x40B, 0x613, 0x338, 0x03E, 0x3EC, 0xD68, 0x836, 0x6D6,
		0x6A2, 0x1A8, 0x021, 0x9AA, 0xA86, 0x266, 0xB4C, 0xFA9,
		0xA92, 0xB18, 0x226, 0xAA5, 0x635, 0x42D, 0x142, 0x663,
		0x540, 0x45D, 0xA95, 0x804, 0xD31, 0x543, 0x1B3, 0x6EA,
		0x78A, 0x617, 0xAA8, 0xA01, 0x145, 0x099, 0xA67, 0x19F,
		0x5B3, 0x834, 0x145, 0x467, 0x84B, 0x06C, 0x261, 0x354,
		0x255, 0x09C, 0x01F, 0x0B0, 0x798, 0x811, 0x102, 0xFB3,
		0xC81, 0xA40, 0xA26, 0x9A8, 0x133, 0x555, 0x0C5, 0xA22,
		0x1A6, 0x2A8, 0x4CD, 0x328, 0xE67, 0x940, 0x3E5, 0x0C5,
		0x0C2, 0x6F1, 0x4CC, 0x16D, 0x895, 0xB50, 0x309, 0xBC5,
		0x330, 0x07C, 0xB9A, 0x955, 0x0EC, 0xDB3, 0x837, 0x325,
		0x44B, 0x344, 0x023, 0x854, 0xA08, 0x22A, 0x862, 0x914,
		0xCD5, 0x988, 0x279, 0xA9E, 0x853, 0x5A2, 0x012, 0x6AA,
		0x5A8, 0x15D, 0xA95, 0x804, 0xE2B, 0x114, 0x3B5, 0x026,
		0x98A, 0xA02, 0x3CC, 0x40C, 0x613, 0xAD5, 0x558, 0x4C2,
		0xF50, 0xD21, 0xA99, 0xADB, 0x503, 0x431, 0x426, 0xA54,
		0x03E, 0x5AA, 0x15D, 0xA95, 0x804, 0xAA1, 0x380, 0x46C,
		0x070, 0x9D5, 0x540, 0x9AC, 0x1AC, 0xD54, 0xAAA, 0x563,
		0x044, 0x401, 0x220, 0x9F1, 0x4F0, 0xDAA, 0x170, 0x90F,
		0x106, 0xE66, 0x85C, 0x2B4, 0xD54, 0x0B8, 0x4D3, 0x52C,
		0x228, 0x825, 0x512, 0xB67, 0x007, 0xC7D, 0x9AD, 0x106,
		0xCD6, 0x89C, 0x484, 0xE26, 0x985, 0xC6A, 0xDA8, 0x195,
		0x954, 0x095, 0x427, 0x049, 0x69D, 0x2D4, 0x09C, 0x445,
		0x355, 0x455, 0x003, 0xE50, 0xC50, 0xBA0, 0xD6A, 0xA81,
		0x958, 0x4E0, 0xA8A, 0x15D, 0xA95, 0x806, 0x76A, 0xCEC,
		0xE0D, 0x048, 0x556, 0xAAA, 0x007, 0xC2C, 0x1E6, 0x205,
		0xA28, 0x4CC, 0x6A8, 0x676, 0xACE, 0xCE0, 0x9A9, 0x501,
		0x1E6, 0x204, 0x907, 0xDC4, 0xD6A, 0xA81, 0x70A, 0xD35,
		0x502, 0x483, 0xCAA, 0x719, 0xF5B, 0x383, 0x455, 0x422,
		0x71A, 0xA01, 0xF22, 0x915, 0x0CD, 0x6DA, 0x814, 0x4C5,
		0x751, 0x440, 0x22E, 0xD4A, 0xC02, 0x6A8, 0x490, 0x7A2,
		0xC60, 0x8AC, 0x4AC, 0x260, 0x23D, 0x545, 0x055, 0x1A5,
		0x9C1, 0xBAA, 0xE69, 0xCC4, 0x134, 0xC55, 0x010, 0xC83,
		0x542, 0x933, 0xCB3, 0x34D, 0x550, 0x9CC, 0xD55, 0x035,
		0xB4E, 0x2AA, 0x05E, 0x620, 0x5B0, 0x999, 0xC01, 0xF1F,
		0x66B, 0x441, 0xB36, 0xB35, 0x10D, 0x401, 0x0CD, 0x554,
		0x313, 0x35A, 0x67D, 0x4D4, 0x958, 0xC11, 0x355, 0x2B1,
		0xAA1, 0x68A, 0x133, 0x1AA, 0x022, 0xED4, 0xAC0, 0x269,
		0x8AA, 0x18D, 0x9B7, 0x53C, 0x530, 0xBD5, 0x450, 0x08A,
		0x284, 0xCD3, 0x38C, 0xFAD, 0x9C1, 0xA0A, 0x2A3, 0x3C2,
		0x583, 0x613, 0x09A, 0xA12, 0xA84, 0xE00, 0xF85, 0x83C,
		0xC40, 0x888, 0x17D, 0x9E4, 0x0D2, 0x051, 0x34D, 0x409,
		0x9AA, 0xA86, 0x2D1, 0x10D, 0x315, 0x426, 0x699, 0x473,
		0x3CA, 0x01F, 0x286, 0x286, 0x137, 0x8A6, 0x60B, 0x6C4,
		0xADA, 0x818, 0x4DE, 0x299, 0x803, 0xE5C, 0xD4A, 0xA87,
		0x66D, 0x9C1, 0xB99, 0x2A2, 0x59A, 0x201, 0x1C2, 0xA50,
		0x411, 0x543, 0x148, 0xA66, 0xACC, 0x413, 0xCD4, 0xF42,
		0x9AD, 0x100, 0x935, 0x52D, 0x40A, 0xED4, 0xAC0, 0x271,
		0x588, 0xA1D, 0xA81, 0x34C, 0x550, 0x11E, 0x620, 0x630,
		0x9D6, 0xAAA, 0xC26, 0x17A, 0x869, 0x0D4, 0xCD6, 0xDA8,
		0x1A1, 0x8A1, 0x352, 0xA01, 0xF2D, 0x50A, 0xED4, 0xAC0,
		0x255, 0x09C, 0x023, 0x603, 0x84E, 0xAAA, 0x04D, 0x60D,
		0x66A, 0xA55, 0x52B, 0x182, 0x220, 0x091, 0x00F, 0x8A7,
		0x86D, 0x50B, 0x848, 0x788, 0x373, 0x342, 0xE15, 0xA6A,
		0xA05, 0xC26, 0x9A9, 0x611, 0x441, 0x2A8, 0x95B, 0x380,
		0x3E3, 0xECD, 0x688, 0x366, 0xB44, 0xE24, 0x271, 0x34C,
		0x2E3, 0x56D, 0x40C, 0xACA, 0xA04, 0xAA1, 0x382, 0x4B4,
		0xE96, 0xA04, 0xE22, 0x29A, 0xAA2, 0xA80, 0x1F2, 0x862,
		0x85D, 0x06B, 0x554, 0x0CA, 0xC27, 0x054, 0x50A, 0xED4,
		0xAC0, 0x33B, 0x567, 0x670, 0x682, 0x42A, 0xB55, 0x500,
		0x3E1, 0x60F, 0x310, 0x2D1, 0x426, 0x635, 0x433, 0xB56,
		0x767, 0x04D, 0x4A8, 0x08F, 0x310, 0x248, 0x3EE, 0x26B,
		0x554, 0x0B8, 0x569, 0xAA8, 0x124, 0x1E5, 0x538, 0xCFA,
		0xD9C, 0x1A2, 0xAA1, 0x138, 0xD50, 0x0F9, 0x148, 0xA86,
		0x6B6, 0xD40, 0xA26, 0x2BA, 0x8A2, 0x011, 0x76A, 0x560,
		0x135, 0x424, 0x83D, 0x163, 0x045, 0x625, 0x613, 0x011,
		0xEAA, 0x282, 0xA8D, 0x2CE, 0x0DD, 0x573, 0x4E6, 0x209,
		0xA62, 0xA80, 0x864, 0x1AA, 0x149, 0x9E5, 0x99A, 0x6AA,
		0x84E, 0x66A, 0xA81, 0xADA, 0x715, 0x502, 0xF31, 0x02D,
		0x84C, 0xCE0, 0x0F8, 0xFB3, 0x5A2, 0x0D9, 0xB59, 0xA88,
		0x6A0, 0x086, 0x6AA, 0xA18, 0x99A, 0xD33, 0xEA6, 0xA4A,
		0xC60, 0x89A, 0xA95, 0x8D5, 0x0B4, 0x509, 0x98D, 0x501,
		0x176, 0xA56, 0x013, 0x4C5, 0x50C, 0x6CD, 0xBA9, 0xE29,
		0x85E, 0xAA2, 0x804, 0x514, 0x266, 0x99C, 0x67D, 0x6CE,
		0x0D0, 0x515, 0x19E, 0x12C, 0x1B0, 0x984, 0xD50, 0x954,
		0x270, 0x07C, 0x2C1, 0xE62, 0x044, 0x40B, 0xECF, 0x206,
		0x902, 0x89A, 0x6A0, 0x4CD, 0x554, 0x316, 0x888, 0x698,
		0xAA1, 0x334, 0xCA3, 0x99E, 0x500, 0xF94, 0x314, 0x309,
		0xBC5, 0x330, 0x5B6, 0x256, 0xD40, 0xC26, 0xF14, 0xCC0,
		0x1F2, 0xE6A, 0x554, 0x3B3, 0x6CE, 0x0DC, 0xC95, 0x12C,
		0xD10, 0x08E, 0x152, 0x820, 0x8AA, 0x18A, 0x453, 0x356,
		0x620, 0x9E6, 0xA7A, 0x14D, 0x688, 0x049, 0xAA9, 0x6A0,
		0x576, 0xA56, 0x013, 0x8AC, 0x450, 0xED4, 0x09A, 0x62A,
		0x808, 0xF31, 0x031, 0x84E, 0xB55, 0x561, 0x30B, 0xD43,
		0x486, 0xA66, 0xB6D, 0x40D, 0x0C5, 0x09A, 0x950, 0x0F9,
		0x6A8, 0x576, 0xA56, 0x012, 0xA84, 0xE01, 0x1B0, 0x1C2,
		0x755, 0x502, 0x6B0, 0x6B3, 0x552, 0xAA9, 0x58C, 0x111,
		0x004, 0x882, 0x7C5, 0x3C3, 0x6A8, 0x5C2, 0x43C, 0x41B,
		0x99A, 0x170, 0xAD3, 0x550, 0x2E1, 0x34D, 0x4B0, 0x8A2,
		0x095, 0x44A, 0xD9C, 0x01F, 0x1F6, 0x6B4, 0x41B, 0x35A,
		0x271, 0x213, 0x89A, 0x617, 0x1AB, 0x6A0, 0x656, 0x550,
		0x255, 0x09C, 0x125, 0xA74, 0xB50, 0x271, 0x114, 0xD55,
		0x154, 0x00F, 0x943, 0x142, 0xE83, 0x5AA, 0xA06, 0x561,
		0x382, 0xA28, 0x576, 0xA56, 0x019, 0xDAB, 0x3B3, 0x834,
		0x121, 0x55A, 0xAA8, 0x01F, 0x0B0, 0x798, 0x816, 0x8A1,
		0x331, 0xAA1, 0x9DA, 0xB3B, 0x382, 0x6A5, 0x404, 0x798,
		0x812, 0x41F, 0x713, 0x5AA, 0xA05, 0xC2B, 0x4D5, 0x409,
		0x20F, 0x2A9, 0xC67, 0xD6C, 0xE0D, 0x155, 0x089, 0xC6A,
		0x807, 0xC8A, 0x454, 0x335, 0xB6A, 0x051, 0x315, 0xD45,
		0x100, 0x8BB, 0x52B, 0x009, 0xAA1, 0x241, 0xE8B, 0x182,
		0x2B1, 0x2B0, 0x980, 0x8F5, 0x514, 0x154, 0x696, 0x706,
		0xEAB, 0x9A7, 0x310, 0x4D3, 0x154, 0x043, 0x20D, 0x50A,
		0x4CF, 0x2CC, 0xD35, 0x542, 0x733, 0x554, 0x0D6, 0xD38,
		0xAA8, 0x179, 0x881, 0x6C2, 0x667, 0x007, 0xC7D, 0x9AD,
		0x106, 0xCDA, 0xCD4, 0x435, 0x004, 0x335, 0x550, 0xC4C,
		0xD69, 0x9F5, 0x352, 0x563, 0x044, 0xD54, 0xAC6, 0xA85,
		0xA28, 0x4CC, 0x6A8, 0x08B, 0xB52, 0xB00, 0x9A6, 0x2A8,
		0x636, 0x6DD, 0x4F1, 0x4C2, 0xF55, 0x140, 0x228, 0xA13,
		0x34C, 0xE33, 0xEB6, 0x706, 0x828, 0xA8C, 0xF09, 0x60D,
		0x84C, 0x26A, 0x84A, 0xA13, 0x803, 0xE16, 0x0F3, 0x102,
		0x220, 0x5F6, 0x790, 0x348, 0x144, 0xD35, 0x026, 0x6AA,
		0xA18, 0xB44, 0x434, 0xC55, 0x099, 0xA65, 0x1CC, 0xF28,
		0x07C, 0xA18, 0xA18, 0x4DE, 0x299, 0x82D, 0xB12, 0xB6A,
		0x061, 0x378, 0xA66, 0x00F, 0x973, 0x52A, 0xA1D, 0x9B6,
		0x706, 0xE64, 0xA89, 0x668, 0x804, 0x70A, 0x941, 0x045,
		0x50C, 0x522, 0x99A, 0xB31, 0x04F, 0x353, 0xD0A, 0x6B4,
		0x402, 0x4D5, 0x4B5, 0x02B, 0xB52, 0xB00, 0x9C5, 0x622,
		0x876, 0xA04, 0xD31, 0x540, 0x479, 0x881, 0x8C2, 0x75A,
		0xAAB, 0x098, 0x5EA, 0x1A4, 0x353, 0x35B, 0x6A0, 0x686,
		0x284, 0xD4A, 0x807, 0xCB5, 0x42B, 0xB52, 0xB00, 0x954,
		0x270, 0x08D, 0x80E, 0x13A, 0xAA8, 0x135, 0x835, 0x9AA,
		0x801, 0xF14, 0xF0D, 0xAA1, 0x709, 0x0F1, 0x06E, 0x668,
		0x5C2, 0xB4D, 0x540, 0xB84, 0xD35, 0x2C2, 0x288, 0x255,
		0x12B, 0x670, 0x07C, 0x7D9, 0xAD1, 0x06C, 0xD68, 0x9C4,
		0x84E, 0x269, 0x85C, 0x6AD, 0xA81, 0x959, 0x540, 0x954,
		0x270, 0x496, 0x9D2, 0xD40, 0x9C4, 0x453, 0x554, 0x550,
		0x03E, 0x50C, 0x50B, 0xA0D, 0x6AA, 0x819, 0x584, 0xE0A,
		0x8A1, 0x5DA, 0x958, 0x067, 0x6AC, 0xECE, 0x0D0, 0x485,
		0x56A, 0xAA0, 0x07C, 0x2C1, 0xE62, 0x05A, 0x284, 0xCC6,
		0xA86, 0x76A, 0xCEC, 0xE09, 0xA95, 0x011, 0xE62, 0x049,
		0x07D, 0xC4D, 0x6AA, 0x817, 0x0AD, 0x355, 0x024, 0x83C,
		0xAA7, 0x19F, 0x5B3, 0x834, 0x554, 0x227, 0x1AA, 0x01F,
		0x229, 0x150, 0xCD6, 0xDA8, 0x144, 0xC57, 0x514, 0x402,
		0x2ED, 0x4AC, 0x026, 0xA84, 0x907, 0xA2C, 0x608, 0xAC4,
		0xAC2, 0x602, 0x3D5, 0x450, 0x551, 0xA59, 0xC1B, 0xAAE,
		0x69C, 0xC41, 0x34C, 0x550, 0x10C, 0x835, 0x429, 0x33C,
		0xB33, 0x4D5, 0x509, 0xCCD, 0x550, 0x35B, 0x4E2, 0xAA0,
		0x5E6, 0x205, 0xB09, 0x99C, 0x09F };
	int32_t ec_words[] = { 0xD54, 0x221, 0x154, 0x7CD, 0xBF3, 0x112, 0x89B, 0xC5E,
		0x9CD, 0x07E, 0xFB6, 0x78F, 0x7FA, 0x16F, 0x377, 0x4B4,
		0x62D, 0x475, 0xBC2, 0x861, 0xB72, 0x9D0, 0x76A, 0x5A1,
		0x22A, 0xF74, 0xDBA, 0x8B1, 0x139, 0xDCD, 0x012, 0x293,
		0x705, 0xA34, 0xDD5, 0x3D2, 0x7F8, 0x0A6, 0x89A, 0x346,
		0xCE0, 0x690, 0x40E, 0xFF3, 0xC4D, 0x97F, 0x9C9, 0x016,
		0x73A, 0x923, 0xBCE, 0xFA9, 0xE6A, 0xB92, 0x02A, 0x07C,
		0x04B, 0x8D5, 0x753, 0x42E, 0x67E, 0x87C, 0xEE6, 0xD7D,
		0x2BF, 0xFB2, 0xFF8, 0x42F, 0x4CB, 0x214, 0x779, 0x02D,
		0x606, 0xA02, 0x08A, 0xD4F, 0xB87, 0xDDF, 0xC49, 0xB51,
		0x0E9, 0xF89, 0xAEF, 0xC92, 0x383, 0x98D, 0x367, 0xBD3,
		0xA55, 0x148, 0x9DB, 0x913, 0xC79, 0x6FF, 0x387, 0x6EA,
		0x7FA, 0xC1B, 0x12D, 0x303, 0xBCA, 0x503, 0x0FB, 0xB14,
		0x0D4, 0xAD1, 0xAFC, 0x9DD, 0x404, 0x145, 0x6E5, 0x8ED,
		0xF94, 0xD72, 0x645, 0xA21, 0x1A8, 0xABF, 0xC03, 0x91E,
		0xD53, 0x48C, 0x471, 0x4E4, 0x408, 0x33C, 0x5DF, 0x73D,
		0xA2A, 0x454, 0xD77, 0xC48, 0x2F5, 0x96A, 0x9CF, 0x047,
		0x611, 0xE92, 0xC2F, 0xA98, 0x56D, 0x919, 0x615, 0x535,
		0x67A, 0x8C1, 0x2E2, 0xBC4, 0xBE8, 0x328, 0x04F, 0x257,
		0x3F9, 0xFA5, 0x477, 0x12E, 0x94B, 0x116, 0xEF7, 0x65F,
		0x6B3, 0x915, 0xC64, 0x9AF, 0xB6C, 0x6A2, 0x50D, 0xEA3,
		0x26E, 0xC23, 0x817, 0xA42, 0x71A, 0x9DD, 0xDA8, 0x84D,
		0x3F3, 0x85B, 0xB00, 0x1FC, 0xB0A, 0xC2F, 0x00C, 0x095,
		0xC58, 0x0E3, 0x807, 0x962, 0xC4B, 0x29A, 0x6FC, 0x958,
		0xD29, 0x59E, 0xB14, 0x95A, 0xEDE, 0xF3D, 0xFB8, 0x0E5,
		0x348, 0x2E7, 0x38E, 0x56A, 0x410, 0x3B1, 0x4B0, 0x793,
		0xAB7, 0x0BC, 0x648, 0x719, 0xE3E, 0xFB4, 0x3B4, 0xE5C,
		0x950, 0xD2A, 0x50B, 0x76F, 0x8D2, 0x3C7, 0xECC, 0x87C,
		0x53A, 0xBA7, 0x4C3, 0x148, 0x437, 0x820, 0xECD, 0x660,
		0x095, 0x2F4, 0x661, 0x6A4, 0xB74, 0x5F3, 0x1D2, 0x7EC,
		0x8E2, 0xA40, 0xA6F, 0xFC3, 0x3BE, 0x1E9, 0x52C, 0x233,
		0x173, 0x4EF, 0xA7C, 0x40B, 0x14C, 0x88D, 0xF30, 0x8D9,
		0xBDB, 0x0A6, 0x940, 0xD46, 0xB2B, 0x03E, 0x46A, 0x641,
		0xF08, 0xAFF, 0x496, 0x68A, 0x7A4, 0x0BA, 0xD43, 0x515,
		0xB26, 0xD8F, 0x05C, 0xD6E, 0xA2C, 0xF25, 0x628, 0x4E5,
		0x81D, 0xA2A, 0x1FF, 0x302, 0xFBD, 0x6D9, 0x711, 0xD8B,
		0xE5C, 0x5CF, 0x42E, 0x008, 0x863, 0xB6F, 0x1E1, 0x3DA,
		0xACE, 0x82B, 0x2DB, 0x7EB, 0xC15, 0x79F, 0xA79, 0xDAF,
		0x00D, 0x2F6, 0x0CE, 0x370, 0x7E8, 0x9E6, 0x89F, 0xAE9,
		0x175, 0xA95, 0x06B, 0x9DF, 0xAFF, 0x45B, 0x823, 0xAA4,
		0xC79, 0x773, 0x886, 0x854, 0x0A5, 0x6D1, 0xE55, 0xEBB,
		0x518, 0xE50, 0xF8F, 0x8CC, 0x834, 0x388, 0xCD2, 0xFC1,
		0xA55, 0x1F8, 0xD1F, 0xE08, 0xF93, 0x362, 0xA22, 0x9FA,
		0xCE5, 0x3C3, 0xDD4, 0xC53, 0xB94, 0xAD0, 0x6EB, 0x68D,
		0x660, 0x8FC, 0xBCD, 0x914, 0x16F, 0x4C0, 0x134, 0xE1A,
		0x76F, 0x9CB, 0x660, 0xEA0, 0x320, 0x15A, 0xCE3, 0x7E8,
		0x03E, 0xB9A, 0xC90, 0xA14, 0x256, 0x1A8, 0x639, 0x7C6,
		0xA59, 0xA65, 0x956, 0x9E4, 0x592, 0x6A9, 0xCFF, 0x4DC,
		0xAA3, 0xD2A, 0xFDE, 0xA87, 0xBF5, 0x9F0, 0xC32, 0x94F,
		0x675, 0x9A6, 0x369, 0x648, 0x289, 0x823, 0x498, 0x574,
		0x8D1, 0xA13, 0xD1A, 0xBB5, 0xA19, 0x7F7, 0x775, 0x138,
		0x949, 0xA4C, 0xE36, 0x126, 0xC85, 0xE05, 0xFEE, 0x962,
		0x36D, 0x08D, 0xC76, 0x1E1, 0x1EC, 0x8D7, 0x231, 0xB68,
		0x03C, 0x1DE, 0x7DF, 0x2B1, 0x09D, 0xC81, 0xDA4, 0x8F7,
		0x6B9, 0x947, 0x9B0 };

	testEncoder(&encoder, data_words, sizeof(data_words) / sizeof(int32_t), ec_words, sizeof(ec_words) / sizeof(int32_t));

	qrtone_reed_solomon_encoder_free(&encoder);
}


/**
 * Compute all combinations of errors for n errors with n locations
 * @param currentNumberOfErrors Cursor for current number of errors [0-tryTable.length[
 * @param tryTable Give the locations of errors, it must be the size of maximum number of errors
 * @param locations Maximum index of symbol
 * @return New value of currentNumberOfErrors
 */
int next_error_state(int32_t current_number_of_errors, int32_t* try_table,int32_t try_table_length, int32_t locations) {
	if (try_table[current_number_of_errors] == locations - 1) {
		// All possibilities have been done for the last entry
		int32_t increment_cursor = current_number_of_errors - 1;
		while (increment_cursor >= 0) {
			if (try_table[increment_cursor] < try_table[increment_cursor + 1] - 1) {
				try_table[increment_cursor]++;
				try_table[increment_cursor + 1] = try_table[increment_cursor] + 1;
				break;
			}
			increment_cursor--;
		}
		if (increment_cursor < 0) {
			current_number_of_errors++;
			try_table[0] = 0;
			int32_t c;
			for (c = 1; c < try_table_length; c++) {
				try_table[c] = try_table[c - 1] + 1;
			}
		}
	} else {
		try_table[current_number_of_errors] += 1;
	}
	return current_number_of_errors;
}

void testDecode(generic_gf_t* field, int32_t* message, int32_t message_length, int32_t ec_words_length) {
	int32_t try_cursor = 0;
	int32_t max_number_of_errors = MAX(1, ec_words_length / 2);
	int32_t* try_table = malloc(sizeof(int32_t) * max_number_of_errors);
	memset(try_table, 0, sizeof(int32_t) * max_number_of_errors);
	// Corrupt all possible data locations
	while (minunit_status == 0 && try_cursor < max_number_of_errors) {
		int32_t* decoded = malloc(sizeof(int32_t) * message_length);
		memcpy(decoded, message, sizeof(int32_t) * message_length);
		int32_t c;
		// Insert errors
		for (c = 0; c < try_cursor + 1; c++) {
			decoded[try_table[c]] = abs(~decoded[try_table[c]]) % field->size;
			if (decoded[try_table[c]] == message[try_table[c]]) {
				decoded[try_table[c]] = 0;
			}
		}
		int32_t res = qrtone_reed_solomon_decoder_decode(field, decoded, message_length, ec_words_length);		
		mu_assert_int_eq(QRTONE_NO_ERRORS, res);
		mu_assert_int_array_eq(message, message_length, decoded, message_length);
		free(decoded);
		try_cursor = next_error_state(try_cursor, try_table, max_number_of_errors, message_length);
	}
	free(try_table);
}

// Give message (payload+ecc), and test with multiple errors at all possible locations
MU_TEST(testGF16) {
	generic_gf_t field;
	qrtone_generic_gf_init(&field, 0x13, 16, 1);

	int32_t message1[] = { 3, 13, 14, 0, 4, 10, 0, 11, 13, 9, 14, 14, 0, 11 };

	testDecode(&field, message1, 14, 2);

	int32_t message2[] = { 11, 2, 15, 6, 12, 15, 1, 12, 15, 15, 4, 3 };

	testDecode(&field, message2, 12, 4);

	int32_t message3[] = { 11, 3, 7, 4, 11, 10, 11, 15, 6, 12, 5, 10 };

	testDecode(&field, message3, 12, 6);

	int32_t message4[] = { 11, 5, 15, 0, 1, 6, 9, 8, 10, 10 };

	testDecode(&field, message4, 10, 6);

	qrtone_generic_gf_free(&field);
}

MU_TEST_SUITE(test_suite) {
	MU_RUN_TEST(testEvaluate);
	MU_RUN_TEST(testPolynomial);
	MU_RUN_TEST(testZero);
	MU_RUN_TEST(testDataMatrix1);
	MU_RUN_TEST(testDataMatrix2);
	MU_RUN_TEST(testQRCode1);
	MU_RUN_TEST(testQRCode2);
	MU_RUN_TEST(testAztec1);
	MU_RUN_TEST(testAztec2);
	MU_RUN_TEST(testAztec3);
	MU_RUN_TEST(testAztec4);
	MU_RUN_TEST(testAztec5);
	MU_RUN_TEST(testAztec6);
	MU_RUN_TEST(testAztec7);
	MU_RUN_TEST(testGF16);
}

int main(int argc, char** argv) {
	//_crtBreakAlloc = 13205;
	MU_RUN_SUITE(test_suite);
	MU_REPORT();
	#ifdef _WIN32
	#ifdef _DEBUG
		_CrtDumpMemoryLeaks(); //Display memory leaks
	#endif
	#endif
	return minunit_status == 1 || minunit_fail > 0 ? -1 : 0;
}
