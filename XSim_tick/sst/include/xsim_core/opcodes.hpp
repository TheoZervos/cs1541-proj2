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


/// INSTRUCTION IDS
#define ADD_ID  0U
#define SUB_ID  1U
#define NOR_ID  2U
#define AND_ID  3U
#define LIS_ID  4U
#define LIZ_ID  5U
#define LUI_ID  6U
#define PUT_ID  7U
#define HALT_ID  8U

#define DIV_ID  0U
#define EXP_ID  1U
#define MOD_ID  2U

#define MUL_ID  0U

#define LW_ID   0U
#define SW_ID   1U


