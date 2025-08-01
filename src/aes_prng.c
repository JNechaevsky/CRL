//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2025 Julia Nechaevskaya
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// This implements a cryptographically secure pseudorandom number
// generator for implementing secure demos. The approach taken is to
// use the AES (Rijndael) stream cipher in "counter" mode, encrypting
// an incrementing counter. The cipher key acts as the random seed.
// Cryptanalysis of AES used in this way has shown it to be an
// effective PRNG (see: Empirical Evidence concerning AES, Hellekalek
// & Wegenkittl, 2003).
//
// AES implementation is taken from the Linux kernel's AES
// implementation, found in crypto/aes_generic.c. It has been hacked
// up to work independently.
//

#include <stdint.h>

#include "aes_prng.h"
#include "doomtype.h"
#include "i_swap.h"

/*
 * Cryptographic API.
 *
 * AES Cipher Algorithm.
 *
 * Based on Brian Gladman's code.
 *
 * Linux developers:
 *  Alexander Kjeldaas <astor@fast.no>
 *  Herbert Valerio Riedel <hvr@hvrlab.org>
 *  Kyle McMartin <kyle@debian.org>
 *  Adam J. Richter <adam@yggdrasil.com> (conversion to 2.5 API).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this product
 * may be distributed under the terms of the GNU General Public License (GPL),
 * in which case the provisions of the GPL apply INSTEAD OF those given above.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 * ---------------------------------------------------------------------------
 */

#define AES_MIN_KEY_SIZE        16
#define AES_MAX_KEY_SIZE        32
#define AES_KEYSIZE_128         16
#define AES_KEYSIZE_192         24
#define AES_KEYSIZE_256         32
#define AES_BLOCK_SIZE          16
#define AES_MAX_KEYLENGTH       (15 * 16)
#define AES_MAX_KEYLENGTH_U32   (AES_MAX_KEYLENGTH / sizeof(uint32_t))

/*
 * Please ensure that the first two fields are 16-byte aligned
 * relative to the start of the structure, i.e., don't move them!
 */
typedef struct
{
    uint32_t key_enc[AES_MAX_KEYLENGTH_U32];
    uint32_t key_dec[AES_MAX_KEYLENGTH_U32];
    uint32_t key_length;
} aes_context_t;

static inline uint8_t get_byte(const uint32_t x, const unsigned n)
{
    return x >> (n << 3);
}

static const uint32_t rco_tab[10] = { 1, 2, 4, 8, 16, 32, 64, 128, 27, 54 };

static const uint32_t crypto_ft_tab[4][256] = {
    {
        0xa56363c6, 0x847c7cf8, 0x997777ee, 0x8d7b7bf6,
        0x0df2f2ff, 0xbd6b6bd6, 0xb16f6fde, 0x54c5c591,
        0x50303060, 0x03010102, 0xa96767ce, 0x7d2b2b56,
        0x19fefee7, 0x62d7d7b5, 0xe6abab4d, 0x9a7676ec,
        0x45caca8f, 0x9d82821f, 0x40c9c989, 0x877d7dfa,
        0x15fafaef, 0xeb5959b2, 0xc947478e, 0x0bf0f0fb,
        0xecadad41, 0x67d4d4b3, 0xfda2a25f, 0xeaafaf45,
        0xbf9c9c23, 0xf7a4a453, 0x967272e4, 0x5bc0c09b,
        0xc2b7b775, 0x1cfdfde1, 0xae93933d, 0x6a26264c,
        0x5a36366c, 0x413f3f7e, 0x02f7f7f5, 0x4fcccc83,
        0x5c343468, 0xf4a5a551, 0x34e5e5d1, 0x08f1f1f9,
        0x937171e2, 0x73d8d8ab, 0x53313162, 0x3f15152a,
        0x0c040408, 0x52c7c795, 0x65232346, 0x5ec3c39d,
        0x28181830, 0xa1969637, 0x0f05050a, 0xb59a9a2f,
        0x0907070e, 0x36121224, 0x9b80801b, 0x3de2e2df,
        0x26ebebcd, 0x6927274e, 0xcdb2b27f, 0x9f7575ea,
        0x1b090912, 0x9e83831d, 0x742c2c58, 0x2e1a1a34,
        0x2d1b1b36, 0xb26e6edc, 0xee5a5ab4, 0xfba0a05b,
        0xf65252a4, 0x4d3b3b76, 0x61d6d6b7, 0xceb3b37d,
        0x7b292952, 0x3ee3e3dd, 0x712f2f5e, 0x97848413,
        0xf55353a6, 0x68d1d1b9, 0x00000000, 0x2cededc1,
        0x60202040, 0x1ffcfce3, 0xc8b1b179, 0xed5b5bb6,
        0xbe6a6ad4, 0x46cbcb8d, 0xd9bebe67, 0x4b393972,
        0xde4a4a94, 0xd44c4c98, 0xe85858b0, 0x4acfcf85,
        0x6bd0d0bb, 0x2aefefc5, 0xe5aaaa4f, 0x16fbfbed,
        0xc5434386, 0xd74d4d9a, 0x55333366, 0x94858511,
        0xcf45458a, 0x10f9f9e9, 0x06020204, 0x817f7ffe,
        0xf05050a0, 0x443c3c78, 0xba9f9f25, 0xe3a8a84b,
        0xf35151a2, 0xfea3a35d, 0xc0404080, 0x8a8f8f05,
        0xad92923f, 0xbc9d9d21, 0x48383870, 0x04f5f5f1,
        0xdfbcbc63, 0xc1b6b677, 0x75dadaaf, 0x63212142,
        0x30101020, 0x1affffe5, 0x0ef3f3fd, 0x6dd2d2bf,
        0x4ccdcd81, 0x140c0c18, 0x35131326, 0x2fececc3,
        0xe15f5fbe, 0xa2979735, 0xcc444488, 0x3917172e,
        0x57c4c493, 0xf2a7a755, 0x827e7efc, 0x473d3d7a,
        0xac6464c8, 0xe75d5dba, 0x2b191932, 0x957373e6,
        0xa06060c0, 0x98818119, 0xd14f4f9e, 0x7fdcdca3,
        0x66222244, 0x7e2a2a54, 0xab90903b, 0x8388880b,
        0xca46468c, 0x29eeeec7, 0xd3b8b86b, 0x3c141428,
        0x79dedea7, 0xe25e5ebc, 0x1d0b0b16, 0x76dbdbad,
        0x3be0e0db, 0x56323264, 0x4e3a3a74, 0x1e0a0a14,
        0xdb494992, 0x0a06060c, 0x6c242448, 0xe45c5cb8,
        0x5dc2c29f, 0x6ed3d3bd, 0xefacac43, 0xa66262c4,
        0xa8919139, 0xa4959531, 0x37e4e4d3, 0x8b7979f2,
        0x32e7e7d5, 0x43c8c88b, 0x5937376e, 0xb76d6dda,
        0x8c8d8d01, 0x64d5d5b1, 0xd24e4e9c, 0xe0a9a949,
        0xb46c6cd8, 0xfa5656ac, 0x07f4f4f3, 0x25eaeacf,
        0xaf6565ca, 0x8e7a7af4, 0xe9aeae47, 0x18080810,
        0xd5baba6f, 0x887878f0, 0x6f25254a, 0x722e2e5c,
        0x241c1c38, 0xf1a6a657, 0xc7b4b473, 0x51c6c697,
        0x23e8e8cb, 0x7cdddda1, 0x9c7474e8, 0x211f1f3e,
        0xdd4b4b96, 0xdcbdbd61, 0x868b8b0d, 0x858a8a0f,
        0x907070e0, 0x423e3e7c, 0xc4b5b571, 0xaa6666cc,
        0xd8484890, 0x05030306, 0x01f6f6f7, 0x120e0e1c,
        0xa36161c2, 0x5f35356a, 0xf95757ae, 0xd0b9b969,
        0x91868617, 0x58c1c199, 0x271d1d3a, 0xb99e9e27,
        0x38e1e1d9, 0x13f8f8eb, 0xb398982b, 0x33111122,
        0xbb6969d2, 0x70d9d9a9, 0x898e8e07, 0xa7949433,
        0xb69b9b2d, 0x221e1e3c, 0x92878715, 0x20e9e9c9,
        0x49cece87, 0xff5555aa, 0x78282850, 0x7adfdfa5,
        0x8f8c8c03, 0xf8a1a159, 0x80898909, 0x170d0d1a,
        0xdabfbf65, 0x31e6e6d7, 0xc6424284, 0xb86868d0,
        0xc3414182, 0xb0999929, 0x772d2d5a, 0x110f0f1e,
        0xcbb0b07b, 0xfc5454a8, 0xd6bbbb6d, 0x3a16162c,
    }, {
        0x6363c6a5, 0x7c7cf884, 0x7777ee99, 0x7b7bf68d,
        0xf2f2ff0d, 0x6b6bd6bd, 0x6f6fdeb1, 0xc5c59154,
        0x30306050, 0x01010203, 0x6767cea9, 0x2b2b567d,
        0xfefee719, 0xd7d7b562, 0xabab4de6, 0x7676ec9a,
        0xcaca8f45, 0x82821f9d, 0xc9c98940, 0x7d7dfa87,
        0xfafaef15, 0x5959b2eb, 0x47478ec9, 0xf0f0fb0b,
        0xadad41ec, 0xd4d4b367, 0xa2a25ffd, 0xafaf45ea,
        0x9c9c23bf, 0xa4a453f7, 0x7272e496, 0xc0c09b5b,
        0xb7b775c2, 0xfdfde11c, 0x93933dae, 0x26264c6a,
        0x36366c5a, 0x3f3f7e41, 0xf7f7f502, 0xcccc834f,
        0x3434685c, 0xa5a551f4, 0xe5e5d134, 0xf1f1f908,
        0x7171e293, 0xd8d8ab73, 0x31316253, 0x15152a3f,
        0x0404080c, 0xc7c79552, 0x23234665, 0xc3c39d5e,
        0x18183028, 0x969637a1, 0x05050a0f, 0x9a9a2fb5,
        0x07070e09, 0x12122436, 0x80801b9b, 0xe2e2df3d,
        0xebebcd26, 0x27274e69, 0xb2b27fcd, 0x7575ea9f,
        0x0909121b, 0x83831d9e, 0x2c2c5874, 0x1a1a342e,
        0x1b1b362d, 0x6e6edcb2, 0x5a5ab4ee, 0xa0a05bfb,
        0x5252a4f6, 0x3b3b764d, 0xd6d6b761, 0xb3b37dce,
        0x2929527b, 0xe3e3dd3e, 0x2f2f5e71, 0x84841397,
        0x5353a6f5, 0xd1d1b968, 0x00000000, 0xededc12c,
        0x20204060, 0xfcfce31f, 0xb1b179c8, 0x5b5bb6ed,
        0x6a6ad4be, 0xcbcb8d46, 0xbebe67d9, 0x3939724b,
        0x4a4a94de, 0x4c4c98d4, 0x5858b0e8, 0xcfcf854a,
        0xd0d0bb6b, 0xefefc52a, 0xaaaa4fe5, 0xfbfbed16,
        0x434386c5, 0x4d4d9ad7, 0x33336655, 0x85851194,
        0x45458acf, 0xf9f9e910, 0x02020406, 0x7f7ffe81,
        0x5050a0f0, 0x3c3c7844, 0x9f9f25ba, 0xa8a84be3,
        0x5151a2f3, 0xa3a35dfe, 0x404080c0, 0x8f8f058a,
        0x92923fad, 0x9d9d21bc, 0x38387048, 0xf5f5f104,
        0xbcbc63df, 0xb6b677c1, 0xdadaaf75, 0x21214263,
        0x10102030, 0xffffe51a, 0xf3f3fd0e, 0xd2d2bf6d,
        0xcdcd814c, 0x0c0c1814, 0x13132635, 0xececc32f,
        0x5f5fbee1, 0x979735a2, 0x444488cc, 0x17172e39,
        0xc4c49357, 0xa7a755f2, 0x7e7efc82, 0x3d3d7a47,
        0x6464c8ac, 0x5d5dbae7, 0x1919322b, 0x7373e695,
        0x6060c0a0, 0x81811998, 0x4f4f9ed1, 0xdcdca37f,
        0x22224466, 0x2a2a547e, 0x90903bab, 0x88880b83,
        0x46468cca, 0xeeeec729, 0xb8b86bd3, 0x1414283c,
        0xdedea779, 0x5e5ebce2, 0x0b0b161d, 0xdbdbad76,
        0xe0e0db3b, 0x32326456, 0x3a3a744e, 0x0a0a141e,
        0x494992db, 0x06060c0a, 0x2424486c, 0x5c5cb8e4,
        0xc2c29f5d, 0xd3d3bd6e, 0xacac43ef, 0x6262c4a6,
        0x919139a8, 0x959531a4, 0xe4e4d337, 0x7979f28b,
        0xe7e7d532, 0xc8c88b43, 0x37376e59, 0x6d6ddab7,
        0x8d8d018c, 0xd5d5b164, 0x4e4e9cd2, 0xa9a949e0,
        0x6c6cd8b4, 0x5656acfa, 0xf4f4f307, 0xeaeacf25,
        0x6565caaf, 0x7a7af48e, 0xaeae47e9, 0x08081018,
        0xbaba6fd5, 0x7878f088, 0x25254a6f, 0x2e2e5c72,
        0x1c1c3824, 0xa6a657f1, 0xb4b473c7, 0xc6c69751,
        0xe8e8cb23, 0xdddda17c, 0x7474e89c, 0x1f1f3e21,
        0x4b4b96dd, 0xbdbd61dc, 0x8b8b0d86, 0x8a8a0f85,
        0x7070e090, 0x3e3e7c42, 0xb5b571c4, 0x6666ccaa,
        0x484890d8, 0x03030605, 0xf6f6f701, 0x0e0e1c12,
        0x6161c2a3, 0x35356a5f, 0x5757aef9, 0xb9b969d0,
        0x86861791, 0xc1c19958, 0x1d1d3a27, 0x9e9e27b9,
        0xe1e1d938, 0xf8f8eb13, 0x98982bb3, 0x11112233,
        0x6969d2bb, 0xd9d9a970, 0x8e8e0789, 0x949433a7,
        0x9b9b2db6, 0x1e1e3c22, 0x87871592, 0xe9e9c920,
        0xcece8749, 0x5555aaff, 0x28285078, 0xdfdfa57a,
        0x8c8c038f, 0xa1a159f8, 0x89890980, 0x0d0d1a17,
        0xbfbf65da, 0xe6e6d731, 0x424284c6, 0x6868d0b8,
        0x414182c3, 0x999929b0, 0x2d2d5a77, 0x0f0f1e11,
        0xb0b07bcb, 0x5454a8fc, 0xbbbb6dd6, 0x16162c3a,
    }, {
        0x63c6a563, 0x7cf8847c, 0x77ee9977, 0x7bf68d7b,
        0xf2ff0df2, 0x6bd6bd6b, 0x6fdeb16f, 0xc59154c5,
        0x30605030, 0x01020301, 0x67cea967, 0x2b567d2b,
        0xfee719fe, 0xd7b562d7, 0xab4de6ab, 0x76ec9a76,
        0xca8f45ca, 0x821f9d82, 0xc98940c9, 0x7dfa877d,
        0xfaef15fa, 0x59b2eb59, 0x478ec947, 0xf0fb0bf0,
        0xad41ecad, 0xd4b367d4, 0xa25ffda2, 0xaf45eaaf,
        0x9c23bf9c, 0xa453f7a4, 0x72e49672, 0xc09b5bc0,
        0xb775c2b7, 0xfde11cfd, 0x933dae93, 0x264c6a26,
        0x366c5a36, 0x3f7e413f, 0xf7f502f7, 0xcc834fcc,
        0x34685c34, 0xa551f4a5, 0xe5d134e5, 0xf1f908f1,
        0x71e29371, 0xd8ab73d8, 0x31625331, 0x152a3f15,
        0x04080c04, 0xc79552c7, 0x23466523, 0xc39d5ec3,
        0x18302818, 0x9637a196, 0x050a0f05, 0x9a2fb59a,
        0x070e0907, 0x12243612, 0x801b9b80, 0xe2df3de2,
        0xebcd26eb, 0x274e6927, 0xb27fcdb2, 0x75ea9f75,
        0x09121b09, 0x831d9e83, 0x2c58742c, 0x1a342e1a,
        0x1b362d1b, 0x6edcb26e, 0x5ab4ee5a, 0xa05bfba0,
        0x52a4f652, 0x3b764d3b, 0xd6b761d6, 0xb37dceb3,
        0x29527b29, 0xe3dd3ee3, 0x2f5e712f, 0x84139784,
        0x53a6f553, 0xd1b968d1, 0x00000000, 0xedc12ced,
        0x20406020, 0xfce31ffc, 0xb179c8b1, 0x5bb6ed5b,
        0x6ad4be6a, 0xcb8d46cb, 0xbe67d9be, 0x39724b39,
        0x4a94de4a, 0x4c98d44c, 0x58b0e858, 0xcf854acf,
        0xd0bb6bd0, 0xefc52aef, 0xaa4fe5aa, 0xfbed16fb,
        0x4386c543, 0x4d9ad74d, 0x33665533, 0x85119485,
        0x458acf45, 0xf9e910f9, 0x02040602, 0x7ffe817f,
        0x50a0f050, 0x3c78443c, 0x9f25ba9f, 0xa84be3a8,
        0x51a2f351, 0xa35dfea3, 0x4080c040, 0x8f058a8f,
        0x923fad92, 0x9d21bc9d, 0x38704838, 0xf5f104f5,
        0xbc63dfbc, 0xb677c1b6, 0xdaaf75da, 0x21426321,
        0x10203010, 0xffe51aff, 0xf3fd0ef3, 0xd2bf6dd2,
        0xcd814ccd, 0x0c18140c, 0x13263513, 0xecc32fec,
        0x5fbee15f, 0x9735a297, 0x4488cc44, 0x172e3917,
        0xc49357c4, 0xa755f2a7, 0x7efc827e, 0x3d7a473d,
        0x64c8ac64, 0x5dbae75d, 0x19322b19, 0x73e69573,
        0x60c0a060, 0x81199881, 0x4f9ed14f, 0xdca37fdc,
        0x22446622, 0x2a547e2a, 0x903bab90, 0x880b8388,
        0x468cca46, 0xeec729ee, 0xb86bd3b8, 0x14283c14,
        0xdea779de, 0x5ebce25e, 0x0b161d0b, 0xdbad76db,
        0xe0db3be0, 0x32645632, 0x3a744e3a, 0x0a141e0a,
        0x4992db49, 0x060c0a06, 0x24486c24, 0x5cb8e45c,
        0xc29f5dc2, 0xd3bd6ed3, 0xac43efac, 0x62c4a662,
        0x9139a891, 0x9531a495, 0xe4d337e4, 0x79f28b79,
        0xe7d532e7, 0xc88b43c8, 0x376e5937, 0x6ddab76d,
        0x8d018c8d, 0xd5b164d5, 0x4e9cd24e, 0xa949e0a9,
        0x6cd8b46c, 0x56acfa56, 0xf4f307f4, 0xeacf25ea,
        0x65caaf65, 0x7af48e7a, 0xae47e9ae, 0x08101808,
        0xba6fd5ba, 0x78f08878, 0x254a6f25, 0x2e5c722e,
        0x1c38241c, 0xa657f1a6, 0xb473c7b4, 0xc69751c6,
        0xe8cb23e8, 0xdda17cdd, 0x74e89c74, 0x1f3e211f,
        0x4b96dd4b, 0xbd61dcbd, 0x8b0d868b, 0x8a0f858a,
        0x70e09070, 0x3e7c423e, 0xb571c4b5, 0x66ccaa66,
        0x4890d848, 0x03060503, 0xf6f701f6, 0x0e1c120e,
        0x61c2a361, 0x356a5f35, 0x57aef957, 0xb969d0b9,
        0x86179186, 0xc19958c1, 0x1d3a271d, 0x9e27b99e,
        0xe1d938e1, 0xf8eb13f8, 0x982bb398, 0x11223311,
        0x69d2bb69, 0xd9a970d9, 0x8e07898e, 0x9433a794,
        0x9b2db69b, 0x1e3c221e, 0x87159287, 0xe9c920e9,
        0xce8749ce, 0x55aaff55, 0x28507828, 0xdfa57adf,
        0x8c038f8c, 0xa159f8a1, 0x89098089, 0x0d1a170d,
        0xbf65dabf, 0xe6d731e6, 0x4284c642, 0x68d0b868,
        0x4182c341, 0x9929b099, 0x2d5a772d, 0x0f1e110f,
        0xb07bcbb0, 0x54a8fc54, 0xbb6dd6bb, 0x162c3a16,
    }, {
        0xc6a56363, 0xf8847c7c, 0xee997777, 0xf68d7b7b,
        0xff0df2f2, 0xd6bd6b6b, 0xdeb16f6f, 0x9154c5c5,
        0x60503030, 0x02030101, 0xcea96767, 0x567d2b2b,
        0xe719fefe, 0xb562d7d7, 0x4de6abab, 0xec9a7676,
        0x8f45caca, 0x1f9d8282, 0x8940c9c9, 0xfa877d7d,
        0xef15fafa, 0xb2eb5959, 0x8ec94747, 0xfb0bf0f0,
        0x41ecadad, 0xb367d4d4, 0x5ffda2a2, 0x45eaafaf,
        0x23bf9c9c, 0x53f7a4a4, 0xe4967272, 0x9b5bc0c0,
        0x75c2b7b7, 0xe11cfdfd, 0x3dae9393, 0x4c6a2626,
        0x6c5a3636, 0x7e413f3f, 0xf502f7f7, 0x834fcccc,
        0x685c3434, 0x51f4a5a5, 0xd134e5e5, 0xf908f1f1,
        0xe2937171, 0xab73d8d8, 0x62533131, 0x2a3f1515,
        0x080c0404, 0x9552c7c7, 0x46652323, 0x9d5ec3c3,
        0x30281818, 0x37a19696, 0x0a0f0505, 0x2fb59a9a,
        0x0e090707, 0x24361212, 0x1b9b8080, 0xdf3de2e2,
        0xcd26ebeb, 0x4e692727, 0x7fcdb2b2, 0xea9f7575,
        0x121b0909, 0x1d9e8383, 0x58742c2c, 0x342e1a1a,
        0x362d1b1b, 0xdcb26e6e, 0xb4ee5a5a, 0x5bfba0a0,
        0xa4f65252, 0x764d3b3b, 0xb761d6d6, 0x7dceb3b3,
        0x527b2929, 0xdd3ee3e3, 0x5e712f2f, 0x13978484,
        0xa6f55353, 0xb968d1d1, 0x00000000, 0xc12ceded,
        0x40602020, 0xe31ffcfc, 0x79c8b1b1, 0xb6ed5b5b,
        0xd4be6a6a, 0x8d46cbcb, 0x67d9bebe, 0x724b3939,
        0x94de4a4a, 0x98d44c4c, 0xb0e85858, 0x854acfcf,
        0xbb6bd0d0, 0xc52aefef, 0x4fe5aaaa, 0xed16fbfb,
        0x86c54343, 0x9ad74d4d, 0x66553333, 0x11948585,
        0x8acf4545, 0xe910f9f9, 0x04060202, 0xfe817f7f,
        0xa0f05050, 0x78443c3c, 0x25ba9f9f, 0x4be3a8a8,
        0xa2f35151, 0x5dfea3a3, 0x80c04040, 0x058a8f8f,
        0x3fad9292, 0x21bc9d9d, 0x70483838, 0xf104f5f5,
        0x63dfbcbc, 0x77c1b6b6, 0xaf75dada, 0x42632121,
        0x20301010, 0xe51affff, 0xfd0ef3f3, 0xbf6dd2d2,
        0x814ccdcd, 0x18140c0c, 0x26351313, 0xc32fecec,
        0xbee15f5f, 0x35a29797, 0x88cc4444, 0x2e391717,
        0x9357c4c4, 0x55f2a7a7, 0xfc827e7e, 0x7a473d3d,
        0xc8ac6464, 0xbae75d5d, 0x322b1919, 0xe6957373,
        0xc0a06060, 0x19988181, 0x9ed14f4f, 0xa37fdcdc,
        0x44662222, 0x547e2a2a, 0x3bab9090, 0x0b838888,
        0x8cca4646, 0xc729eeee, 0x6bd3b8b8, 0x283c1414,
        0xa779dede, 0xbce25e5e, 0x161d0b0b, 0xad76dbdb,
        0xdb3be0e0, 0x64563232, 0x744e3a3a, 0x141e0a0a,
        0x92db4949, 0x0c0a0606, 0x486c2424, 0xb8e45c5c,
        0x9f5dc2c2, 0xbd6ed3d3, 0x43efacac, 0xc4a66262,
        0x39a89191, 0x31a49595, 0xd337e4e4, 0xf28b7979,
        0xd532e7e7, 0x8b43c8c8, 0x6e593737, 0xdab76d6d,
        0x018c8d8d, 0xb164d5d5, 0x9cd24e4e, 0x49e0a9a9,
        0xd8b46c6c, 0xacfa5656, 0xf307f4f4, 0xcf25eaea,
        0xcaaf6565, 0xf48e7a7a, 0x47e9aeae, 0x10180808,
        0x6fd5baba, 0xf0887878, 0x4a6f2525, 0x5c722e2e,
        0x38241c1c, 0x57f1a6a6, 0x73c7b4b4, 0x9751c6c6,
        0xcb23e8e8, 0xa17cdddd, 0xe89c7474, 0x3e211f1f,
        0x96dd4b4b, 0x61dcbdbd, 0x0d868b8b, 0x0f858a8a,
        0xe0907070, 0x7c423e3e, 0x71c4b5b5, 0xccaa6666,
        0x90d84848, 0x06050303, 0xf701f6f6, 0x1c120e0e,
        0xc2a36161, 0x6a5f3535, 0xaef95757, 0x69d0b9b9,
        0x17918686, 0x9958c1c1, 0x3a271d1d, 0x27b99e9e,
        0xd938e1e1, 0xeb13f8f8, 0x2bb39898, 0x22331111,
        0xd2bb6969, 0xa970d9d9, 0x07898e8e, 0x33a79494,
        0x2db69b9b, 0x3c221e1e, 0x15928787, 0xc920e9e9,
        0x8749cece, 0xaaff5555, 0x50782828, 0xa57adfdf,
        0x038f8c8c, 0x59f8a1a1, 0x09808989, 0x1a170d0d,
        0x65dabfbf, 0xd731e6e6, 0x84c64242, 0xd0b86868,
        0x82c34141, 0x29b09999, 0x5a772d2d, 0x1e110f0f,
        0x7bcbb0b0, 0xa8fc5454, 0x6dd6bbbb, 0x2c3a1616,
    }
};

static const uint32_t crypto_fl_tab[4][256] = {
    {
        0x00000063, 0x0000007c, 0x00000077, 0x0000007b,
        0x000000f2, 0x0000006b, 0x0000006f, 0x000000c5,
        0x00000030, 0x00000001, 0x00000067, 0x0000002b,
        0x000000fe, 0x000000d7, 0x000000ab, 0x00000076,
        0x000000ca, 0x00000082, 0x000000c9, 0x0000007d,
        0x000000fa, 0x00000059, 0x00000047, 0x000000f0,
        0x000000ad, 0x000000d4, 0x000000a2, 0x000000af,
        0x0000009c, 0x000000a4, 0x00000072, 0x000000c0,
        0x000000b7, 0x000000fd, 0x00000093, 0x00000026,
        0x00000036, 0x0000003f, 0x000000f7, 0x000000cc,
        0x00000034, 0x000000a5, 0x000000e5, 0x000000f1,
        0x00000071, 0x000000d8, 0x00000031, 0x00000015,
        0x00000004, 0x000000c7, 0x00000023, 0x000000c3,
        0x00000018, 0x00000096, 0x00000005, 0x0000009a,
        0x00000007, 0x00000012, 0x00000080, 0x000000e2,
        0x000000eb, 0x00000027, 0x000000b2, 0x00000075,
        0x00000009, 0x00000083, 0x0000002c, 0x0000001a,
        0x0000001b, 0x0000006e, 0x0000005a, 0x000000a0,
        0x00000052, 0x0000003b, 0x000000d6, 0x000000b3,
        0x00000029, 0x000000e3, 0x0000002f, 0x00000084,
        0x00000053, 0x000000d1, 0x00000000, 0x000000ed,
        0x00000020, 0x000000fc, 0x000000b1, 0x0000005b,
        0x0000006a, 0x000000cb, 0x000000be, 0x00000039,
        0x0000004a, 0x0000004c, 0x00000058, 0x000000cf,
        0x000000d0, 0x000000ef, 0x000000aa, 0x000000fb,
        0x00000043, 0x0000004d, 0x00000033, 0x00000085,
        0x00000045, 0x000000f9, 0x00000002, 0x0000007f,
        0x00000050, 0x0000003c, 0x0000009f, 0x000000a8,
        0x00000051, 0x000000a3, 0x00000040, 0x0000008f,
        0x00000092, 0x0000009d, 0x00000038, 0x000000f5,
        0x000000bc, 0x000000b6, 0x000000da, 0x00000021,
        0x00000010, 0x000000ff, 0x000000f3, 0x000000d2,
        0x000000cd, 0x0000000c, 0x00000013, 0x000000ec,
        0x0000005f, 0x00000097, 0x00000044, 0x00000017,
        0x000000c4, 0x000000a7, 0x0000007e, 0x0000003d,
        0x00000064, 0x0000005d, 0x00000019, 0x00000073,
        0x00000060, 0x00000081, 0x0000004f, 0x000000dc,
        0x00000022, 0x0000002a, 0x00000090, 0x00000088,
        0x00000046, 0x000000ee, 0x000000b8, 0x00000014,
        0x000000de, 0x0000005e, 0x0000000b, 0x000000db,
        0x000000e0, 0x00000032, 0x0000003a, 0x0000000a,
        0x00000049, 0x00000006, 0x00000024, 0x0000005c,
        0x000000c2, 0x000000d3, 0x000000ac, 0x00000062,
        0x00000091, 0x00000095, 0x000000e4, 0x00000079,
        0x000000e7, 0x000000c8, 0x00000037, 0x0000006d,
        0x0000008d, 0x000000d5, 0x0000004e, 0x000000a9,
        0x0000006c, 0x00000056, 0x000000f4, 0x000000ea,
        0x00000065, 0x0000007a, 0x000000ae, 0x00000008,
        0x000000ba, 0x00000078, 0x00000025, 0x0000002e,
        0x0000001c, 0x000000a6, 0x000000b4, 0x000000c6,
        0x000000e8, 0x000000dd, 0x00000074, 0x0000001f,
        0x0000004b, 0x000000bd, 0x0000008b, 0x0000008a,
        0x00000070, 0x0000003e, 0x000000b5, 0x00000066,
        0x00000048, 0x00000003, 0x000000f6, 0x0000000e,
        0x00000061, 0x00000035, 0x00000057, 0x000000b9,
        0x00000086, 0x000000c1, 0x0000001d, 0x0000009e,
        0x000000e1, 0x000000f8, 0x00000098, 0x00000011,
        0x00000069, 0x000000d9, 0x0000008e, 0x00000094,
        0x0000009b, 0x0000001e, 0x00000087, 0x000000e9,
        0x000000ce, 0x00000055, 0x00000028, 0x000000df,
        0x0000008c, 0x000000a1, 0x00000089, 0x0000000d,
        0x000000bf, 0x000000e6, 0x00000042, 0x00000068,
        0x00000041, 0x00000099, 0x0000002d, 0x0000000f,
        0x000000b0, 0x00000054, 0x000000bb, 0x00000016,
    }, {
        0x00006300, 0x00007c00, 0x00007700, 0x00007b00,
        0x0000f200, 0x00006b00, 0x00006f00, 0x0000c500,
        0x00003000, 0x00000100, 0x00006700, 0x00002b00,
        0x0000fe00, 0x0000d700, 0x0000ab00, 0x00007600,
        0x0000ca00, 0x00008200, 0x0000c900, 0x00007d00,
        0x0000fa00, 0x00005900, 0x00004700, 0x0000f000,
        0x0000ad00, 0x0000d400, 0x0000a200, 0x0000af00,
        0x00009c00, 0x0000a400, 0x00007200, 0x0000c000,
        0x0000b700, 0x0000fd00, 0x00009300, 0x00002600,
        0x00003600, 0x00003f00, 0x0000f700, 0x0000cc00,
        0x00003400, 0x0000a500, 0x0000e500, 0x0000f100,
        0x00007100, 0x0000d800, 0x00003100, 0x00001500,
        0x00000400, 0x0000c700, 0x00002300, 0x0000c300,
        0x00001800, 0x00009600, 0x00000500, 0x00009a00,
        0x00000700, 0x00001200, 0x00008000, 0x0000e200,
        0x0000eb00, 0x00002700, 0x0000b200, 0x00007500,
        0x00000900, 0x00008300, 0x00002c00, 0x00001a00,
        0x00001b00, 0x00006e00, 0x00005a00, 0x0000a000,
        0x00005200, 0x00003b00, 0x0000d600, 0x0000b300,
        0x00002900, 0x0000e300, 0x00002f00, 0x00008400,
        0x00005300, 0x0000d100, 0x00000000, 0x0000ed00,
        0x00002000, 0x0000fc00, 0x0000b100, 0x00005b00,
        0x00006a00, 0x0000cb00, 0x0000be00, 0x00003900,
        0x00004a00, 0x00004c00, 0x00005800, 0x0000cf00,
        0x0000d000, 0x0000ef00, 0x0000aa00, 0x0000fb00,
        0x00004300, 0x00004d00, 0x00003300, 0x00008500,
        0x00004500, 0x0000f900, 0x00000200, 0x00007f00,
        0x00005000, 0x00003c00, 0x00009f00, 0x0000a800,
        0x00005100, 0x0000a300, 0x00004000, 0x00008f00,
        0x00009200, 0x00009d00, 0x00003800, 0x0000f500,
        0x0000bc00, 0x0000b600, 0x0000da00, 0x00002100,
        0x00001000, 0x0000ff00, 0x0000f300, 0x0000d200,
        0x0000cd00, 0x00000c00, 0x00001300, 0x0000ec00,
        0x00005f00, 0x00009700, 0x00004400, 0x00001700,
        0x0000c400, 0x0000a700, 0x00007e00, 0x00003d00,
        0x00006400, 0x00005d00, 0x00001900, 0x00007300,
        0x00006000, 0x00008100, 0x00004f00, 0x0000dc00,
        0x00002200, 0x00002a00, 0x00009000, 0x00008800,
        0x00004600, 0x0000ee00, 0x0000b800, 0x00001400,
        0x0000de00, 0x00005e00, 0x00000b00, 0x0000db00,
        0x0000e000, 0x00003200, 0x00003a00, 0x00000a00,
        0x00004900, 0x00000600, 0x00002400, 0x00005c00,
        0x0000c200, 0x0000d300, 0x0000ac00, 0x00006200,
        0x00009100, 0x00009500, 0x0000e400, 0x00007900,
        0x0000e700, 0x0000c800, 0x00003700, 0x00006d00,
        0x00008d00, 0x0000d500, 0x00004e00, 0x0000a900,
        0x00006c00, 0x00005600, 0x0000f400, 0x0000ea00,
        0x00006500, 0x00007a00, 0x0000ae00, 0x00000800,
        0x0000ba00, 0x00007800, 0x00002500, 0x00002e00,
        0x00001c00, 0x0000a600, 0x0000b400, 0x0000c600,
        0x0000e800, 0x0000dd00, 0x00007400, 0x00001f00,
        0x00004b00, 0x0000bd00, 0x00008b00, 0x00008a00,
        0x00007000, 0x00003e00, 0x0000b500, 0x00006600,
        0x00004800, 0x00000300, 0x0000f600, 0x00000e00,
        0x00006100, 0x00003500, 0x00005700, 0x0000b900,
        0x00008600, 0x0000c100, 0x00001d00, 0x00009e00,
        0x0000e100, 0x0000f800, 0x00009800, 0x00001100,
        0x00006900, 0x0000d900, 0x00008e00, 0x00009400,
        0x00009b00, 0x00001e00, 0x00008700, 0x0000e900,
        0x0000ce00, 0x00005500, 0x00002800, 0x0000df00,
        0x00008c00, 0x0000a100, 0x00008900, 0x00000d00,
        0x0000bf00, 0x0000e600, 0x00004200, 0x00006800,
        0x00004100, 0x00009900, 0x00002d00, 0x00000f00,
        0x0000b000, 0x00005400, 0x0000bb00, 0x00001600,
    }, {
        0x00630000, 0x007c0000, 0x00770000, 0x007b0000,
        0x00f20000, 0x006b0000, 0x006f0000, 0x00c50000,
        0x00300000, 0x00010000, 0x00670000, 0x002b0000,
        0x00fe0000, 0x00d70000, 0x00ab0000, 0x00760000,
        0x00ca0000, 0x00820000, 0x00c90000, 0x007d0000,
        0x00fa0000, 0x00590000, 0x00470000, 0x00f00000,
        0x00ad0000, 0x00d40000, 0x00a20000, 0x00af0000,
        0x009c0000, 0x00a40000, 0x00720000, 0x00c00000,
        0x00b70000, 0x00fd0000, 0x00930000, 0x00260000,
        0x00360000, 0x003f0000, 0x00f70000, 0x00cc0000,
        0x00340000, 0x00a50000, 0x00e50000, 0x00f10000,
        0x00710000, 0x00d80000, 0x00310000, 0x00150000,
        0x00040000, 0x00c70000, 0x00230000, 0x00c30000,
        0x00180000, 0x00960000, 0x00050000, 0x009a0000,
        0x00070000, 0x00120000, 0x00800000, 0x00e20000,
        0x00eb0000, 0x00270000, 0x00b20000, 0x00750000,
        0x00090000, 0x00830000, 0x002c0000, 0x001a0000,
        0x001b0000, 0x006e0000, 0x005a0000, 0x00a00000,
        0x00520000, 0x003b0000, 0x00d60000, 0x00b30000,
        0x00290000, 0x00e30000, 0x002f0000, 0x00840000,
        0x00530000, 0x00d10000, 0x00000000, 0x00ed0000,
        0x00200000, 0x00fc0000, 0x00b10000, 0x005b0000,
        0x006a0000, 0x00cb0000, 0x00be0000, 0x00390000,
        0x004a0000, 0x004c0000, 0x00580000, 0x00cf0000,
        0x00d00000, 0x00ef0000, 0x00aa0000, 0x00fb0000,
        0x00430000, 0x004d0000, 0x00330000, 0x00850000,
        0x00450000, 0x00f90000, 0x00020000, 0x007f0000,
        0x00500000, 0x003c0000, 0x009f0000, 0x00a80000,
        0x00510000, 0x00a30000, 0x00400000, 0x008f0000,
        0x00920000, 0x009d0000, 0x00380000, 0x00f50000,
        0x00bc0000, 0x00b60000, 0x00da0000, 0x00210000,
        0x00100000, 0x00ff0000, 0x00f30000, 0x00d20000,
        0x00cd0000, 0x000c0000, 0x00130000, 0x00ec0000,
        0x005f0000, 0x00970000, 0x00440000, 0x00170000,
        0x00c40000, 0x00a70000, 0x007e0000, 0x003d0000,
        0x00640000, 0x005d0000, 0x00190000, 0x00730000,
        0x00600000, 0x00810000, 0x004f0000, 0x00dc0000,
        0x00220000, 0x002a0000, 0x00900000, 0x00880000,
        0x00460000, 0x00ee0000, 0x00b80000, 0x00140000,
        0x00de0000, 0x005e0000, 0x000b0000, 0x00db0000,
        0x00e00000, 0x00320000, 0x003a0000, 0x000a0000,
        0x00490000, 0x00060000, 0x00240000, 0x005c0000,
        0x00c20000, 0x00d30000, 0x00ac0000, 0x00620000,
        0x00910000, 0x00950000, 0x00e40000, 0x00790000,
        0x00e70000, 0x00c80000, 0x00370000, 0x006d0000,
        0x008d0000, 0x00d50000, 0x004e0000, 0x00a90000,
        0x006c0000, 0x00560000, 0x00f40000, 0x00ea0000,
        0x00650000, 0x007a0000, 0x00ae0000, 0x00080000,
        0x00ba0000, 0x00780000, 0x00250000, 0x002e0000,
        0x001c0000, 0x00a60000, 0x00b40000, 0x00c60000,
        0x00e80000, 0x00dd0000, 0x00740000, 0x001f0000,
        0x004b0000, 0x00bd0000, 0x008b0000, 0x008a0000,
        0x00700000, 0x003e0000, 0x00b50000, 0x00660000,
        0x00480000, 0x00030000, 0x00f60000, 0x000e0000,
        0x00610000, 0x00350000, 0x00570000, 0x00b90000,
        0x00860000, 0x00c10000, 0x001d0000, 0x009e0000,
        0x00e10000, 0x00f80000, 0x00980000, 0x00110000,
        0x00690000, 0x00d90000, 0x008e0000, 0x00940000,
        0x009b0000, 0x001e0000, 0x00870000, 0x00e90000,
        0x00ce0000, 0x00550000, 0x00280000, 0x00df0000,
        0x008c0000, 0x00a10000, 0x00890000, 0x000d0000,
        0x00bf0000, 0x00e60000, 0x00420000, 0x00680000,
        0x00410000, 0x00990000, 0x002d0000, 0x000f0000,
        0x00b00000, 0x00540000, 0x00bb0000, 0x00160000,
    }, {
        0x63000000, 0x7c000000, 0x77000000, 0x7b000000,
        0xf2000000, 0x6b000000, 0x6f000000, 0xc5000000,
        0x30000000, 0x01000000, 0x67000000, 0x2b000000,
        0xfe000000, 0xd7000000, 0xab000000, 0x76000000,
        0xca000000, 0x82000000, 0xc9000000, 0x7d000000,
        0xfa000000, 0x59000000, 0x47000000, 0xf0000000,
        0xad000000, 0xd4000000, 0xa2000000, 0xaf000000,
        0x9c000000, 0xa4000000, 0x72000000, 0xc0000000,
        0xb7000000, 0xfd000000, 0x93000000, 0x26000000,
        0x36000000, 0x3f000000, 0xf7000000, 0xcc000000,
        0x34000000, 0xa5000000, 0xe5000000, 0xf1000000,
        0x71000000, 0xd8000000, 0x31000000, 0x15000000,
        0x04000000, 0xc7000000, 0x23000000, 0xc3000000,
        0x18000000, 0x96000000, 0x05000000, 0x9a000000,
        0x07000000, 0x12000000, 0x80000000, 0xe2000000,
        0xeb000000, 0x27000000, 0xb2000000, 0x75000000,
        0x09000000, 0x83000000, 0x2c000000, 0x1a000000,
        0x1b000000, 0x6e000000, 0x5a000000, 0xa0000000,
        0x52000000, 0x3b000000, 0xd6000000, 0xb3000000,
        0x29000000, 0xe3000000, 0x2f000000, 0x84000000,
        0x53000000, 0xd1000000, 0x00000000, 0xed000000,
        0x20000000, 0xfc000000, 0xb1000000, 0x5b000000,
        0x6a000000, 0xcb000000, 0xbe000000, 0x39000000,
        0x4a000000, 0x4c000000, 0x58000000, 0xcf000000,
        0xd0000000, 0xef000000, 0xaa000000, 0xfb000000,
        0x43000000, 0x4d000000, 0x33000000, 0x85000000,
        0x45000000, 0xf9000000, 0x02000000, 0x7f000000,
        0x50000000, 0x3c000000, 0x9f000000, 0xa8000000,
        0x51000000, 0xa3000000, 0x40000000, 0x8f000000,
        0x92000000, 0x9d000000, 0x38000000, 0xf5000000,
        0xbc000000, 0xb6000000, 0xda000000, 0x21000000,
        0x10000000, 0xff000000, 0xf3000000, 0xd2000000,
        0xcd000000, 0x0c000000, 0x13000000, 0xec000000,
        0x5f000000, 0x97000000, 0x44000000, 0x17000000,
        0xc4000000, 0xa7000000, 0x7e000000, 0x3d000000,
        0x64000000, 0x5d000000, 0x19000000, 0x73000000,
        0x60000000, 0x81000000, 0x4f000000, 0xdc000000,
        0x22000000, 0x2a000000, 0x90000000, 0x88000000,
        0x46000000, 0xee000000, 0xb8000000, 0x14000000,
        0xde000000, 0x5e000000, 0x0b000000, 0xdb000000,
        0xe0000000, 0x32000000, 0x3a000000, 0x0a000000,
        0x49000000, 0x06000000, 0x24000000, 0x5c000000,
        0xc2000000, 0xd3000000, 0xac000000, 0x62000000,
        0x91000000, 0x95000000, 0xe4000000, 0x79000000,
        0xe7000000, 0xc8000000, 0x37000000, 0x6d000000,
        0x8d000000, 0xd5000000, 0x4e000000, 0xa9000000,
        0x6c000000, 0x56000000, 0xf4000000, 0xea000000,
        0x65000000, 0x7a000000, 0xae000000, 0x08000000,
        0xba000000, 0x78000000, 0x25000000, 0x2e000000,
        0x1c000000, 0xa6000000, 0xb4000000, 0xc6000000,
        0xe8000000, 0xdd000000, 0x74000000, 0x1f000000,
        0x4b000000, 0xbd000000, 0x8b000000, 0x8a000000,
        0x70000000, 0x3e000000, 0xb5000000, 0x66000000,
        0x48000000, 0x03000000, 0xf6000000, 0x0e000000,
        0x61000000, 0x35000000, 0x57000000, 0xb9000000,
        0x86000000, 0xc1000000, 0x1d000000, 0x9e000000,
        0xe1000000, 0xf8000000, 0x98000000, 0x11000000,
        0x69000000, 0xd9000000, 0x8e000000, 0x94000000,
        0x9b000000, 0x1e000000, 0x87000000, 0xe9000000,
        0xce000000, 0x55000000, 0x28000000, 0xdf000000,
        0x8c000000, 0xa1000000, 0x89000000, 0x0d000000,
        0xbf000000, 0xe6000000, 0x42000000, 0x68000000,
        0x41000000, 0x99000000, 0x2d000000, 0x0f000000,
        0xb0000000, 0x54000000, 0xbb000000, 0x16000000,
    }
};

/* initialise the key schedule from the user supplied key */

static uint32_t aes_ror32(uint32_t word, unsigned int shift)
{
    return (word >> shift) | (word << (32 - shift));
}

#define cpu_to_le32(x) SDL_SwapLE32(x)
#define le32_to_cpu(x) SDL_SwapLE32(x)

#define star_x(x) (((x) & 0x7f7f7f7f) << 1) ^ ((((x) & 0x80808080) >> 7) * 0x1b)

#define imix_col(y, x)  do {        \
    u       = star_x(x);        \
    v       = star_x(u);        \
    w       = star_x(v);        \
    t       = w ^ (x);          \
    (y)     = u ^ v ^ w;        \
    (y)     ^= aes_ror32(u ^ t, 8) ^    \
        aes_ror32(v ^ t, 16) ^      \
        aes_ror32(t, 24);       \
} while (0)

#define ls_box(x)           \
    crypto_fl_tab[0][get_byte(x, 0)] ^  \
    crypto_fl_tab[1][get_byte(x, 1)] ^  \
    crypto_fl_tab[2][get_byte(x, 2)] ^  \
    crypto_fl_tab[3][get_byte(x, 3)]

#define loop4(i)    do {        \
    t = aes_ror32(t, 8);        \
    t = ls_box(t) ^ rco_tab[i];     \
    t ^= ctx->key_enc[4 * i];           \
    ctx->key_enc[4 * i + 4] = t;        \
    t ^= ctx->key_enc[4 * i + 1];       \
    ctx->key_enc[4 * i + 5] = t;        \
    t ^= ctx->key_enc[4 * i + 2];       \
    ctx->key_enc[4 * i + 6] = t;        \
    t ^= ctx->key_enc[4 * i + 3];       \
    ctx->key_enc[4 * i + 7] = t;        \
} while (0)

#define loop6(i)    do {        \
    t = aes_ror32(t, 8);        \
    t = ls_box(t) ^ rco_tab[i];     \
    t ^= ctx->key_enc[6 * i];           \
    ctx->key_enc[6 * i + 6] = t;        \
    t ^= ctx->key_enc[6 * i + 1];       \
    ctx->key_enc[6 * i + 7] = t;        \
    t ^= ctx->key_enc[6 * i + 2];       \
    ctx->key_enc[6 * i + 8] = t;        \
    t ^= ctx->key_enc[6 * i + 3];       \
    ctx->key_enc[6 * i + 9] = t;        \
    t ^= ctx->key_enc[6 * i + 4];       \
    ctx->key_enc[6 * i + 10] = t;       \
    t ^= ctx->key_enc[6 * i + 5];       \
    ctx->key_enc[6 * i + 11] = t;       \
} while (0)

#define loop8tophalf(i) do {            \
    t = aes_ror32(t, 8);            \
    t = ls_box(t) ^ rco_tab[i];         \
    t ^= ctx->key_enc[8 * i];               \
    ctx->key_enc[8 * i + 8] = t;            \
    t ^= ctx->key_enc[8 * i + 1];           \
    ctx->key_enc[8 * i + 9] = t;            \
    t ^= ctx->key_enc[8 * i + 2];           \
    ctx->key_enc[8 * i + 10] = t;           \
    t ^= ctx->key_enc[8 * i + 3];           \
    ctx->key_enc[8 * i + 11] = t;           \
} while (0)

#define loop8(i)    do {                \
    loop8tophalf(i);                \
    t  = ctx->key_enc[8 * i + 4] ^ ls_box(t);       \
    ctx->key_enc[8 * i + 12] = t;           \
    t ^= ctx->key_enc[8 * i + 5];           \
    ctx->key_enc[8 * i + 13] = t;           \
    t ^= ctx->key_enc[8 * i + 6];           \
    ctx->key_enc[8 * i + 14] = t;           \
    t ^= ctx->key_enc[8 * i + 7];           \
    ctx->key_enc[8 * i + 15] = t;           \
} while (0)

/**
 * AES_ExpandKey - Expands the AES key as described in FIPS-197
 * @ctx:    The location where the computed key will be stored.
 * @in_key:     The supplied key.
 * @key_len:    The length of the supplied key.
 *
 * Returns 0 on success. The function fails only if an invalid key size (or
 * pointer) is supplied.
 * The expanded key size is 240 bytes (max of 14 rounds with a unique 16 bytes
 * key schedule plus a 16 bytes key which is used before the first round).
 * The decryption key is prepared for the "Equivalent Inverse Cipher" as
 * described in FIPS-197. The first slot (16 bytes) of each key (enc or dec) is
 * for the initial combination, the second slot for the first round and so on.
 */
static int AES_ExpandKey(aes_context_t *ctx, const uint8_t *in_key,
                         unsigned int key_len)
{
    const uint32_t *key = (const uint32_t *)in_key;
    uint32_t i, t, u, v, w, j;

    if (key_len != AES_KEYSIZE_128 && key_len != AES_KEYSIZE_192 &&
        key_len != AES_KEYSIZE_256)
        return -1;

    ctx->key_length = key_len;

    ctx->key_dec[key_len + 24] = ctx->key_enc[0] = le32_to_cpu(key[0]);
    ctx->key_dec[key_len + 25] = ctx->key_enc[1] = le32_to_cpu(key[1]);
    ctx->key_dec[key_len + 26] = ctx->key_enc[2] = le32_to_cpu(key[2]);
    ctx->key_dec[key_len + 27] = ctx->key_enc[3] = le32_to_cpu(key[3]);

    switch (key_len) {
        case AES_KEYSIZE_128:
            t = ctx->key_enc[3];
            for (i = 0; i < 10; ++i)
                loop4(i);
            break;

        case AES_KEYSIZE_192:
            ctx->key_enc[4] = le32_to_cpu(key[4]);
            t = ctx->key_enc[5] = le32_to_cpu(key[5]);
            for (i = 0; i < 8; ++i)
                loop6(i);
            break;

        case AES_KEYSIZE_256:
            ctx->key_enc[4] = le32_to_cpu(key[4]);
            ctx->key_enc[5] = le32_to_cpu(key[5]);
            ctx->key_enc[6] = le32_to_cpu(key[6]);
            t = ctx->key_enc[7] = le32_to_cpu(key[7]);
            for (i = 0; i < 6; ++i)
                loop8(i);
            loop8tophalf(i);
            break;
    }

    ctx->key_dec[0] = ctx->key_enc[key_len + 24];
    ctx->key_dec[1] = ctx->key_enc[key_len + 25];
    ctx->key_dec[2] = ctx->key_enc[key_len + 26];
    ctx->key_dec[3] = ctx->key_enc[key_len + 27];

    for (i = 4; i < key_len + 24; ++i) {
        j = key_len + 24 - (i & ~3) + (i & 3);
        imix_col(ctx->key_dec[j], ctx->key_enc[i]);
    }
    return 0;
}

/**
 * AES_SetKey - Set the AES key.
 * @ctx:        AES context struct.
 * @in_key:     The input key.
 * @key_len:    The size of the key.
 *
 * Returns 0 on success, on failure -1 is returned.
 * The function uses AES_ExpandKey() to expand the key.
 */
static int AES_SetKey(aes_context_t *ctx, const uint8_t *in_key,
                      unsigned int key_len)
{
    int ret;

    ret = AES_ExpandKey(ctx, in_key, key_len);
    if (!ret)
        return 0;

    return -1;
}

/* encrypt a block of text */

#define f_rn(bo, bi, n, k)      do {                \
    bo[n] = crypto_ft_tab[0][get_byte(bi[n], 0)] ^              \
        crypto_ft_tab[1][get_byte(bi[(n + 1) & 3], 1)] ^        \
        crypto_ft_tab[2][get_byte(bi[(n + 2) & 3], 2)] ^        \
        crypto_ft_tab[3][get_byte(bi[(n + 3) & 3], 3)] ^ *(k + n);  \
} while (0)

#define f_nround(bo, bi, k)     do {\
    f_rn(bo, bi, 0, k);     \
    f_rn(bo, bi, 1, k);     \
    f_rn(bo, bi, 2, k);     \
    f_rn(bo, bi, 3, k);     \
    k += 4;         \
} while (0)

#define f_rl(bo, bi, n, k)      do {                \
    bo[n] = crypto_fl_tab[0][get_byte(bi[n], 0)] ^              \
        crypto_fl_tab[1][get_byte(bi[(n + 1) & 3], 1)] ^        \
        crypto_fl_tab[2][get_byte(bi[(n + 2) & 3], 2)] ^        \
        crypto_fl_tab[3][get_byte(bi[(n + 3) & 3], 3)] ^ *(k + n);  \
} while (0)

#define f_lround(bo, bi, k)     do {\
    f_rl(bo, bi, 0, k);     \
    f_rl(bo, bi, 1, k);     \
    f_rl(bo, bi, 2, k);     \
    f_rl(bo, bi, 3, k);     \
} while (0)

static void AES_Encrypt(aes_context_t *ctx, uint8_t *out,
                        const uint8_t *in)
{
    const uint32_t *src = (const uint32_t *)in;
    uint32_t *dst = (uint32_t *)out;
    uint32_t b0[4], b1[4];
    const uint32_t *kp = ctx->key_enc + 4;
    const int key_len = ctx->key_length;

    b0[0] = le32_to_cpu(src[0]) ^ ctx->key_enc[0];
    b0[1] = le32_to_cpu(src[1]) ^ ctx->key_enc[1];
    b0[2] = le32_to_cpu(src[2]) ^ ctx->key_enc[2];
    b0[3] = le32_to_cpu(src[3]) ^ ctx->key_enc[3];

    if (key_len > 24) {
        f_nround(b1, b0, kp);
        f_nround(b0, b1, kp);
    }

    if (key_len > 16) {
        f_nround(b1, b0, kp);
        f_nround(b0, b1, kp);
    }

    f_nround(b1, b0, kp);
    f_nround(b0, b1, kp);
    f_nround(b1, b0, kp);
    f_nround(b0, b1, kp);
    f_nround(b1, b0, kp);
    f_nround(b0, b1, kp);
    f_nround(b1, b0, kp);
    f_nround(b0, b1, kp);
    f_nround(b1, b0, kp);
    f_lround(b0, b1, kp);

    dst[0] = cpu_to_le32(b0[0]);
    dst[1] = cpu_to_le32(b0[1]);
    dst[2] = cpu_to_le32(b0[2]);
    dst[3] = cpu_to_le32(b0[3]);
}

static boolean prng_enabled = false;
static aes_context_t prng_context;
static uint32_t prng_input_counter;
static uint32_t prng_values[4];
static unsigned int prng_value_index = 0;

// Initialize Pseudo-RNG using the specified 128-bit key.

void PRNG_Start(const prng_seed_t key)
{
    AES_SetKey(&prng_context, key, sizeof(prng_seed_t));
    prng_value_index = 4;
    prng_input_counter = 0;
    prng_enabled = true;
}

void PRNG_Stop(void)
{
    prng_enabled = false;
}

// Generate a set of new PRNG values by encrypting a new block.

static void PRNG_Generate(void)
{
    byte input[16], output[16];
    unsigned int i;

    // Input for the cipher is a consecutively increasing 32-bit counter.

    for (i = 0; i < 4; ++i)
    {
        input[4*i] = prng_input_counter & 0xff;
        input[4*i + 1] = (prng_input_counter >> 8) & 0xff;
        input[4*i + 2] = (prng_input_counter >> 16) & 0xff;
        input[4*i + 3] = (prng_input_counter >> 24) & 0xff;
        ++prng_input_counter;
    }

    AES_Encrypt(&prng_context, output, input);

    for (i = 0; i < 4; ++i)
    {
        prng_values[i] = output[4*i]
                       | (output[4*i + 1] << 8)
                       | (output[4*i + 2] << 16)
                       | (output[4*i + 3] << 24);
    }

    prng_value_index = 0;
}

// Read a random 32-bit integer from the PRNG.

unsigned int PRNG_Random(void)
{
    unsigned int result;

    if (!prng_enabled)
    {
        return 0;
    }

    if (prng_value_index >= 4)
    {
        PRNG_Generate();
    }

    result = prng_values[prng_value_index];
    ++prng_value_index;

    return result;
}

