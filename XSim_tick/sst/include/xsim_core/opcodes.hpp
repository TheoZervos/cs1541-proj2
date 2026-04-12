#pragma once

// r-type instructions
#define ADD		0U
#define SUB		1U
#define AND		2U
#define NOR     3U
#define DIV     4U
#define MUL     5U
#define MOD     6U
#define EXP     7U
#define LW      8U
#define SW      9U
#define JR      12U
#define JALR    19U
#define HALT    13U
#define PUT     14U

// i-type instructions
#define LIZ     16U
#define LIS     17U
#define LUI     18U
#define BP      20U
#define BN      21U
#define BX      22U
#define BZ      23U

// ix-type instructions
#define J       24U

