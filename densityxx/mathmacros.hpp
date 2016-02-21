// see LICENSE.md for license.
#pragma once

#define DENSITY_FORMAT(v)               0##v##llu

#define DENSITY_ISOLATE(b, p)           ((DENSITY_FORMAT(b) / p) & 0x1)

#define DENSITY_BINARY_TO_UINT(b)        ((DENSITY_ISOLATE(b, 1llu) ? 0x1 : 0)\
                                        + (DENSITY_ISOLATE(b, 8llu) ? 0x2 : 0)\
                                        + (DENSITY_ISOLATE(b, 64llu) ? 0x4 : 0)\
                                        + (DENSITY_ISOLATE(b, 512llu) ? 0x8 : 0)\
                                        + (DENSITY_ISOLATE(b, 4096llu) ? 0x10 : 0)\
                                        + (DENSITY_ISOLATE(b, 32768llu) ? 0x20 : 0)\
                                        + (DENSITY_ISOLATE(b, 262144llu) ? 0x40 : 0)\
                                        + (DENSITY_ISOLATE(b, 2097152llu) ? 0x80 : 0)\
                                        + (DENSITY_ISOLATE(b, 16777216llu) ? 0x100 : 0)\
                                        + (DENSITY_ISOLATE(b, 134217728llu) ? 0x200 : 0)\
                                        + (DENSITY_ISOLATE(b, 1073741824llu) ? 0x400 : 0)\
                                        + (DENSITY_ISOLATE(b, 8589934592llu) ? 0x800 : 0)\
                                        + (DENSITY_ISOLATE(b, 68719476736llu) ? 0x1000 : 0)\
                                        + (DENSITY_ISOLATE(b, 549755813888llu) ? 0x2000 : 0)\
                                        + (DENSITY_ISOLATE(b, 4398046511104llu) ? 0x4000 : 0)\
                                        + (DENSITY_ISOLATE(b, 35184372088832llu) ? 0x8000 : 0)\
                                        + (DENSITY_ISOLATE(b, 281474976710656llu) ? 0x10000 : 0)\
                                        + (DENSITY_ISOLATE(b, 2251799813685248llu) ? 0x20000 : 0))

#define DENSITY_UNROLL_2(op)     op; op
#define DENSITY_UNROLL_4(op)     DENSITY_UNROLL_2(op);    DENSITY_UNROLL_2(op)
#define DENSITY_UNROLL_8(op)     DENSITY_UNROLL_4(op);    DENSITY_UNROLL_4(op)
#define DENSITY_UNROLL_16(op)    DENSITY_UNROLL_8(op);    DENSITY_UNROLL_8(op)
#define DENSITY_UNROLL_32(op)    DENSITY_UNROLL_16(op);   DENSITY_UNROLL_16(op)
#define DENSITY_UNROLL_64(op)    DENSITY_UNROLL_32(op);   DENSITY_UNROLL_32(op)

#define DENSITY_CASE_GENERATOR_2(op_a, flag_a, op_b, flag_b, op_mid, shift)\
    case ((flag_b << shift) | flag_a):\
        op_a;\
        op_mid;\
        op_b;\
        break;

#define DENSITY_CASE_GENERATOR_4(op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift)\
    case ((flag_d << (shift * 3)) | (flag_c << (shift * 2)) | (flag_b << shift) | flag_a):\
        op_a;\
        op_mid;\
        op_b;\
        op_mid;\
        op_c;\
        op_mid;\
        op_d;\
        break;

#define DENSITY_CASE_GENERATOR_4_2_LAST_1_COMBINED(op_1, flag_1, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift)\
    DENSITY_CASE_GENERATOR_2(op_1, flag_1, op_a, flag_a, op_mid, shift);\
    DENSITY_CASE_GENERATOR_2(op_1, flag_1, op_b, flag_b, op_mid, shift);\
    DENSITY_CASE_GENERATOR_2(op_1, flag_1, op_c, flag_c, op_mid, shift);\
    DENSITY_CASE_GENERATOR_2(op_1, flag_1, op_d, flag_d, op_mid, shift);

#define DENSITY_CASE_GENERATOR_4_2_COMBINED(op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift)\
    DENSITY_CASE_GENERATOR_4_2_LAST_1_COMBINED(op_a, flag_a, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_2_LAST_1_COMBINED(op_b, flag_b, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_2_LAST_1_COMBINED(op_c, flag_c, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_2_LAST_1_COMBINED(op_d, flag_d, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);

#define DENSITY_CASE_GENERATOR_4_4_LAST_1_COMBINED(op_1, flag_1, op_2, flag_2, op_3, flag_3, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift)\
    DENSITY_CASE_GENERATOR_4(op_1, flag_1, op_2, flag_2, op_3, flag_3, op_a, flag_a, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4(op_1, flag_1, op_2, flag_2, op_3, flag_3, op_b, flag_b, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4(op_1, flag_1, op_2, flag_2, op_3, flag_3, op_c, flag_c, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4(op_1, flag_1, op_2, flag_2, op_3, flag_3, op_d, flag_d, op_mid, shift);

#define DENSITY_CASE_GENERATOR_4_4_LAST_2_COMBINED(op_1, flag_1, op_2, flag_2, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift)\
    DENSITY_CASE_GENERATOR_4_4_LAST_1_COMBINED(op_1, flag_1, op_2, flag_2, op_a, flag_a, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_1_COMBINED(op_1, flag_1, op_2, flag_2, op_b, flag_b, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_1_COMBINED(op_1, flag_1, op_2, flag_2, op_c, flag_c, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_1_COMBINED(op_1, flag_1, op_2, flag_2, op_d, flag_d, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);

#define DENSITY_CASE_GENERATOR_4_4_LAST_3_COMBINED(op_1, flag_1, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift)\
    DENSITY_CASE_GENERATOR_4_4_LAST_2_COMBINED(op_1, flag_1, op_a, flag_a, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_2_COMBINED(op_1, flag_1, op_b, flag_b, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_2_COMBINED(op_1, flag_1, op_c, flag_c, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_2_COMBINED(op_1, flag_1, op_d, flag_d, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);

#define DENSITY_CASE_GENERATOR_4_4_COMBINED(op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift)\
    DENSITY_CASE_GENERATOR_4_4_LAST_3_COMBINED(op_a, flag_a, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_3_COMBINED(op_b, flag_b, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_3_COMBINED(op_c, flag_c, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);\
    DENSITY_CASE_GENERATOR_4_4_LAST_3_COMBINED(op_d, flag_d, op_a, flag_a, op_b, flag_b, op_c, flag_c, op_d, flag_d, op_mid, shift);
