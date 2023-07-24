/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 *the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
 * PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
 * THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
 * OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <typedef.h>

#include <stdio.h>
#include <string.h>

#include "../clk.h"
#include "../clk_periph.h"
#include "../clk_factors.h"
#include "clk_sun8iw21.h"

static int get_factors_pll_cpu(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor);

static int get_factors_pll_ddr0(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor);

static int get_factors_pll_periph0x2(u32 rate, u32 parent_rate,
                                   struct clk_factors_value *factor);

static int get_factors_pll_periph0800m(u32 rate, u32 parent_rate,
                                   struct clk_factors_value *factor);

static int get_factors_pll_periph0480m(u32 rate, u32 parent_rate,
                                   struct clk_factors_value *factor);

static int calc_rate_pll_cpu(u32 parent_rate,
                             struct clk_factors_value *factor);

static int calc_rate_pll_ddr0(u32 parent_rate,
                             struct clk_factors_value *factor);

static int calc_rate_pll_periph0x2(u32 parent_rate,
                                 struct clk_factors_value *factor);

static int calc_rate_pll_periph0800m(u32 parent_rate,
                                 struct clk_factors_value *factor);

static int calc_rate_pll_periph0480m(u32 parent_rate,
                                 struct clk_factors_value *factor);

static int get_factors_pll_video0x4(u32 rate, u32 parent_rate,
                                 struct clk_factors_value *factor);

static int calc_rate_pll_video0x4(u32 parent_rate,
                               struct clk_factors_value *factor);

static int get_factors_pll_csix4(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor);

static int calc_rate_pll_csix4(u32 parent_rate,
                             struct clk_factors_value *factor);

static int get_factors_pll_audio_div2(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor);

static int calc_rate_pll_audio_div2(u32 parent_rate,
                             struct clk_factors_value *factor);

static int get_factors_pll_audio_div5(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor);

static int calc_rate_pll_audio_div5(u32 parent_rate,
                             struct clk_factors_value *factor);

static int get_factors_pll_npux4(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor);

static int calc_rate_pll_npux4(u32 parent_rate,
                             struct clk_factors_value *factor);

/* PLLCPU(n, m, p, freq)	F_N8X8_M0X2_P16x2 */
struct sunxi_clk_factor_freq factor_pllcpu_tbl[] =
{
    PLLCPU(66,     2,     402000000U),
    PLLCPU(16,     0,     408000000U),
    PLLCPU(68,     2,     414000000U),
    PLLCPU(34,     1,     420000000U),
    PLLCPU(70,     2,     426000000U),
    PLLCPU(17,     0,     432000000U),
    PLLCPU(72,     2,     438000000U),
    PLLCPU(36,     1,     444000000U),
    PLLCPU(74,     2,     450000000U),
    PLLCPU(18,     0,     456000000U),
    PLLCPU(76,     2,     462000000U),
    PLLCPU(38,     1,     468000000U),
    PLLCPU(78,     2,     474000000U),
    PLLCPU(19,     0,     480000000U),
    PLLCPU(80,     2,     486000000U),
    PLLCPU(40,     1,     492000000U),
    PLLCPU(82,     2,     498000000U),
    PLLCPU(20,     0,     504000000U),
    PLLCPU(84,     2,     510000000U),
    PLLCPU(42,     1,     516000000U),
    PLLCPU(86,     2,     522000000U),
    PLLCPU(21,     0,     528000000U),
    PLLCPU(88,     2,     534000000U),
    PLLCPU(44,     1,     540000000U),
    PLLCPU(90,     2,     546000000U),
    PLLCPU(22,     0,     552000000U),
    PLLCPU(92,     2,     558000000U),
    PLLCPU(46,     1,     564000000U),
    PLLCPU(94,     2,     570000000U),
    PLLCPU(23,     0,     576000000U),
    PLLCPU(96,     2,     582000000U),
    PLLCPU(48,     1,     588000000U),
    PLLCPU(98,     2,     594000000U),
    PLLCPU(24,     0,     600000000U),
    PLLCPU(100,    2,     606000000U),
    PLLCPU(50,     1,     612000000U),
    PLLCPU(102,    2,     618000000U),
    PLLCPU(25,     0,     624000000U),
    PLLCPU(104,    2,     630000000U),
    PLLCPU(52,     1,     636000000U),
    PLLCPU(106,    2,     642000000U),
    PLLCPU(26,     0,     648000000U),
    PLLCPU(108,    2,     654000000U),
    PLLCPU(54,     1,     660000000U),
    PLLCPU(110,    2,     666000000U),
    PLLCPU(27,     0,     672000000U),
    PLLCPU(112,    2,     678000000U),
    PLLCPU(56,     1,     684000000U),
    PLLCPU(114,    2,     690000000U),
    PLLCPU(28,     0,     696000000U),
    PLLCPU(116,    2,     702000000U),
    PLLCPU(58,     1,     708000000U),
    PLLCPU(118,    2,     714000000U),
    PLLCPU(29,     0,     720000000U),
    PLLCPU(120,    2,     726000000U),
    PLLCPU(60,     1,     732000000U),
    PLLCPU(122,    2,     738000000U),
    PLLCPU(30,     0,     744000000U),
    PLLCPU(124,    2,     750000000U),
    PLLCPU(62,     1,     756000000U),
    PLLCPU(126,    2,     762000000U),
    PLLCPU(31,     0,     768000000U),
    PLLCPU(128,    2,     774000000U),
    PLLCPU(64,     1,     780000000U),
    PLLCPU(130,    2,     786000000U),
    PLLCPU(32,     0,     792000000U),
    PLLCPU(132,    2,     798000000U),
    PLLCPU(66,     1,     804000000U),
    PLLCPU(134,    2,     810000000U),
    PLLCPU(33,     0,     816000000U),
    PLLCPU(136,    2,     822000000U),
    PLLCPU(68,     1,     828000000U),
    PLLCPU(138,    2,     834000000U),
    PLLCPU(34,     0,     840000000U),
    PLLCPU(140,    2,     846000000U),
    PLLCPU(70,     1,     852000000U),
    PLLCPU(142,    2,     858000000U),
    PLLCPU(35,     0,     864000000U),
    PLLCPU(144,    2,     870000000U),
    PLLCPU(72,     1,     876000000U),
    PLLCPU(146,    2,     882000000U),
    PLLCPU(36,     0,     888000000U),
    PLLCPU(148,    2,     894000000U),
    PLLCPU(74,     1,     900000000U),
    PLLCPU(150,    2,     906000000U),
    PLLCPU(37,     0,     912000000U),
    PLLCPU(152,    2,     918000000U),
    PLLCPU(76,     1,     924000000U),
    PLLCPU(154,    2,     930000000U),
    PLLCPU(38,     0,     936000000U),
    PLLCPU(156,    2,     942000000U),
    PLLCPU(78,     1,     948000000U),
    PLLCPU(158,    2,     954000000U),
    PLLCPU(39,     0,     960000000U),
    PLLCPU(160,    2,     966000000U),
    PLLCPU(80,     1,     972000000U),
    PLLCPU(162,    2,     978000000U),
    PLLCPU(40,     0,     984000000U),
    PLLCPU(164,    2,     990000000U),
    PLLCPU(82,     1,     996000000U),
    PLLCPU(166,    2,     1002000000U),
    PLLCPU(41,     0,     1008000000U),
    PLLCPU(168,    2,     1014000000U),
    PLLCPU(84,     1,     1020000000U),
    PLLCPU(170,    2,     1026000000U),
    PLLCPU(42,     0,     1032000000U),
    PLLCPU(172,     2,     1038000000U),
    PLLCPU(86,     1,     1044000000U),
    PLLCPU(174,     2,     1050000000U),
    PLLCPU(43,     0,     1056000000U),
    PLLCPU(176,     2,     1062000000U),
    PLLCPU(88,     1,     1068000000U),
    PLLCPU(178,     2,     1074000000U),
    PLLCPU(44,     0,     1080000000U),
    PLLCPU(180,     2,     1086000000U),
    PLLCPU(90,     1,     1092000000U),
    PLLCPU(182,     2,     1098000000U),
    PLLCPU(45,     0,     1104000000U),
    PLLCPU(184,     2,     1110000000U),
    PLLCPU(92,     1,     1116000000U),
    PLLCPU(186,     2,     1122000000U),
    PLLCPU(46,     0,      1128000000U),
    PLLCPU(188,     2,     1134000000U),
    PLLCPU(94,     1,     1140000000U),
    PLLCPU(190,     2,     1146000000U),
    PLLCPU(47,     0,      1152000000U),
    PLLCPU(192,     2,     1158000000U),
    PLLCPU(96,     1,     1164000000U),
    PLLCPU(194,     2,     1170000000U),
    PLLCPU(48,     0,     1176000000U),
    PLLCPU(196,     2,     1182000000U),
    PLLCPU(98,     1,     1188000000U),
    PLLCPU(198,     2,     1194000000U),
    PLLCPU(49,      0,     1200000000U),
    PLLCPU(200,     2,     1206000000U),
    PLLCPU(100,     1,     1212000000U),
    PLLCPU(202,     2,     1218000000U),
    PLLCPU(50,      0,     1224000000U),
    PLLCPU(204,     2,     1230000000U),
    PLLCPU(102,     1,     1236000000U),
    PLLCPU(206,     2,     1242000000U),
    PLLCPU(51,      0,     1248000000U),
    PLLCPU(208,     2,     1254000000U),
    PLLCPU(104,     1,     1260000000U),
    PLLCPU(52,      0,     1272000000U),
    PLLCPU(106,     1,     1284000000U),
    PLLCPU(53,      0,     1296000000U),
};

/* PLLDDR(n, d1, d2, freq)  F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllddr0_tbl[] =
{
    PLLDDR(23,     0,     1,     288000000U),
    PLLDDR(48,     1,     1,     294000000U),
    PLLDDR(24,     0,     1,     300000000U),
    PLLDDR(50,     1,     1,     306000000U),
    PLLDDR(25,     0,     1,     312000000U),
    PLLDDR(52,     1,     1,     318000000U),
    PLLDDR(26,     0,     1,     324000000U),
    PLLDDR(54,     1,     1,     330000000U),
    PLLDDR(27,     0,     1,     336000000U),
    PLLDDR(56,     1,     1,     342000000U),
    PLLDDR(28,     0,     1,     348000000U),
    PLLDDR(58,     1,     1,     354000000U),
    PLLDDR(29,     0,     1,     360000000U),
    PLLDDR(60,     1,     1,     366000000U),
    PLLDDR(30,     0,     1,     372000000U),
    PLLDDR(62,     1,     1,     378000000U),
    PLLDDR(31,     0,     1,     384000000U),
    PLLDDR(64,     1,     1,     390000000U),
    PLLDDR(32,     0,     1,     396000000U),
    PLLDDR(66,     1,     1,     402000000U),
    PLLDDR(33,     0,     1,     408000000U),
    PLLDDR(68,     1,     1,     414000000U),
    PLLDDR(34,     0,     1,     420000000U),
    PLLDDR(70,     1,     1,     426000000U),
    PLLDDR(17,     0,     0,     432000000U),
    PLLDDR(72,     1,     1,     438000000U),
    PLLDDR(36,     0,     1,     444000000U),
    PLLDDR(74,     1,     1,     450000000U),
    PLLDDR(18,     0,     0,     456000000U),
    PLLDDR(76,     1,     1,     462000000U),
    PLLDDR(38,     0,     1,     468000000U),
    PLLDDR(78,     1,     1,     474000000U),
    PLLDDR(19,     0,     0,     480000000U),
    PLLDDR(80,     1,     1,     486000000U),
    PLLDDR(40,     0,     1,     492000000U),
    PLLDDR(82,     1,     1,     498000000U),
    PLLDDR(20,     0,     0,     504000000U),
    PLLDDR(84,     1,     1,     510000000U),
    PLLDDR(42,     0,     1,     516000000U),
    PLLDDR(86,     1,     1,     522000000U),
    PLLDDR(21,     0,     0,     528000000U),
    PLLDDR(88,     1,     1,     534000000U),
    PLLDDR(44,     0,     1,     540000000U),
    PLLDDR(90,     1,     1,     546000000U),
    PLLDDR(22,     0,     0,     552000000U),
    PLLDDR(92,     1,     1,     558000000U),
    PLLDDR(46,     0,     1,     564000000U),
    PLLDDR(94,     1,     1,     570000000U),
    PLLDDR(23,     0,     0,     576000000U),
    PLLDDR(96,     1,     1,     582000000U),
    PLLDDR(48,     0,     1,     588000000U),
    PLLDDR(98,     1,     1,     594000000U),
    PLLDDR(24,     0,     0,     600000000U),
    PLLDDR(100,     1,     1,     606000000U),
    PLLDDR(50,     0,     1,     612000000U),
    PLLDDR(102,     1,     1,     618000000U),
    PLLDDR(25,     0,     0,     624000000U),
    PLLDDR(104,     1,     1,     630000000U),
    PLLDDR(52,     0,     1,     636000000U),
    PLLDDR(106,     1,     1,     642000000U),
    PLLDDR(26,     0,     0,     648000000U),
    PLLDDR(108,     1,     1,     654000000U),
    PLLDDR(54,     0,     1,     660000000U),
    PLLDDR(110,     1,     1,     666000000U),
    PLLDDR(27,     0,     0,     672000000U),
    PLLDDR(112,     1,     1,     678000000U),
    PLLDDR(56,     0,     1,     684000000U),
    PLLDDR(114,     1,     1,     690000000U),
    PLLDDR(28,     0,     0,     696000000U),
    PLLDDR(116,     1,     1,     702000000U),
    PLLDDR(58,     0,     1,     708000000U),
    PLLDDR(118,     1,     1,     714000000U),
    PLLDDR(29,     0,     0,     720000000U),
    PLLDDR(120,     1,     1,     726000000U),
    PLLDDR(60,     0,     1,     732000000U),
    PLLDDR(122,     1,     1,     738000000U),
    PLLDDR(30,     0,     0,     744000000U),
    PLLDDR(124,     1,     1,     750000000U),
    PLLDDR(62,     0,     1,     756000000U),
    PLLDDR(126,     1,     1,     762000000U),
    PLLDDR(31,     0,     0,     768000000U),
    PLLDDR(128,     1,     1,     774000000U),
    PLLDDR(64,     0,     1,     780000000U),
    PLLDDR(130,     1,     1,     786000000U),
    PLLDDR(32,     0,     0,     792000000U),
    PLLDDR(132,     1,     1,     798000000U),
    PLLDDR(66,     0,     1,     804000000U),
    PLLDDR(134,     1,     1,     810000000U),
    PLLDDR(33,     0,     0,     816000000U),
    PLLDDR(136,     1,     1,     822000000U),
    PLLDDR(68,     0,     1,     828000000U),
    PLLDDR(138,     1,     1,     834000000U),
    PLLDDR(34,     0,     0,     840000000U),
    PLLDDR(140,     1,     1,     846000000U),
    PLLDDR(70,     0,     1,     852000000U),
    PLLDDR(142,     1,     1,     858000000U),
    PLLDDR(35,     0,     0,     864000000U),
    PLLDDR(144,     1,     1,     870000000U),
    PLLDDR(72,     0,     1,     876000000U),
    PLLDDR(146,     1,     1,     882000000U),
    PLLDDR(36,     0,     0,     888000000U),
    PLLDDR(148,     1,     1,     894000000U),
    PLLDDR(74,     0,     1,     900000000U),
    PLLDDR(150,     1,     1,     906000000U),
    PLLDDR(37,     0,     0,     912000000U),
    PLLDDR(152,     1,     1,     918000000U),
    PLLDDR(76,     0,     1,     924000000U),
    PLLDDR(154,     1,     1,     930000000U),
    PLLDDR(38,     0,     0,     936000000U),
    PLLDDR(156,     1,     1,     942000000U),
    PLLDDR(78,     0,     1,     948000000U),
    PLLDDR(158,     1,     1,     954000000U),
    PLLDDR(39,     0,     0,     960000000U),
    PLLDDR(160,     1,     1,     966000000U),
    PLLDDR(80,     0,     1,     972000000U),
    PLLDDR(162,     1,     1,     978000000U),
    PLLDDR(40,     0,     0,     984000000U),
    PLLDDR(164,     1,     1,     990000000U),
    PLLDDR(82,     0,     1,     996000000U),
    PLLDDR(166,     1,     1,     1002000000U),
    PLLDDR(41,     0,     0,     1008000000U),
    PLLDDR(168,     1,     1,     1014000000U),
    PLLDDR(84,     0,     1,     1020000000U),
    PLLDDR(170,     1,     1,     1026000000U),
    PLLDDR(42,     0,     0,     1032000000U),
    PLLDDR(172,     1,     1,     1038000000U),
    PLLDDR(86,     0,     1,     1044000000U),
    PLLDDR(174,     1,     1,     1050000000U),
    PLLDDR(43,     0,     0,     1056000000U),
    PLLDDR(176,     1,     1,     1062000000U),
    PLLDDR(88,     0,     1,     1068000000U),
    PLLDDR(178,     1,     1,     1074000000U),
    PLLDDR(44,     0,     0,     1080000000U),
    PLLDDR(180,     1,     1,     1086000000U),
    PLLDDR(90,     0,     1,     1092000000U),
    PLLDDR(182,     1,     1,     1098000000U),
    PLLDDR(45,     0,     0,     1104000000U),
    PLLDDR(184,     1,     1,     1110000000U),
    PLLDDR(92,     0,     1,     1116000000U),
    PLLDDR(186,     1,     1,     1122000000U),
    PLLDDR(46,     0,     0,     1128000000U),
    PLLDDR(188,     1,     1,     1134000000U),
    PLLDDR(94,     0,     1,     1140000000U),
    PLLDDR(190,     1,     1,     1146000000U),
    PLLDDR(47,     0,     0,     1152000000U),
    PLLDDR(192,     1,     1,     1158000000U),
    PLLDDR(96,     0,     1,     1164000000U),
    PLLDDR(194,     1,     1,     1170000000U),
    PLLDDR(48,     0,     0,     1176000000U),
    PLLDDR(196,     1,     1,     1182000000U),
    PLLDDR(98,     0,     1,     1188000000U),
    PLLDDR(198,     1,     1,     1194000000U),
    PLLDDR(49,     0,     0,     1200000000U),
    PLLDDR(200,     1,     1,     1206000000U),
    PLLDDR(100,     0,     1,     1212000000U),
    PLLDDR(202,     1,     1,     1218000000U),
    PLLDDR(50,     0,     0,     1224000000U),
    PLLDDR(204,     1,     1,     1230000000U),
    PLLDDR(102,     0,     1,     1236000000U),
    PLLDDR(206,     1,     1,     1242000000U),
    PLLDDR(51,     0,     0,     1248000000U),
    PLLDDR(208,     1,     1,     1254000000U),
    PLLDDR(104,     0,     1,     1260000000U),
    PLLDDR(52,     0,     0,     1272000000U),
    PLLDDR(106,     0,     1,     1284000000U),
    PLLDDR(53,     0,     0,     1296000000U),
    PLLDDR(108,     0,     1,     1308000000U),
    PLLDDR(54,     0,     0,     1320000000U),
    PLLDDR(110,     0,     1,     1332000000U),
    PLLDDR(55,     0,     0,     1344000000U),
    PLLDDR(112,     0,     1,     1356000000U),
    PLLDDR(56,     0,     0,     1368000000U),
    PLLDDR(114,     0,     1,     1380000000U),
    PLLDDR(57,     0,     0,     1392000000U),
    PLLDDR(116,     0,     1,     1404000000U),
    PLLDDR(58,     0,     0,     1416000000U),
    PLLDDR(118,     0,     1,     1428000000U),
    PLLDDR(59,     0,     0,     1440000000U),
    PLLDDR(120,     0,     1,     1452000000U),
    PLLDDR(60,     0,     0,     1464000000U),
    PLLDDR(122,     0,     1,     1476000000U),
    PLLDDR(61,     0,     0,     1488000000U),
    PLLDDR(124,     0,     1,     1500000000U),
    PLLDDR(62,     0,     0,     1512000000U),
    PLLDDR(126,     0,     1,     1524000000U),
    PLLDDR(63,     0,     0,     1536000000U),
    PLLDDR(128,     0,     1,     1548000000U),
    PLLDDR(64,     0,     0,     1560000000U),
    PLLDDR(130,     0,     1,     1572000000U),
    PLLDDR(65,     0,     0,     1584000000U),
    PLLDDR(132,     1,     0,     1596000000U),
    PLLDDR(66,     0,     0,     1608000000U),
    PLLDDR(134,     1,     0,     1620000000U),
    PLLDDR(67,     0,     0,     1632000000U),
    PLLDDR(136,     1,     0,     1644000000U),
    PLLDDR(68,     0,     0,     1656000000U),
    PLLDDR(138,     1,     0,     1668000000U),
    PLLDDR(69,     0,     0,     1680000000U),
    PLLDDR(140,     1,     0,     1692000000U),
    PLLDDR(70,     0,     0,     1704000000U),
    PLLDDR(142,     1,     0,     1716000000U),
    PLLDDR(71,     0,     0,     1728000000U),
    PLLDDR(144,     1,     0,     1740000000U),
    PLLDDR(72,     0,     0,     1752000000U),
    PLLDDR(146,     1,     0,     1764000000U),
    PLLDDR(73,     0,     0,     1776000000U),
    PLLDDR(148,     1,     0,     1788000000U),
    PLLDDR(74,     0,     0,     1800000000U),
    PLLDDR(150,     1,     0,     1812000000U),
    PLLDDR(75,     0,     0,     1824000000U),
    PLLDDR(152,     1,     0,     1836000000U),
    PLLDDR(76,     0,     0,     1848000000U),
    PLLDDR(154,     1,     0,     1860000000U),
    PLLDDR(77,     0,     0,     1872000000U),
    PLLDDR(156,     1,     0,     1884000000U),
    PLLDDR(78,     0,     0,     1896000000U),
    PLLDDR(158,     1,     0,     1908000000U),
    PLLDDR(79,     0,     0,     1920000000U),
    PLLDDR(160,     1,     0,     1932000000U),
    PLLDDR(80,     0,     0,     1944000000U),
    PLLDDR(162,     1,     0,     1956000000U),
    PLLDDR(81,     0,     0,     1968000000U),
    PLLDDR(164,     1,     0,     1980000000U),
    PLLDDR(82,     0,     0,     1992000000U),
    PLLDDR(166,     1,     0,     2004000000U),
    PLLDDR(83,     0,     0,     2016000000U),
    PLLDDR(168,     1,     0,     2028000000U),
    PLLDDR(84,     0,     0,     2040000000U),
    PLLDDR(170,     1,     0,     2052000000U),
    PLLDDR(85,     0,     0,     2064000000U),
    PLLDDR(172,     1,     0,     2076000000U),
    PLLDDR(86,     0,     0,     2088000000U),
    PLLDDR(174,     1,     0,     2100000000U),
    PLLDDR(87,     0,     0,     2112000000U),
    PLLDDR(176,     1,     0,     2124000000U),
    PLLDDR(88,     0,     0,     2136000000U),
    PLLDDR(178,     1,     0,     2148000000U),
    PLLDDR(89,     0,     0,     2160000000U),
    PLLDDR(180,     1,     0,     2172000000U),
    PLLDDR(90,     0,     0,     2184000000U),
    PLLDDR(182,     1,     0,     2196000000U),
    PLLDDR(91,     0,     0,     2208000000U),
    PLLDDR(184,     1,     0,     2220000000U),
    PLLDDR(92,     0,     0,     2232000000U),
    PLLDDR(186,     1,     0,     2244000000U),
    PLLDDR(93,     0,     0,     2256000000U),
    PLLDDR(188,     1,     0,     2268000000U),
    PLLDDR(94,     0,     0,     2280000000U),
    PLLDDR(190,     1,     0,     2292000000U),
    PLLDDR(95,     0,     0,     2304000000U),
    PLLDDR(192,     1,     0,     2316000000U),
    PLLDDR(96,     0,     0,     2328000000U),
    PLLDDR(194,     1,     0,     2340000000U),
    PLLDDR(97,     0,     0,     2352000000U),
    PLLDDR(196,     1,     0,     2364000000U),
    PLLDDR(98,     0,     0,     2376000000U),
    PLLDDR(198,     1,     0,     2388000000U),
    PLLDDR(99,     0,     0,     2400000000U),
    PLLDDR(200,     1,     0,     2412000000U),
    PLLDDR(100,     0,     0,     2424000000U),
    PLLDDR(202,     1,     0,     2436000000U),
    PLLDDR(101,     0,     0,     2448000000U),
    PLLDDR(204,     1,     0,     2460000000U),
    PLLDDR(102,     0,     0,     2472000000U),
    PLLDDR(206,     1,     0,     2484000000U),
    PLLDDR(103,     0,     0,     2496000000U),
    PLLDDR(208,     1,     0,     2508000000U),
    PLLDDR(104,     0,     0,     2520000000U),
};

/* PLLPERIPH0(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
/* periph0 periph1 use this table */
struct sunxi_clk_factor_freq factor_pllperiph0x2_tbl[] = {
PLLPERIPH0(99,     0,     1,     1200000000U),
};

struct sunxi_clk_factor_freq factor_pllperiph0800m_tbl[] = {
PLLPERIPH0(99,     0,     2,     1200000000U),
};

struct sunxi_clk_factor_freq factor_pllperiph0480m_tbl[] = {
PLLPERIPH0(99,     0,     4,     1200000000U),
};

/* PLLVIDEO0(n, d1, freq)	F_N8X8_D1V1X1 */
struct sunxi_clk_factor_freq factor_pllvideo0x4_tbl[] = {
PLLVIDEO0(98,     1,     1188000000U),
};

/* PLL_CSI(n, d1, freq)	F_N8X8_D1V1X1 */
struct sunxi_clk_factor_freq factor_pllcsix4_tbl[] = {
PLLCSI(98,     1,     1188000000U),
PLLCSI(98,     0,     2376000000U),
};

/* PLLAUDIO(n, p, d1, d2, freq)	F_N8X8_P16X6_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllaudio_tbl[] = {
PLLAUDIO(11,    0,    0,    0,    288000000U),
};

/* PLL_NPU(n, d1, freq)	F_N8X8_D1V1X1 */
struct sunxi_clk_factor_freq factor_pllnpux4_tbl[] = {
PLLNPU(41,     0,     1188000000U),
};

#define FACTOR_SIZEOF(name) (sizeof(factor_pll##name##_tbl)/ \
                             sizeof(struct sunxi_clk_factor_freq))

#define FACTOR_SEARCH(name) (sunxi_clk_com_ftr_sr( \
                             &sunxi_clk_factor_config_pll_##name, factor, \
                             factor_pll##name##_tbl, index, \
                             FACTOR_SIZEOF(name)))

/**These clock belong to fixed clock in sun8iw21p1-clk.dtsi**/
SUNXI_CLK_FIXED_SRC(ext_losc,  HAL_CLK_SRC_EXTLOSC,  HAL_CLK_SRC_ROOT, HAL_CLK_ROOT, 32768U, 0);           /* defined as RTC_32K in spec*/
SUNXI_CLK_FIXED_SRC(iosc,  HAL_CLK_SRC_IOSC,  HAL_CLK_SRC_ROOT, HAL_CLK_ROOT, 16000000U, 0);     /* defined as RC61M in spec*/
SUNXI_CLK_FIXED_SRC(hosc,  HAL_CLK_SRC_HOSC,  HAL_CLK_SRC_ROOT, HAL_CLK_ROOT, 24000000U, 0);     /* defined as OSC24M in spec*/
SUNXI_CLK_FIXED_SRC(hoscdiv32k,  HAL_CLK_SRC_HOSCDIV32K,  HAL_CLK_SRC_ROOT, HAL_CLK_ROOT, 32768U, 0);     /* defined as OSC24M in spec*/
SUNXI_CLK_FIXED_SRC(osc48m, HAL_CLK_SRC_OSC48M, HAL_CLK_SRC_ROOT, HAL_CLK_ROOT, 48000000U, 0);

/**These clock belong to fixed factor clock in sun8iw21p1-clk.dtsi**/
SUNXI_CLK_FIXED_FACTOR(iosc32k, HAL_CLK_PLL_IOSC32K, HAL_CLK_SRC_IOSC, HAL_CLK_FACTOR, 1, 500);
SUNXI_CLK_FIXED_FACTOR(pll_periph0600m, HAL_CLK_PLL_PERI0600M, HAL_CLK_PLL_PERI0X2, HAL_CLK_FACTOR, 1, 2);
SUNXI_CLK_FIXED_FACTOR(pll_periph0400m, HAL_CLK_PLL_PERI0400M, HAL_CLK_PLL_PERI0X2, HAL_CLK_FACTOR, 1, 3);
SUNXI_CLK_FIXED_FACTOR(pll_periph0300m, HAL_CLK_PLL_PERI0300M, HAL_CLK_PLL_PERI0600M, HAL_CLK_FACTOR, 1, 2);
SUNXI_CLK_FIXED_FACTOR(pll_periph0200m, HAL_CLK_PLL_PERI0200M, HAL_CLK_PLL_PERI0400M, HAL_CLK_FACTOR, 1, 2);
SUNXI_CLK_FIXED_FACTOR(pll_periph0160m, HAL_CLK_PLL_PERI0160M, HAL_CLK_PLL_PERI0480M, HAL_CLK_FACTOR, 1, 3);
SUNXI_CLK_FIXED_FACTOR(pll_periph0150m, HAL_CLK_PLL_PERI0150M, HAL_CLK_PLL_PERI0300M, HAL_CLK_FACTOR, 1, 2);
SUNXI_CLK_FIXED_FACTOR(pll_periph0150m_div6, HAL_CLK_PLL_PERI0150MDIV6, HAL_CLK_PLL_PERI0150M, HAL_CLK_FACTOR, 1, 6);
SUNXI_CLK_FIXED_FACTOR(pll_periph0160m_div10, HAL_CLK_PLL_PERI0160MDIV10, HAL_CLK_PLL_PERI0160M, HAL_CLK_FACTOR, 1, 10);
SUNXI_CLK_FIXED_FACTOR(pll_video0x2, HAL_CLK_PLL_VIDEO0X2, HAL_CLK_PLL_VIDEOX4, HAL_CLK_FACTOR, 1, 2);
SUNXI_CLK_FIXED_FACTOR(pll_video0, HAL_CLK_PLL_VIDEO0, HAL_CLK_PLL_VIDEOX4, HAL_CLK_FACTOR, 1, 4);
SUNXI_CLK_FIXED_FACTOR(pll_npux2, HAL_CLK_PLL_NPUX2, HAL_CLK_PLL_NPUX4, HAL_CLK_FACTOR, 1, 2);
SUNXI_CLK_FIXED_FACTOR(pll_npu, HAL_CLK_PLL_NPU, HAL_CLK_PLL_NPUX4, HAL_CLK_FACTOR, 1, 4);
SUNXI_CLK_FIXED_FACTOR(hoscd2, HAL_CLK_HOSCD2, HAL_CLK_SRC_HOSC, HAL_CLK_FACTOR, 1, 2);
SUNXI_CLK_FIXED_FACTOR(osc48md4, HAL_CLK_OSC48MD4, HAL_CLK_SRC_OSC48M, HAL_CLK_FACTOR, 1, 4);
SUNXI_CLK_FIXED_FACTOR(sdramd4, HAL_CLK_SDRAMD4, HAL_CLK_PERIPH_SDRAM, HAL_CLK_FACTOR, 1, 4);
SUNXI_CLK_FIXED_FACTOR(hosc_div32, HAL_HOSC_DIV32, HAL_CLK_SRC_HOSC, HAL_CLK_FACTOR, 1, 32);
SUNXI_CLK_FIXED_FACTOR(hosc_div16, HAL_HOSC_DIV16, HAL_CLK_SRC_HOSC, HAL_CLK_FACTOR, 1, 16);
SUNXI_CLK_FIXED_FACTOR(hosc_div8, HAL_HOSC_DIV8, HAL_CLK_SRC_HOSC, HAL_CLK_FACTOR, 1, 8);
SUNXI_CLK_FIXED_FACTOR(hosc_div4, HAL_HOSC_DIV4, HAL_CLK_SRC_HOSC, HAL_CLK_FACTOR, 1, 4);
SUNXI_CLK_FIXED_FACTOR(hosc_div2, HAL_HOSC_DIV2, HAL_CLK_SRC_HOSC, HAL_CLK_FACTOR, 1, 2);


/*                       name,           ns  nw  ks  kw  ms  mw  ps  pw  d1s d1w d2s d2w {frac   out mode}   en-s    sdmss   sdmsw   sdmpat             sdmval  mux_in-s out_en-s*/
SUNXI_CLK_FACTORS_CONFIG(pll_cpu,		8,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,    0,    0,  0,      31,     0,      0,      0,              0,                   23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_ddr0,		8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  1,    0,    0,  0,      31,     24,     1,      CLK_PLL_DDRPAT,    0xd1303333,       23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_periph0x2,	8,  8,  0,  0,  1,  1,  0,  0,  16, 3,  0,  0,    0,    0,  0,      31,     24,     0,      CLK_PLL_PERI0PAT0,  0xd1303333,      23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_periph0800m,	8,  8,  0,  0,  1,  1,  0,  0,  20, 3,  0,  0,    0,    0,  0,      31,     24,     1,      CLK_PLL_PERI0PAT0,  0xd1303333,  23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_periph0480m,	8,  8,  0,  0,  1,  1,  0,  0,  2,  3,  0,  0,    0,    0,  0,      31,     24,     1,      CLK_PLL_PERI0PAT0,  0xd1303333,  23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_video0x4,		8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  0,    0,    0,  0,      31,     24,     1,      CLK_PLL_VIDEO0PAT0, 0xd1303333,  23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_csix4,		8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  0,  0,    0,    0,  0,      31,     24,     1,      CLK_PLL_CSIPAT0,    0xd1303333,      23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_audio_div2,	8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  16, 3,    0,    0,  0,      31,     24,     1,      CLK_PLL_AUDIOPAT0,  0xd1303333,  23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_audio_div5,	8,  8,  0,  0,  0,  0,  0,  0,  1,  1,  20, 3,    0,    0,  0,      31,     24,     1,      CLK_PLL_AUDIOPAT0,  0xd1303333,  23,  27);
SUNXI_CLK_FACTORS_CONFIG(pll_npux4,		8,  8,  0,  0,  1,  1,  0,  0,  0,  0,  0,  0,    0,    0,  0,      31,     24,     1,      CLK_PLL_NPUPAT0,    0xd1303333,      23,  27);

/* name        reg              lock_reg    lock_bit pll_lock_ctrl_reg lock_en_bit */
SUNXI_CLK_FACTORS_INIT(pll_cpu,      CLK_PLL_CPU,     CLK_PLL_CPU,     28,  CLK_PLL_CPU,     29);
SUNXI_CLK_FACTORS_INIT(pll_ddr0,     CLK_PLL_DDR,     CLK_PLL_DDR,     28,  CLK_PLL_DDR,     29);
SUNXI_CLK_FACTORS_INIT(pll_periph0x2,  CLK_PLL_PERIPH0, CLK_PLL_PERIPH0, 28,  CLK_PLL_PERIPH0, 29);
SUNXI_CLK_FACTORS_INIT(pll_periph0800m,  CLK_PLL_PERIPH0, CLK_PLL_PERIPH0, 28,  CLK_PLL_PERIPH0, 29);
SUNXI_CLK_FACTORS_INIT(pll_periph0480m,  CLK_PLL_PERIPH0, CLK_PLL_PERIPH0, 28,  CLK_PLL_PERIPH0, 29);
SUNXI_CLK_FACTORS_INIT(pll_video0x4,    CLK_PLL_VIDEO0,  CLK_PLL_VIDEO0,  28,  CLK_PLL_VIDEO0,  29);
SUNXI_CLK_FACTORS_INIT(pll_csix4,    CLK_PLL_CSI,   CLK_PLL_CSI,   28,  CLK_PLL_CSI,   29);
SUNXI_CLK_FACTORS_INIT(pll_audio_div2,    CLK_PLL_AUDIO,   CLK_PLL_AUDIO,   28,  CLK_PLL_AUDIO,   29);
SUNXI_CLK_FACTORS_INIT(pll_audio_div5,    CLK_PLL_AUDIO,   CLK_PLL_AUDIO,   28,  CLK_PLL_AUDIO,   29);
SUNXI_CLK_FACTORS_INIT(pll_npux4,      CLK_PLL_NPU,     CLK_PLL_NPU,     28,  CLK_PLL_CSI,     29);

SUNXI_CLK_FACTOR(pll_cpu, HAL_CLK_PLL_CPUX_C0, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 1296000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_ddr0, HAL_CLK_PLL_DDR0, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 2520000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_periph0x2, HAL_CLK_PLL_PERI0X2, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 1200000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_periph0800m, HAL_CLK_PLL_PERI0800M, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 1200000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_periph0480m, HAL_CLK_PLL_PERI0480M, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 1200000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_video0x4, HAL_CLK_PLL_VIDEOX4, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 1188000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_csix4, HAL_CLK_PLL_CSIX4, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 1188000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_audio_div2, HAL_CLK_PLL_AUDIODIV2, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 288000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_audio_div5, HAL_CLK_PLL_AUDIODIV5, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 288000000U, 24000000U);
SUNXI_CLK_FACTOR(pll_npux4, HAL_CLK_PLL_NPUX4, HAL_CLK_SRC_HOSC, HAL_CLK_FIXED_SRC, 1188000000U, 24000000U);

hal_clk_id_t hosc_parents[] = {HAL_CLK_SRC_HOSC};
hal_clk_id_t losc_parents[] = {HAL_CLK_SRC_LOSC};
hal_clk_id_t cpu_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_SRC_LOSC, HAL_CLK_SRC_IOSC, HAL_CLK_PLL_CPUX_C0, HAL_CLK_PLL_PERI0600M, HAL_CLK_PLL_PERI0800M};
hal_clk_id_t cpu_apb_axi_parents[] = {HAL_CLK_BUS_C0_CPU};
hal_clk_id_t ahb_apb_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_SRC_LOSC, HAL_CLK_SRC_IOSC, HAL_CLK_PLL_PERI0600M};
hal_clk_id_t mbus_parents[] = {HAL_CLK_SDRAMD4};
hal_clk_id_t de_parents[] = {HAL_CLK_PLL_PERI0300M, HAL_CLK_PLL_VIDEO0};
hal_clk_id_t ce_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_PLL_PERI0400M, HAL_CLK_PLL_PERI0300M};
hal_clk_id_t ve_parents[] = {HAL_CLK_PLL_PERI0300M, HAL_CLK_PLL_PERI0400M, HAL_CLK_PLL_PERI0480M, HAL_CLK_PLL_NPUX4, HAL_CLK_PLL_VIDEOX4, HAL_CLK_PLL_CSIX4};
hal_clk_id_t npu_parents[] = {HAL_CLK_PLL_PERI0480M, HAL_CLK_PLL_PERI0600M, HAL_CLK_PLL_PERI0800M, HAL_CLK_PLL_NPUX4};

hal_clk_id_t sdram_parents[] = {HAL_CLK_PLL_DDR0, HAL_CLK_PLL_PERI0X2, HAL_CLK_PLL_PERI0800M};
hal_clk_id_t smhc01_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_PLL_PERI0400M, HAL_CLK_PLL_PERI0300M};
hal_clk_id_t smhc2_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_PLL_PERI0600M, HAL_CLK_PLL_PERI0400M};
hal_clk_id_t spi_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_PLL_PERI0300M, HAL_CLK_PLL_PERI0200M};
hal_clk_id_t spif_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_PLL_PERI0400M, HAL_CLK_PLL_PERI0300M};
hal_clk_id_t gmac_25m_parents[] = {HAL_CLK_PLL_PERI0150MDIV6};
hal_clk_id_t pll_audiox4_parents[] = {HAL_CLK_PLL_AUDIODIV2};
hal_clk_id_t pll_audio_parents[] = {HAL_CLK_PLL_AUDIODIV5};
hal_clk_id_t audio_parents[] = {HAL_CLK_PLL_AUDIO, HAL_CLK_PLL_AUDIOX4};
hal_clk_id_t usbohci_12m_parents[] = {HAL_CLK_SRC_OSC48MD4, HAL_CLK_HOSCD2, HAL_CLK_SRC_LOSC};
hal_clk_id_t dsi_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_PLL_PERI0200M, HAL_CLK_PLL_PERI0150M};
hal_clk_id_t tcon_lcd_parents[] = {HAL_CLK_PLL_VIDEOX4, HAL_CLK_PLL_PERI0X2, HAL_CLK_PLL_CSIX4};
hal_clk_id_t csi_top_parents[] = {HAL_CLK_PLL_PERI0300M, HAL_CLK_PLL_PERI0400M, HAL_CLK_PLL_VIDEOX4, HAL_CLK_PLL_CSIX4};
hal_clk_id_t csi_master_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_PLL_CSIX4, HAL_CLK_PLL_VIDEOX4, HAL_CLK_PLL_PERI0X2};
hal_clk_id_t isp_parents[] = {HAL_CLK_PLL_PERI0300M, HAL_CLK_PLL_PERI0200M, HAL_CLK_PLL_VIDEOX4, HAL_CLK_PLL_CSIX4};
hal_clk_id_t dspo_parents[] = {HAL_CLK_PLL_VIDEO, HAL_CLK_PLL_VIDEOX4, HAL_CLK_PLL_PERI0X2, HAL_CLK_PLL_PERI0X2, HAL_CLK_PLL_PERI1, HAL_CLK_PLL_PERI1X2, HAL_CLK_PLL_CSI};
hal_clk_id_t e907_parents[] = {HAL_CLK_SRC_HOSC, HAL_CLK_SRC_LOSC, HAL_CLK_SRC_IOSC, HAL_CLK_PLL_PERI0600M, HAL_CLK_PLL_PERI0480M, HAL_CLK_PLL_CPUX_C0};
hal_clk_id_t e907_axi_parents[] = {HAL_CLK_PERIPH_E907};
hal_clk_id_t fanout25m_parents[] = {HAL_CLK_PLL_PERI0150MDIV6};
hal_clk_id_t fanout16m_parents[] = {HAL_CLK_PLL_PERI0160MDIV10};
hal_clk_id_t fanout12m_parents[] = {HAL_CLK_HOSCD2};
hal_clk_id_t fanout24m_parents[] = {HAL_CLK_SRC_HOSC};
hal_clk_id_t fanout27m_parents[] = {HAL_CLK_PLL_AUDIO, HAL_CLK_PLL_CSI, HAL_CLK_PLL_PERI0300M};
hal_clk_id_t fanout_pclk_parents[] = {HAL_CLK_BUS_APB0};
hal_clk_id_t gpadc_parents[] = {HAL_HOSC_DIV32, HAL_HOSC_DIV16, HAL_HOSC_DIV8, HAL_HOSC_DIV4, HAL_HOSC_DIV2, HAL_CLK_SRC_HOSC};
hal_clk_id_t fanout012_parents[] = {HAL_CLK_SRC_LOSC_OUT, HAL_CLK_PERIPH_FANOUT_12M, HAL_CLK_PERIPH_FANOUT_16M, HAL_CLK_PERIPH_FANOUT_24M, HAL_CLK_PERIPH_FANOUT_25M, HAL_CLK_PERIPH_FANOUT_27M, HAL_CLK_PERIPH_FANOUT_PCLK};

hal_clk_id_t ahbmod_parents[] = {HAL_CLK_BUS_AHB};
hal_clk_id_t apb0mod_parents[] = {HAL_CLK_BUS_APB0};
hal_clk_id_t apb1mod_parents[] = {HAL_CLK_BUS_APB1};

struct sunxi_clk_comgate com_gates[] =
{
    {"csi",     0, 0x1ff, BUS_GATE_SHARE | RST_GATE_SHARE | MBUS_GATE_SHARE, 0},
    {"codec",   0, 0x3, BUS_GATE_SHARE | RST_GATE_SHARE, 0},
    {"e907",    0, 0x3, BUS_GATE_SHARE | RST_GATE_SHARE, 0},
};

/*
SUNXI_CLK_PERIPH_CONFIG(name,           mux_reg,         mux_sft, mux_wid,      div_reg,            div_msft,  div_mwid,   div_nsft,   div_nwid,   gate_flag,  en_reg,          rst_reg,         bus_gate_reg,  drm_gate_reg,  en_sft,     rst_sft,    bus_gate_sft,   dram_gate_sft,  com_gate,         com_gate_off)
*/
SUNXI_CLK_PERIPH_CONFIG(cpu,            CLK_CPU_CFG,     24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(axi,            0,               0,       0,            CLK_CPU_CFG,        0,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(cpuapb,         0,               0,       0,            CLK_CPU_CFG,        8,         2,          0,          0,          0,          0,               0,               0,             0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(ahb,            CLK_AHB_CFG,     24,      2,            CLK_AHB_CFG,        0,         5,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(apb0,           CLK_APB0_CFG,    24,      2,            CLK_APB0_CFG,       0,         5,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(apb1,           CLK_APB1_CFG,    24,      2,            CLK_APB1_CFG,       0,         5,          8,          2,          0,          0,               0,               0,             0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(mbus,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_MBUS_CFG,    0,             0,             0,          30,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(de,             CLK_DE_CFG,      24,      1,            CLK_DE_CFG,         0,         5,          0,          0,          0,          CLK_DE_CFG,      CLK_DE_GATE,     CLK_DE_GATE,   0,             31,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(g2d,            CLK_G2D_CFG,     24,      1,            CLK_G2D_CFG,        0,         5,          0,          0,          0,          CLK_G2D_CFG,     CLK_G2D_GATE,    CLK_G2D_GATE,  CLK_MBUS_GATE, 31,         16,         0,              10,             NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(ce,             CLK_CE_CFG,      24,      2,            CLK_CE_CFG,         0,         4,          0,          0,          0,          CLK_CE_CFG,      CLK_CE_GATE,     CLK_CE_GATE,   CLK_MBUS_GATE, 31,         16,         0,              2,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(ve,             CLK_VE_CFG,      24,      3,            CLK_VE_CFG,         0,         5,          0,          0,          0,          CLK_VE_CFG,      CLK_VE_GATE,     CLK_VE_GATE,   CLK_MBUS_GATE, 31,         16,         0,              1,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(npu,            CLK_NPU_CFG,     24,      3,            CLK_NPU_CFG,        0,         5,          0,          0,          0,          CLK_NPU_CFG,     CLK_NPU_GATE,    CLK_NPU_GATE,  0,             21,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(dma,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_DMA_GATE,    CLK_DMA_GATE,  CLK_MBUS_GATE, 0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(msgbox0,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_MSGBOX_GATE, CLK_MSGBOX_GATE, 0,           0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(msgbox1,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_MSGBOX_GATE, CLK_MSGBOX_GATE, 0,           0,          17,         1,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(spinlock,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_SPINLOCK_GATE, CLK_SPINLOCK_GATE, 0,       0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(hstimer,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_HSTIMER_GATE, CLK_HSTIMER_GATE, 0,         0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(avs,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          CLK_AVS_CFG,     0,               0,             0,             31,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(dbgsys,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_DBGSYS_GATE, CLK_DBGSYS_GATE, 0,           0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(pwm,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_PWM_GATE,    CLK_PWM_GATE,  0,             0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(iommu,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               CLK_IOMMU_GATE, 0,            0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdram,          CLK_DRAM_CFG,    24,      3,            CLK_DRAM_CFG,       0,         5,          8,          2,          0,          CLK_DRAM_CFG,    CLK_DRAM_GATE,   CLK_DRAM_GATE, 0,             31,         16,         0,              0,              NULL,             0);

SUNXI_CLK_PERIPH_CONFIG(sdmmc0_mod,     CLK_SMHC0_CFG,   24,      3,            CLK_SMHC0_CFG,      0,         4,          8,          2,          0,          CLK_SMHC0_CFG,   0,              0,              0,             31,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc0_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_SMHC_GATE,  0,              0,             0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc0_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              CLK_SMHC_GATE,  0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc1_mod,     CLK_SMHC1_CFG,   24,      3,            CLK_SMHC1_CFG,      0,         4,          8,          2,          0,          CLK_SMHC1_CFG,   0,              0,              0,             31,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc1_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_SMHC_GATE,  0,              0,             0,          17,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc1_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              CLK_SMHC_GATE,  0,             0,          0,          1,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc2_mod,     CLK_SMHC2_CFG,   24,      3,            CLK_SMHC2_CFG,      0,         4,          8,          2,          0,          CLK_SMHC2_CFG,   0,              0,              0,             31,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc2_rst,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_SMHC_GATE,  0,              0,             0,          18,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(sdmmc2_bus,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,              CLK_SMHC_GATE,  0,             0,          0,          2,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(uart0,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_UART_GATE,  CLK_UART_GATE,  0,             0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(uart1,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_UART_GATE,  CLK_UART_GATE,  0,             0,          17,         1,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(uart2,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_UART_GATE,  CLK_UART_GATE,  0,             0,          18,         2,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(uart3,          0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_UART_GATE,  CLK_UART_GATE,  0,             0,          19,         3,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(twi0,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_TWI_GATE,   CLK_TWI_GATE,   0,             0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(twi1,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_TWI_GATE,   CLK_TWI_GATE,   0,             0,          17,         1,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(twi2,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_TWI_GATE,   CLK_TWI_GATE,   0,             0,          18,         2,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(twi3,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_TWI_GATE,   CLK_TWI_GATE,   0,             0,          19,         3,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(twi4,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_TWI_GATE,   CLK_TWI_GATE,   0,             0,          20,         4,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(spi0,           CLK_SPI0_CFG,    24,      3,            CLK_SPI0_CFG,       0,         4,          8,          2,          0,          CLK_SPI0_CFG,    CLK_SPI_GATE,   CLK_SPI_GATE,   0,             31,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(spi1,           CLK_SPI1_CFG,    24,      3,            CLK_SPI1_CFG,       0,         4,          8,          2,          0,          CLK_SPI1_CFG,    CLK_SPI_GATE,   CLK_SPI_GATE,   0,             31,         17,         1,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(spi2,           CLK_SPI2_CFG,    24,      3,            CLK_SPI2_CFG,       0,         4,          8,          2,          0,          CLK_SPI2_CFG,    CLK_SPI_GATE,   CLK_SPI_GATE,   0,             31,         18,         2,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(spi3,           CLK_SPI3_CFG,    24,      3,            CLK_SPI3_CFG,       0,         4,          8,          2,          0,          CLK_SPI3_CFG,    CLK_SPI_GATE,   CLK_SPI_GATE,   0,             31,         19,         3,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(spif,           CLK_SPIF_CFG,    24,      3,            CLK_SPIF_CFG,       0,         4,          8,          2,          0,          CLK_SPI3_CFG,    CLK_SPI_GATE,   CLK_SPI_GATE,   0,             31,         20,         4,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(gmac_25m,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          CLK_GMAC25M_CFG, 0,              CLK_GMAC25M_CFG, 0,            31,         0,          30,             0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(gmac,           0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_GMAC_GATE,  CLK_GMAC_GATE,  0,             0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(gpadc,          CLK_GPADC_SEL,   22,      3,            0,                  0,         0,          0,          0,          0,          0,               CLK_GPADC_GATE, CLK_GPADC_GATE, 0,             0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(ths,            0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_THS_GATE,   CLK_THS_GATE,   0,             0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(pll_audiox4,    0,               0,       0,            CLK_PLL_PRE_CFG,    5,         5,          0,          0,          0,          0,               0,              0,              0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(pll_audio,      0,               0,       0,            CLK_PLL_PRE_CFG,    0,         5,          0,          0,          0,          0,               0,              0,              0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(i2s0,           CLK_I2S0_CFG,    24,      1,            CLK_I2S0_CFG,       0,         4,          0,          0,          0,          CLK_I2S0_CFG,    CLK_I2S_GATE,   CLK_I2S_GATE,   0,             31,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(i2s1,           CLK_I2S1_CFG,    24,      1,            CLK_I2S1_CFG,       0,         4,          0,          0,          0,          CLK_I2S1_CFG,    CLK_I2S_GATE,   CLK_I2S_GATE,   0,             31,         17,         1,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(dmic,           CLK_DMIC_CFG,    24,      1,            CLK_DMIC_CFG,       0,         4,          0,          0,          0,          CLK_DMIC_CFG,    CLK_DMIC_GATE,  CLK_DMIC_GATE,  0,             31,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(codec_dac,      CLK_CODEC_DAC_CFG,24,     1,            CLK_CODEC_DAC_CFG,  0,         4,          0,          0,          0,          CLK_CODEC_DAC_CFG, CLK_CODEC_GATE, CLK_CODEC_GATE, 0,           31,         16,         0,              0,              &com_gates[1],    0);
SUNXI_CLK_PERIPH_CONFIG(codec_adc,      CLK_CODEC_ADC_CFG,24,     1,            CLK_CODEC_ADC_CFG,  0,         4,          0,          0,          0,          CLK_CODEC_DAC_CFG, CLK_CODEC_GATE, CLK_CODEC_GATE, 0,           31,         16,         0,              0,              &com_gates[1],    1);
SUNXI_CLK_PERIPH_CONFIG(usbphy0,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_USB0_CFG,   0,              0,             0,          30,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(usbohci0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          CLK_USB0_CFG,    CLK_USB_GATE,   CLK_USB_GATE,   0,             31,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(usbohci0_12m,   CLK_USB0_CFG,    24,      2,            0,                  0,         0,          0,          0,          0,          0,               0,              0,              0,             0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(usbehci0,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_USB_GATE,   CLK_USB_GATE,   0,             0,          20,         4,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(usbotg,         0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_USB_GATE,   CLK_USB_GATE,   0,             0,          24,         8,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(dpss_top,       0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_DPSS_TOP_GATE, CLK_DPSS_TOP_GATE, 0,       0,          16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(mipi_dsi,       CLK_DSI_CFG,     24,      3,            CLK_DSI_CFG,        0,         4,          0,          0,          0,          CLK_DSI_CFG,     CLK_DSI_GATE,   CLK_DSI_GATE,   0,             31,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(tcon_lcd,       CLK_TCON_LCD_CFG, 24,     3,            CLK_TCON_LCD_CFG,   0,         4,          8,          2,          0,          CLK_TCON_LCD_CFG, CLK_TCON_LCD_GATE, CLK_TCON_LCD_GATE, 0,      31,         16,         0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(csi_top,        CLK_CSI_TOP_CFG, 24,      3,            CLK_CSI_TOP_CFG,    0,         5,          0,          0,          0,          CLK_CSI_TOP_CFG, CLK_CSI_GATE,   CLK_CSI_GATE,   CLK_MBUS_GATE, 31,         16,         0,              8,              &com_gates[0],    0);
SUNXI_CLK_PERIPH_CONFIG(csi_master0,    CLK_CSI_MASTER0_CFG, 24,  3,            CLK_CSI_MASTER0_CFG, 0,        5,          8,          2,          0,          CLK_CSI_MASTER0_CFG, CLK_CSI_GATE, CLK_CSI_GATE, CLK_MBUS_GATE, 31,         16,         0,              8,              &com_gates[0],    1);
SUNXI_CLK_PERIPH_CONFIG(csi_master1,    CLK_CSI_MASTER1_CFG, 24,  3,            CLK_CSI_MASTER1_CFG, 0,        5,          8,          2,          0,          CLK_CSI_MASTER1_CFG, CLK_CSI_GATE, CLK_CSI_GATE, CLK_MBUS_GATE, 31,         16,         0,              8,              &com_gates[0],    2);
SUNXI_CLK_PERIPH_CONFIG(csi_master2,    CLK_CSI_MASTER2_CFG, 24,  3,            CLK_CSI_MASTER2_CFG, 0,        5,          8,          2,          0,          CLK_CSI_MASTER2_CFG, CLK_CSI_GATE, CLK_CSI_GATE, CLK_MBUS_GATE, 31,         16,         0,              8,              &com_gates[0],    3);
SUNXI_CLK_PERIPH_CONFIG(isp,            CLK_ISP_CFG,     24,      3,            CLK_ISP_CFG,        0,         5,          0,          0,          0,          CLK_ISP_CFG,         0,           0,             CLK_MBUS_GATE, 31,         0,          0,              9,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(wiegand,        0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               CLK_WIEGAND_GATE, CLK_WIEGAND_GATE, 0,         0,          16,         0,              0,              NULL,             0);
/* TODO the E907 modele reset and gate were not config */
SUNXI_CLK_PERIPH_CONFIG(e907,           CLK_E907_CFG,    24,      3,            CLK_E907_CFG,       0,         5,          0,          0,          0,          0,               CLK_RISCV_GATE,  CLK_RISCV_GATE, 0,            0,          16,         0,              0,              &com_gates[2],    0);
SUNXI_CLK_PERIPH_CONFIG(e907_axi,       CLK_E907_CFG,    0,       0,            CLK_E907_CFG,       8,         2,          0,          0,          0,          0,               CLK_RISCV_GATE,  CLK_RISCV_GATE, 0,            0,          16,         0,              0,              &com_gates[2],    1);
SUNXI_CLK_PERIPH_CONFIG(fanout_25m,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               CLK_FANOUT_GATE, 0,           0,          0,          3,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout_16m,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               CLK_FANOUT_GATE, 0,           0,          0,          2,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout_12m,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               CLK_FANOUT_GATE, 0,           0,          0,          1,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout_24m,     0,               0,       0,            0,                  0,         0,          0,          0,          0,          0,               0,               CLK_FANOUT_GATE, 0,           0,          0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout_27m,     CLK_FANOUT27M_CFG, 24,    2,            CLK_FANOUT27M_CFG,  0,         5,          8,          2,          0,          0,               0,               0,             0,             31,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout_pclk,    0,               0,       0,            CLK_FANOUTPCLK_CFG, 0,         5,          0,          0,          0,          CLK_FANOUTPCLK_CFG, 0,            0,             0,             31,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout0,        CLK_CCMU_FANOUT_CFG, 0,   3,            0,                  0,         0,          0,          0,          0,          CLK_CCMU_FANOUT_CFG, 0,           0,             0,             21,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout1,        CLK_CCMU_FANOUT_CFG, 3,   3,            0,                  0,         0,          0,          0,          0,          CLK_CCMU_FANOUT_CFG, 0,           0,             0,             22,         0,          0,              0,              NULL,             0);
SUNXI_CLK_PERIPH_CONFIG(fanout2,        CLK_CCMU_FANOUT_CFG, 6,   3,            0,                  0,         0,          0,          0,          0,          CLK_CCMU_FANOUT_CFG, 0,           0,             0,             23,         0,          0,              0,              NULL,             0);

SUNXI_CLK_PERIPH(cpu,       HAL_CLK_BUS_C0_CPU,     cpu_parents);
SUNXI_CLK_PERIPH(axi,       HAL_CLK_BUS_C0_AXI,     cpu_apb_axi_parents);
SUNXI_CLK_PERIPH(cpuapb,    HAL_CLK_BUS_CPUAPB,     cpu_apb_axi_parents);
SUNXI_CLK_PERIPH(ahb,       HAL_CLK_BUS_AHB,        ahb_apb_parents);
SUNXI_CLK_PERIPH(apb0,      HAL_CLK_BUS_APB0,       ahb_apb_parents);
SUNXI_CLK_PERIPH(apb1,      HAL_CLK_BUS_APB1,       ahb_apb_parents);
SUNXI_CLK_PERIPH(mbus,      HAL_CLK_BUS_MBUS,       mbus_parents);
SUNXI_CLK_PERIPH(de,        HAL_CLK_PERIPH_DE,      de_parents);
SUNXI_CLK_PERIPH(g2d,       HAL_CLK_PERIPH_G2D,     de_parents);
SUNXI_CLK_PERIPH(ce,        HAL_CLK_PERIPH_CE,      ce_parents);
SUNXI_CLK_PERIPH(ve,        HAL_CLK_PERIPH_VE,      ve_parents);
SUNXI_CLK_PERIPH(npu,       HAL_CLK_PERIPH_NPU,     npu_parents);
SUNXI_CLK_PERIPH(dma,       HAL_CLK_PERIPH_DMA,     ahbmod_parents);
SUNXI_CLK_PERIPH(msgbox0,   HAL_CLK_PERIPH_MSGBOX0,     ahbmod_parents);
SUNXI_CLK_PERIPH(msgbox1,   HAL_CLK_PERIPH_MSGBOX1,     ahbmod_parents);
SUNXI_CLK_PERIPH(spinlock,  HAL_CLK_PERIPH_SPINLOCK,     ahbmod_parents);
SUNXI_CLK_PERIPH(hstimer,   HAL_CLK_PERIPH_HSTIMER,     ahbmod_parents);
SUNXI_CLK_PERIPH(avs,       HAL_CLK_PERIPH_AVS,     hosc_parents);
SUNXI_CLK_PERIPH(dbgsys,    HAL_CLK_PERIPH_DBGSYS,  hosc_parents);
SUNXI_CLK_PERIPH(pwm,       HAL_CLK_PERIPH_PWM,     apb0mod_parents);
SUNXI_CLK_PERIPH(iommu,     HAL_CLK_PERIPH_IOMMU,   ahbmod_parents);
SUNXI_CLK_PERIPH(sdram,     HAL_CLK_PERIPH_SDRAM,    sdram_parents);
SUNXI_CLK_PERIPH(sdmmc0_mod,    HAL_CLK_PERIPH_SDMMC0_MOD,  smhc01_parents);
SUNXI_CLK_PERIPH(sdmmc0_rst,    HAL_CLK_PERIPH_SDMMC0_RST,  smhc01_parents);
SUNXI_CLK_PERIPH(sdmmc0_bus,    HAL_CLK_PERIPH_SDMMC0_BUS,  smhc01_parents);
SUNXI_CLK_PERIPH(sdmmc1_mod,    HAL_CLK_PERIPH_SDMMC1_MOD,  smhc01_parents);
SUNXI_CLK_PERIPH(sdmmc1_rst,    HAL_CLK_PERIPH_SDMMC1_RST,  smhc01_parents);
SUNXI_CLK_PERIPH(sdmmc1_bus,    HAL_CLK_PERIPH_SDMMC1_BUS,  smhc01_parents);
SUNXI_CLK_PERIPH(sdmmc2_mod,    HAL_CLK_PERIPH_SDMMC2_MOD,  smhc2_parents);
SUNXI_CLK_PERIPH(sdmmc2_rst,    HAL_CLK_PERIPH_SDMMC2_RST,  smhc2_parents);
SUNXI_CLK_PERIPH(sdmmc2_bus,    HAL_CLK_PERIPH_SDMMC2_BUS,  smhc2_parents);
SUNXI_CLK_PERIPH(uart0,     HAL_CLK_PERIPH_UART0,   apb1mod_parents);
SUNXI_CLK_PERIPH(uart1,     HAL_CLK_PERIPH_UART1,   apb1mod_parents);
SUNXI_CLK_PERIPH(uart2,     HAL_CLK_PERIPH_UART2,   apb1mod_parents);
SUNXI_CLK_PERIPH(uart3,     HAL_CLK_PERIPH_UART3,   apb1mod_parents);
SUNXI_CLK_PERIPH(twi0,      HAL_CLK_PERIPH_TWI0,    apb1mod_parents);
SUNXI_CLK_PERIPH(twi1,      HAL_CLK_PERIPH_TWI1,    apb1mod_parents);
SUNXI_CLK_PERIPH(twi2,      HAL_CLK_PERIPH_TWI2,    apb1mod_parents);
SUNXI_CLK_PERIPH(twi3,      HAL_CLK_PERIPH_TWI3,    apb1mod_parents);
SUNXI_CLK_PERIPH(twi4,      HAL_CLK_PERIPH_TWI4,    apb1mod_parents);
SUNXI_CLK_PERIPH(spi0,      HAL_CLK_PERIPH_SPI0,    spi_parents);
SUNXI_CLK_PERIPH(spi1,      HAL_CLK_PERIPH_SPI1,    spi_parents);
SUNXI_CLK_PERIPH(spi2,      HAL_CLK_PERIPH_SPI2,    spi_parents);
SUNXI_CLK_PERIPH(spi3,      HAL_CLK_PERIPH_SPI3,    spi_parents);
SUNXI_CLK_PERIPH(spif,      HAL_CLK_PERIPH_SPIF,    spi_parents);
SUNXI_CLK_PERIPH(gmac_25m,  HAL_CLK_PERIPH_GMAC_25M,    gmac_25m_parents);
SUNXI_CLK_PERIPH(gmac,      HAL_CLK_PERIPH_GMAC,     ahbmod_parents);
SUNXI_CLK_PERIPH(gpadc,     HAL_CLK_PERIPH_GPADC,   gpadc_parents);
SUNXI_CLK_PERIPH(ths,       HAL_CLK_PERIPH_THS,     apb0mod_parents);
/*
 * Strictly speaking, the pll_audiox4 and pll_audio are not pll clocks, although they are called pll
 */
SUNXI_CLK_PERIPH(pll_audiox4,   HAL_CLK_PLL_AUDIOX4,   pll_audiox4_parents);
SUNXI_CLK_PERIPH(pll_audio,     HAL_CLK_PLL_AUDIO,   pll_audio_parents);
SUNXI_CLK_PERIPH(i2s0,      HAL_CLK_PERIPH_I2S0,    audio_parents);
SUNXI_CLK_PERIPH(i2s1,      HAL_CLK_PERIPH_I2S1,    audio_parents);
SUNXI_CLK_PERIPH(dmic,      HAL_CLK_PERIPH_DMIC,    audio_parents);
SUNXI_CLK_PERIPH(codec_dac,  HAL_CLK_PERIPH_CODEC_DAC, audio_parents);
SUNXI_CLK_PERIPH(codec_adc,  HAL_CLK_PERIPH_CODEC_ADC, audio_parents);
SUNXI_CLK_PERIPH(usbphy0,   HAL_CLK_PERIPH_USB0,    hosc_parents);
SUNXI_CLK_PERIPH(usbohci0,  HAL_CLK_PERIPH_USBOHCI0,   ahbmod_parents);
SUNXI_CLK_PERIPH(usbohci0_12m,   HAL_CLK_PERIPH_USBOHCI0_12M,  usbohci_12m_parents);
SUNXI_CLK_PERIPH(usbehci0,  HAL_CLK_PERIPH_USBEHCI0, ahbmod_parents);
SUNXI_CLK_PERIPH(usbotg,    HAL_CLK_PERIPH_USBOTG,   ahbmod_parents);
SUNXI_CLK_PERIPH(dpss_top,  HAL_CLK_PERIPH_DPSS_TOP,   ahbmod_parents);
SUNXI_CLK_PERIPH(mipi_dsi,  HAL_CLK_PERIPH_MIPI_DSI0,      dsi_parents);
SUNXI_CLK_PERIPH(tcon_lcd,  HAL_CLK_PERIPH_TCON_LCD0,      tcon_lcd_parents);
SUNXI_CLK_PERIPH(csi_top,       HAL_CLK_PERIPH_CSI_TOP,      csi_top_parents);
SUNXI_CLK_PERIPH(csi_master0,   HAL_CLK_PERIPH_CSI_MASTER0,      csi_master_parents);
SUNXI_CLK_PERIPH(csi_master1,   HAL_CLK_PERIPH_CSI_MASTER1,      csi_master_parents);
SUNXI_CLK_PERIPH(csi_master2,   HAL_CLK_PERIPH_CSI_MASTER2,      csi_master_parents);
SUNXI_CLK_PERIPH(isp,       HAL_CLK_PERIPH_ISP,      isp_parents);
SUNXI_CLK_PERIPH(wiegand,   HAL_CLK_PERIPH_DSPO,      apb0mod_parents);
SUNXI_CLK_PERIPH(e907,      HAL_CLK_PERIPH_E907,      e907_parents);
SUNXI_CLK_PERIPH(e907_axi,  HAL_CLK_PERIPH_E907_AXI,  e907_axi_parents);
SUNXI_CLK_PERIPH(fanout_25m,  HAL_CLK_PERIPH_FANOUT_25M,  fanout25m_parents);
SUNXI_CLK_PERIPH(fanout_16m,  HAL_CLK_PERIPH_FANOUT_16M,  fanout16m_parents);
SUNXI_CLK_PERIPH(fanout_12m,  HAL_CLK_PERIPH_FANOUT_12M,  fanout12m_parents);
SUNXI_CLK_PERIPH(fanout_24m,  HAL_CLK_PERIPH_FANOUT_24M,  fanout24m_parents);
SUNXI_CLK_PERIPH(fanout_27m,  HAL_CLK_PERIPH_FANOUT_27M,  fanout27m_parents);
SUNXI_CLK_PERIPH(fanout_pclk,  HAL_CLK_PERIPH_FANOUT_PCLK,  fanout_pclk_parents);
SUNXI_CLK_PERIPH(fanout0,  HAL_CLK_PERIPH_FANOUT0,  fanout012_parents);
SUNXI_CLK_PERIPH(fanout1,  HAL_CLK_PERIPH_FANOUT1,  fanout012_parents);
SUNXI_CLK_PERIPH(fanout2,  HAL_CLK_PERIPH_FANOUT2,  fanout012_parents);

SUNXI_PERIPH_INIT(cpu, HAL_CLK_BUS_C0_CPU, HAL_CLK_PLL_CPUX_C0, 1200000000U);
SUNXI_PERIPH_INIT(axi, HAL_CLK_BUS_C0_AXI, HAL_CLK_BUS_C0_CPU, 400000000U);
SUNXI_PERIPH_INIT(apb0, HAL_CLK_BUS_APB0,  HAL_CLK_PLL_PERI0X2, 100000000U);
SUNXI_PERIPH_INIT(apb1, HAL_CLK_BUS_APB1,  HAL_CLK_SRC_HOSC, 24000000U);
SUNXI_PERIPH_INIT(ahb, HAL_CLK_BUS_AHB, HAL_CLK_PLL_PERI0X2, 200000000U);

static int get_factors_pll_cpu(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }
    tmp_rate = rate > SUNXI_CLK_FACTOR_CPU_MAX_FREQ ? SUNXI_CLK_FACTOR_CPU_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(cpu))
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_ddr0(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }

    tmp_rate = rate > SUNXI_CLK_FACTOR_DDR0_MAX_FREQ ? SUNXI_CLK_FACTOR_DDR0_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(ddr0))
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_periph0x2(u32 rate, u32 parent_rate,
                                   struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }

    tmp_rate = rate > SUNXI_CLK_FACTOR_PERIPH0X2_MAX_FREQ ? SUNXI_CLK_FACTOR_PERIPH0X2_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(periph0x2))
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_periph0800m(u32 rate, u32 parent_rate,
                                   struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }

    tmp_rate = rate > SUNXI_CLK_FACTOR_PERIPH0800M_MAX_FREQ ? SUNXI_CLK_FACTOR_PERIPH0800M_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(periph0800m))
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_periph0480m(u32 rate, u32 parent_rate,
                                   struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }

    tmp_rate = rate > SUNXI_CLK_FACTOR_PERIPH0480M_MAX_FREQ ? SUNXI_CLK_FACTOR_PERIPH0480M_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(periph0480m))
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_video0x4(u32 rate, u32 parent_rate,
                                 struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }
    tmp_rate = rate > SUNXI_CLK_FACTOR_VIDEO0X4_MAX_FREQ ? SUNXI_CLK_FACTOR_VIDEO0X4_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(video0x4))
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_csix4(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }
    tmp_rate = rate > SUNXI_CLK_FACTOR_CSIX4_MAX_FREQ ? SUNXI_CLK_FACTOR_CSIX4_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(csix4))
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_audio_div2(u32 rate, u32 parent_rate,
                                 struct clk_factors_value *factor)
{
    if (rate == 22579200)
    {
        factor->factorn = 21;
        factor->factorp = 11;
        factor->factord1 = 0;
        factor->factord2 = 1;
        sunxi_clk_factor_config_pll_audio_div2.sdmval = 0xC001288D;
    }
    else if (rate == 24576000)
    {
        factor->factorn = 23;
        factor->factorp = 11;
        factor->factord1 = 0;
        factor->factord2 = 1;
        sunxi_clk_factor_config_pll_audio_div2.sdmval = 0xC00126E9;
    }
    else
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_audio_div5(u32 rate, u32 parent_rate,
                                 struct clk_factors_value *factor)
{
    if (rate == 22579200)
    {
        factor->factorn = 21;
        factor->factorp = 11;
        factor->factord1 = 0;
        factor->factord2 = 1;
        sunxi_clk_factor_config_pll_audio_div5.sdmval = 0xC001288D;
    }
    else if (rate == 24576000)
    {
        factor->factorn = 23;
        factor->factorp = 11;
        factor->factord1 = 0;
        factor->factord2 = 1;
        sunxi_clk_factor_config_pll_audio_div5.sdmval = 0xC00126E9;
    }
    else
    {
        return -1;
    }

    return 0;
}

static int get_factors_pll_npux4(u32 rate, u32 parent_rate,
                               struct clk_factors_value *factor)
{
    int index;
    u64 tmp_rate;

    if (!factor)
    {
        return -1;
    }
    tmp_rate = rate > SUNXI_CLK_FACTOR_NPUX4_MAX_FREQ ? SUNXI_CLK_FACTOR_NPUX4_MAX_FREQ : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;

    if (FACTOR_SEARCH(npux4))
    {
        return -1;
    }

    return 0;
}

/*    pll_cpux: 24*N/P (P=2^factorp)  */
static int calc_rate_pll_cpu(u32 parent_rate,
                             struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, (1 << factor->factorp));
    return (u32)tmp_rate;
}

/*    pll_ddr: 24*N/D1/D2    */
static int calc_rate_pll_ddr0(u32 parent_rate,
                             struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, (factor->factord1 + 1) * (factor->factord2 + 1));
    return (u32)tmp_rate;
}

/*    pll_periph0x2: 24*N/D1/D2/2    */
static int calc_rate_pll_periph0x2(u32 parent_rate,
                                 struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, 2 * (factor->factord1 + 1) * (factor->factord2 + 1));
    return (u32)tmp_rate;
}

static int calc_rate_pll_periph0800m(u32 parent_rate,
                                 struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, 2 * (factor->factord1 + 1) * (factor->factord2 + 1));
    return (u32)tmp_rate;
}

static int calc_rate_pll_periph0480m(u32 parent_rate,
                                 struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, 2 * (factor->factord1 + 1) * (factor->factord2 + 1));
    return (u32)tmp_rate;
}

static int calc_rate_pll_video0x4(u32 parent_rate,
                               struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, 4 * (factor->factord1 + 1));
    return (u32)tmp_rate;
}

static int calc_rate_pll_csix4(u32 parent_rate,
                               struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, (factor->factord1 + 1));
    return (u32)tmp_rate;
}

/*
 *    pll_audio: 24*N/D1/D2/P
 *
 *    NOTE: pll_audiox4 = 24*N/D1/2
 *          pll_audiox2 = 24*N/D1/4
 *
 *    pll_audiox4=2*pll_audiox2=4*pll_audio only when D2*P=8
 */
static int calc_rate_pll_audio_div2(u32 parent_rate,
                               struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    if ((factor->factorn == 21) &&
        (factor->factorp == 11) &&
        (factor->factord1 == 0) &&
        (factor->factord2 == 1))
    {
        return 22579200;
    }
    else if ((factor->factorn == 23) &&
             (factor->factorp == 11) &&
             (factor->factord1 == 0) &&
             (factor->factord2 == 1))
    {
        return 24576000;
    }
    else
    {
        tmp_rate = tmp_rate * (factor->factorn + 1);
        do_div(tmp_rate, ((factor->factorp + 1) *
                          (factor->factord1 + 1) *
                          (factor->factord2 + 1)));
        return (u32)tmp_rate;
    }
}

static int calc_rate_pll_audio_div5(u32 parent_rate,
                               struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    if ((factor->factorn == 21) &&
        (factor->factorp == 11) &&
        (factor->factord1 == 0) &&
        (factor->factord2 == 1))
    {
        return 22579200;
    }
    else if ((factor->factorn == 23) &&
             (factor->factorp == 11) &&
             (factor->factord1 == 0) &&
             (factor->factord2 == 1))
    {
        return 24576000;
    }
    else
    {
        tmp_rate = tmp_rate * (factor->factorn + 1);
        do_div(tmp_rate, ((factor->factorp + 1) *
                          (factor->factord1 + 1) *
                          (factor->factord2 + 1)));
        return (u32)tmp_rate;
    }
}

static int calc_rate_pll_npux4(u32 parent_rate,
                             struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate ? parent_rate : 24000000);
    tmp_rate = tmp_rate * (factor->factorn + 1);
    do_div(tmp_rate, 4 * (factor->factord1 + 1));
    return (u32)tmp_rate;
}

/*************************************************************************
 * SUNXI Factor Pll Clock Initialize API definition
 * 初始化
 *************************************************************************/

hal_clk_status_t sunxi_factor_pll_cpu_init(void)
{
    u32 i = 0, current_rate = 0, parent_rate = 0;
    hal_clk_status_t ret = HAL_CLK_STATUS_OK;
    clk_factor_pt factor = NULL;
    clk_periph_pt periph = NULL;

    CCMU_TRACE();
    factor = (clk_factor_pt)clk_get_core(HAL_CLK_PLL_CPUX_C0);
    periph = (clk_periph_pt)clk_get_core(HAL_CLK_BUS_C0_AXI);
    if ((factor == NULL) || (periph == NULL))
    {
        return HAL_CLK_STATUS_ERROT_CLK_UNDEFINED;
    }
    ret = sunxi_clk_factors_recalc_rate(factor, &current_rate);
    if (ret != HAL_CLK_STATUS_OK)
    {
        return ret;
    }
    CCMU_DBG("clk %d current rate %d \n", factor->clk_core.clk, current_rate);
    if (factor->clk_core.clk_rate == current_rate)
    {
        return HAL_CLK_STATUS_OK;
    }
    //set AXI clock aprent to OSC23M before PLL_CPUX change rate
    ret = sunxi_clk_periph_set_parent(periph, 0);
    ret = sunxi_clk_factors_set_rate(factor, factor->clk_core.clk_rate);
    if (ret != HAL_CLK_STATUS_OK)
    {
        return ret;
    }
    ret = sunxi_clk_factors_recalc_rate(factor, &current_rate);
    if (ret != HAL_CLK_STATUS_OK)
    {
        return ret;
    }
    CCMU_DBG("clk %d recalc current rate %d \n", factor->clk_core.clk, current_rate);
    if (factor->clk_core.clk_rate != current_rate)
    {
        return HAL_CLK_STATUS_ERROR;
    }
    //set AXI clock aprent to PLL_CPUX after PLL_CPUX change rate
    ret = sunxi_clk_periph_set_parent(periph, 0x03);
    CCMU_DBG("clk init sucess! \n");
    return ret;
}


hal_clk_status_t sunxi_factor_pll_periph0_init(void)
{
    hal_clk_status_t ret = HAL_CLK_STATUS_OK;
    clk_factor_pt factor = NULL;

    CCMU_TRACE();
    factor = (clk_factor_pt)clk_get_core(HAL_CLK_PLL_PERI0X2);
    if (factor == NULL)
    {
        return HAL_CLK_STATUS_ERROT_CLK_UNDEFINED;
    }

    ret = sunxi_clk_fators_enable(factor);
    if (ret == HAL_CLK_STATUS_ENABLED)
    {
        factor->clk_core.clk_enbale = HAL_CLK_STATUS_ENABLED;
    }
    return HAL_CLK_STATUS_OK;
}

clk_core_pt sunxi_clk_fixed_src_arry[] =
{
    &sunxi_clk_fixed_src_ext_losc,
    &sunxi_clk_fixed_src_iosc,
    &sunxi_clk_fixed_src_hosc,
    &sunxi_clk_fixed_src_hoscdiv32k,
    &sunxi_clk_fixed_src_osc48m,
    NULL,
};

clk_factor_pt sunxi_clk_factor_arry[] =
{
    &sunxi_clk_factor_pll_cpu,
    &sunxi_clk_factor_pll_ddr0,
    &sunxi_clk_factor_pll_periph0x2,
    &sunxi_clk_factor_pll_periph0800m,
    &sunxi_clk_factor_pll_periph0480m,
    &sunxi_clk_factor_pll_video0x4,
    &sunxi_clk_factor_pll_csix4,
    &sunxi_clk_factor_pll_audio_div2,
    &sunxi_clk_factor_pll_audio_div5,
    &sunxi_clk_factor_pll_npux4,
    NULL,
};

clk_fixed_factor_pt sunxi_clk_fixed_factor_arry[] =
{
    &sunxi_clk_fixed_factor_iosc32k,
    &sunxi_clk_fixed_factor_pll_periph0600m,
    &sunxi_clk_fixed_factor_pll_periph0400m,
    &sunxi_clk_fixed_factor_pll_periph0300m,
    &sunxi_clk_fixed_factor_pll_periph0200m,
    &sunxi_clk_fixed_factor_pll_periph0160m,
    &sunxi_clk_fixed_factor_pll_periph0150m,
    &sunxi_clk_fixed_factor_pll_periph0150m_div6,
    &sunxi_clk_fixed_factor_pll_periph0160m_div10,
    &sunxi_clk_fixed_factor_pll_video0x2,
    &sunxi_clk_fixed_factor_pll_video0,
    &sunxi_clk_fixed_factor_pll_npux2,
    &sunxi_clk_fixed_factor_pll_npu,
    &sunxi_clk_fixed_factor_hoscd2,
    &sunxi_clk_fixed_factor_osc48md4,
    &sunxi_clk_fixed_factor_sdramd4,
    NULL,
};

clk_periph_pt sunxi_clk_periph_arry[] =
{
    &sunxi_clk_periph_cpu,
    &sunxi_clk_periph_axi,
    &sunxi_clk_periph_cpuapb,
    &sunxi_clk_periph_ahb,
    &sunxi_clk_periph_apb0,
    &sunxi_clk_periph_apb1,
    &sunxi_clk_periph_mbus,
    &sunxi_clk_periph_de,
    &sunxi_clk_periph_g2d,
    &sunxi_clk_periph_ce,
    &sunxi_clk_periph_ve,
    &sunxi_clk_periph_npu,
    &sunxi_clk_periph_dma,
    &sunxi_clk_periph_msgbox0,
    &sunxi_clk_periph_msgbox1,
    &sunxi_clk_periph_spinlock,
    &sunxi_clk_periph_hstimer,
    &sunxi_clk_periph_avs,
    &sunxi_clk_periph_dbgsys,
    &sunxi_clk_periph_pwm,
    &sunxi_clk_periph_iommu,
    &sunxi_clk_periph_sdram,
    &sunxi_clk_periph_sdmmc0_mod,
    &sunxi_clk_periph_sdmmc0_rst,
    &sunxi_clk_periph_sdmmc0_bus,
    &sunxi_clk_periph_sdmmc1_mod,
    &sunxi_clk_periph_sdmmc1_rst,
    &sunxi_clk_periph_sdmmc1_bus,
    &sunxi_clk_periph_sdmmc2_mod,
    &sunxi_clk_periph_sdmmc2_rst,
    &sunxi_clk_periph_sdmmc2_bus,
    &sunxi_clk_periph_uart0,
    &sunxi_clk_periph_uart1,
    &sunxi_clk_periph_uart2,
    &sunxi_clk_periph_uart3,
    &sunxi_clk_periph_twi0,
    &sunxi_clk_periph_twi1,
    &sunxi_clk_periph_twi2,
    &sunxi_clk_periph_twi3,
    &sunxi_clk_periph_twi4,
    &sunxi_clk_periph_spi0,
    &sunxi_clk_periph_spi1,
    &sunxi_clk_periph_spi2,
    &sunxi_clk_periph_spi3,
    &sunxi_clk_periph_spif,
    &sunxi_clk_periph_gmac_25m,
    &sunxi_clk_periph_gmac,
    &sunxi_clk_periph_gpadc,
    &sunxi_clk_periph_ths,
    &sunxi_clk_periph_pll_audiox4,
    &sunxi_clk_periph_pll_audio,
    &sunxi_clk_periph_i2s0,
    &sunxi_clk_periph_i2s1,
    &sunxi_clk_periph_dmic,
    &sunxi_clk_periph_codec_dac,
    &sunxi_clk_periph_codec_adc,
    &sunxi_clk_periph_usbphy0,
    &sunxi_clk_periph_usbohci0,
    &sunxi_clk_periph_usbohci0_12m,
    &sunxi_clk_periph_usbehci0,
    &sunxi_clk_periph_usbotg,
    &sunxi_clk_periph_dpss_top,
    &sunxi_clk_periph_mipi_dsi,
    &sunxi_clk_periph_tcon_lcd,
    &sunxi_clk_periph_csi_top,
    &sunxi_clk_periph_csi_master0,
    &sunxi_clk_periph_csi_master1,
    &sunxi_clk_periph_csi_master2,
    &sunxi_clk_periph_isp,
    &sunxi_clk_periph_wiegand,
    &sunxi_clk_periph_e907,
    &sunxi_clk_periph_e907_axi,
    &sunxi_clk_periph_fanout_25m,
    &sunxi_clk_periph_fanout_16m,
    &sunxi_clk_periph_fanout_12m,
    &sunxi_clk_periph_fanout_24m,
    &sunxi_clk_periph_fanout_27m,
    &sunxi_clk_periph_fanout_pclk,
    &sunxi_clk_periph_fanout0,
    &sunxi_clk_periph_fanout1,
    &sunxi_clk_periph_fanout2,
    NULL,
};

clk_base_pt sunxi_periph_clk_init_arry[] =
{
    &sunxi_periph_clk_init_cpu,
    &sunxi_periph_clk_init_axi,
    &sunxi_periph_clk_init_apb0,
    &sunxi_periph_clk_init_apb1,
    &sunxi_periph_clk_init_ahb,
    NULL,
};

hal_clk_status_t (*sunxi_clk_factor_init[])(void) =
{
//    &sunxi_factor_pll_cpu_init,
    &sunxi_factor_pll_periph0_init,
    NULL,
};

