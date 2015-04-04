#ifndef MSGPACK_TYPE_DEFINE_EXTERNAL_HPP__
#define MSGPACK_TYPE_DEFINE_EXTERNAL_HPP__



#define MSGPACK_VA_NUM_ARGS(...) MSGPACK_VA_NUM_ARGS_IMPL_((__VA_ARGS__,  31,  30,  29,  28,  27,  26,  25,  24,  23,  22,  21,  20,  19,  18,  17,  16,  15,  14,  13,  12,  11,  10,  9,  8,  7,  6,  5,  4,  3,  2,  1))
#define MSGPACK_VA_NUM_ARGS_IMPL_(tuple) MSGPACK_VA_NUM_ARGS_IMPL tuple
#define MSGPACK_VA_NUM_ARGS_IMPL( _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9,  _10,  _11,  _12,  _13,  _14,  _15,  _16,  _17,  _18,  _19,  _20,  _21,  _22,  _23,  _24,  _25,  _26,  _27,  _28,  _29,  _30,  _31,  N,...) N


#define MSGPACK_EXTERNAL_UNPACK_1( A1) \
	o.via.array.ptr[0].convert(&v . A1);

#define MSGPACK_EXTERNAL_UNPACK_2( A1,  A2) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);

#define MSGPACK_EXTERNAL_UNPACK_3( A1,  A2,  A3) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);

#define MSGPACK_EXTERNAL_UNPACK_4( A1,  A2,  A3,  A4) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);

#define MSGPACK_EXTERNAL_UNPACK_5( A1,  A2,  A3,  A4,  A5) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);

#define MSGPACK_EXTERNAL_UNPACK_6( A1,  A2,  A3,  A4,  A5,  A6) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);

#define MSGPACK_EXTERNAL_UNPACK_7( A1,  A2,  A3,  A4,  A5,  A6,  A7) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);

#define MSGPACK_EXTERNAL_UNPACK_8( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);

#define MSGPACK_EXTERNAL_UNPACK_9( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);

#define MSGPACK_EXTERNAL_UNPACK_10( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);

#define MSGPACK_EXTERNAL_UNPACK_11( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);

#define MSGPACK_EXTERNAL_UNPACK_12( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);

#define MSGPACK_EXTERNAL_UNPACK_13( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);

#define MSGPACK_EXTERNAL_UNPACK_14( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);

#define MSGPACK_EXTERNAL_UNPACK_15( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);

#define MSGPACK_EXTERNAL_UNPACK_16( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);

#define MSGPACK_EXTERNAL_UNPACK_17( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);

#define MSGPACK_EXTERNAL_UNPACK_18( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);

#define MSGPACK_EXTERNAL_UNPACK_19( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);

#define MSGPACK_EXTERNAL_UNPACK_20( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);

#define MSGPACK_EXTERNAL_UNPACK_21( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);

#define MSGPACK_EXTERNAL_UNPACK_22( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);

#define MSGPACK_EXTERNAL_UNPACK_23( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);

#define MSGPACK_EXTERNAL_UNPACK_24( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);

#define MSGPACK_EXTERNAL_UNPACK_25( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);o.via.array.ptr[24].convert(&v . A25);

#define MSGPACK_EXTERNAL_UNPACK_26( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);o.via.array.ptr[24].convert(&v . A25);o.via.array.ptr[25].convert(&v . A26);

#define MSGPACK_EXTERNAL_UNPACK_27( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);o.via.array.ptr[24].convert(&v . A25);o.via.array.ptr[25].convert(&v . A26);o.via.array.ptr[26].convert(&v . A27);

#define MSGPACK_EXTERNAL_UNPACK_28( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);o.via.array.ptr[24].convert(&v . A25);o.via.array.ptr[25].convert(&v . A26);o.via.array.ptr[26].convert(&v . A27);o.via.array.ptr[27].convert(&v . A28);

#define MSGPACK_EXTERNAL_UNPACK_29( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28,  A29) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);o.via.array.ptr[24].convert(&v . A25);o.via.array.ptr[25].convert(&v . A26);o.via.array.ptr[26].convert(&v . A27);o.via.array.ptr[27].convert(&v . A28);o.via.array.ptr[28].convert(&v . A29);

#define MSGPACK_EXTERNAL_UNPACK_30( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28,  A29,  A30) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);o.via.array.ptr[24].convert(&v . A25);o.via.array.ptr[25].convert(&v . A26);o.via.array.ptr[26].convert(&v . A27);o.via.array.ptr[27].convert(&v . A28);o.via.array.ptr[28].convert(&v . A29);o.via.array.ptr[29].convert(&v . A30);

#define MSGPACK_EXTERNAL_UNPACK_31( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28,  A29,  A30,  A31) \
	o.via.array.ptr[0].convert(&v . A1);o.via.array.ptr[1].convert(&v . A2);o.via.array.ptr[2].convert(&v . A3);o.via.array.ptr[3].convert(&v . A4);o.via.array.ptr[4].convert(&v . A5);o.via.array.ptr[5].convert(&v . A6);o.via.array.ptr[6].convert(&v . A7);o.via.array.ptr[7].convert(&v . A8);o.via.array.ptr[8].convert(&v . A9);o.via.array.ptr[9].convert(&v . A10);o.via.array.ptr[10].convert(&v . A11);o.via.array.ptr[11].convert(&v . A12);o.via.array.ptr[12].convert(&v . A13);o.via.array.ptr[13].convert(&v . A14);o.via.array.ptr[14].convert(&v . A15);o.via.array.ptr[15].convert(&v . A16);o.via.array.ptr[16].convert(&v . A17);o.via.array.ptr[17].convert(&v . A18);o.via.array.ptr[18].convert(&v . A19);o.via.array.ptr[19].convert(&v . A20);o.via.array.ptr[20].convert(&v . A21);o.via.array.ptr[21].convert(&v . A22);o.via.array.ptr[22].convert(&v . A23);o.via.array.ptr[23].convert(&v . A24);o.via.array.ptr[24].convert(&v . A25);o.via.array.ptr[25].convert(&v . A26);o.via.array.ptr[26].convert(&v . A27);o.via.array.ptr[27].convert(&v . A28);o.via.array.ptr[28].convert(&v . A29);o.via.array.ptr[29].convert(&v . A30);o.via.array.ptr[30].convert(&v . A31);



#define MSGPACK_EXTERNAL_PACK_1( A1) \
	o.pack(v . A1);

#define MSGPACK_EXTERNAL_PACK_2( A1,  A2) \
	o.pack(v . A1);o.pack(v . A2);

#define MSGPACK_EXTERNAL_PACK_3( A1,  A2,  A3) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);

#define MSGPACK_EXTERNAL_PACK_4( A1,  A2,  A3,  A4) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);

#define MSGPACK_EXTERNAL_PACK_5( A1,  A2,  A3,  A4,  A5) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);

#define MSGPACK_EXTERNAL_PACK_6( A1,  A2,  A3,  A4,  A5,  A6) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);

#define MSGPACK_EXTERNAL_PACK_7( A1,  A2,  A3,  A4,  A5,  A6,  A7) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);

#define MSGPACK_EXTERNAL_PACK_8( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);

#define MSGPACK_EXTERNAL_PACK_9( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);

#define MSGPACK_EXTERNAL_PACK_10( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);

#define MSGPACK_EXTERNAL_PACK_11( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);

#define MSGPACK_EXTERNAL_PACK_12( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);

#define MSGPACK_EXTERNAL_PACK_13( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);

#define MSGPACK_EXTERNAL_PACK_14( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);

#define MSGPACK_EXTERNAL_PACK_15( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);

#define MSGPACK_EXTERNAL_PACK_16( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);

#define MSGPACK_EXTERNAL_PACK_17( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);

#define MSGPACK_EXTERNAL_PACK_18( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);

#define MSGPACK_EXTERNAL_PACK_19( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);

#define MSGPACK_EXTERNAL_PACK_20( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);

#define MSGPACK_EXTERNAL_PACK_21( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);

#define MSGPACK_EXTERNAL_PACK_22( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);

#define MSGPACK_EXTERNAL_PACK_23( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);

#define MSGPACK_EXTERNAL_PACK_24( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);

#define MSGPACK_EXTERNAL_PACK_25( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);o.pack(v . A25);

#define MSGPACK_EXTERNAL_PACK_26( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);o.pack(v . A25);o.pack(v . A26);

#define MSGPACK_EXTERNAL_PACK_27( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);o.pack(v . A25);o.pack(v . A26);o.pack(v . A27);

#define MSGPACK_EXTERNAL_PACK_28( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);o.pack(v . A25);o.pack(v . A26);o.pack(v . A27);o.pack(v . A28);

#define MSGPACK_EXTERNAL_PACK_29( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28,  A29) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);o.pack(v . A25);o.pack(v . A26);o.pack(v . A27);o.pack(v . A28);o.pack(v . A29);

#define MSGPACK_EXTERNAL_PACK_30( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28,  A29,  A30) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);o.pack(v . A25);o.pack(v . A26);o.pack(v . A27);o.pack(v . A28);o.pack(v . A29);o.pack(v . A30);

#define MSGPACK_EXTERNAL_PACK_31( A1,  A2,  A3,  A4,  A5,  A6,  A7,  A8,  A9,  A10,  A11,  A12,  A13,  A14,  A15,  A16,  A17,  A18,  A19,  A20,  A21,  A22,  A23,  A24,  A25,  A26,  A27,  A28,  A29,  A30,  A31) \
	o.pack(v . A1);o.pack(v . A2);o.pack(v . A3);o.pack(v . A4);o.pack(v . A5);o.pack(v . A6);o.pack(v . A7);o.pack(v . A8);o.pack(v . A9);o.pack(v . A10);o.pack(v . A11);o.pack(v . A12);o.pack(v . A13);o.pack(v . A14);o.pack(v . A15);o.pack(v . A16);o.pack(v . A17);o.pack(v . A18);o.pack(v . A19);o.pack(v . A20);o.pack(v . A21);o.pack(v . A22);o.pack(v . A23);o.pack(v . A24);o.pack(v . A25);o.pack(v . A26);o.pack(v . A27);o.pack(v . A28);o.pack(v . A29);o.pack(v . A30);o.pack(v . A31);



#ifndef _MSC_VER
// sane compiler

#define MSGPACK_EXTERNAL_UNPACK_(num, ...) \
	MSGPACK_EXTERNAL_UNPACK_##num(__VA_ARGS__)

#define MSGPACK_EXTERNAL_UNPACK(num, ...) \
	MSGPACK_EXTERNAL_UNPACK_(num, __VA_ARGS__)

#define MSGPACK_EXTERNAL_PACK_(num, ...) \
	MSGPACK_EXTERNAL_PACK_##num(__VA_ARGS__)

#define MSGPACK_EXTERNAL_PACK(num, ...) \
	MSGPACK_EXTERNAL_PACK_(num, __VA_ARGS__)

#if MSGPACK_VERSION_MAJOR < 1
// fmTODO: remove me

#define MSGPACK_DEFINE_EXTERNAL(external, ...) \
	namespace msgpack { \
	inline external& operator>> (object o, external& v) \
	{ \
		if(o.type != type::ARRAY) { throw type_error(); } \
		if(o.via.array.size != MSGPACK_VA_NUM_ARGS(__VA_ARGS__)) { throw type_error(); } \
		MSGPACK_EXTERNAL_UNPACK(MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__) \
		return v;\
	}\
	template <typename Stream> \
	inline packer<Stream>& operator<< (packer<Stream>& o, const external& v) \
	{ \
		o.pack_array(MSGPACK_VA_NUM_ARGS(__VA_ARGS__)); \
		MSGPACK_EXTERNAL_PACK(MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__) \
		return o; \
	} \
	}

#else

#define MSGPACK_DEFINE_EXTERNAL(external, ...) \
	namespace msgpack { MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) { namespace adaptor { \
	template<> struct convert<external> { \
		msgpack::object const& operator()(msgpack::object const& o, external& v) const { \
			if(o.type != type::ARRAY) { throw type_error(); } \
			if(o.via.array.size != MSGPACK_VA_NUM_ARGS(__VA_ARGS__)) { throw type_error(); } \
			MSGPACK_EXTERNAL_UNPACK(MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__) \
			return o; \
		} \
	}; \
	template<> struct pack<external> { \
	template <typename Stream> \
		packer<Stream>& operator()(msgpack::packer<Stream>& o, external const& v) const { \
			o.pack_array(MSGPACK_VA_NUM_ARGS(__VA_ARGS__)); \
			MSGPACK_EXTERNAL_PACK(MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__) \
			return o; \
		} \
	}; \
	}}}

#endif

#else
// insane compiler

#define MSGPACK_EXPAND(x) x

#define MSGPACK_EXTERNAL_UNPACK_(num, ...) \
	MSGPACK_EXTERNAL_UNPACK_##num MSGPACK_EXPAND((__VA_ARGS__))

#define MSGPACK_EXTERNAL_UNPACK(num, ...) \
	MSGPACK_EXTERNAL_UNPACK_(num, __VA_ARGS__)

#define MSGPACK_EXTERNAL_PACK_(num, ...) \
	MSGPACK_EXTERNAL_PACK_##num MSGPACK_EXPAND((__VA_ARGS__))

#define MSGPACK_EXTERNAL_PACK(num, ...) \
	MSGPACK_EXTERNAL_PACK_(num, __VA_ARGS__)

// https://github.com/msgpack/msgpack-c/wiki/v1_1_cpp_adaptor#non-intrusive-approach

#if MSGPACK_VERSION_MAJOR < 1
// fmTODO: remove me

#define MSGPACK_DEFINE_EXTERNAL(external, ...) \
	namespace msgpack { \
	inline external& operator>> (object o, external& v) \
	{ \
		if(o.type != type::ARRAY) { throw type_error(); } \
		if(o.via.array.size != MSGPACK_VA_NUM_ARGS(__VA_ARGS__)) { throw type_error(); } \
		MSGPACK_EXTERNAL_UNPACK MSGPACK_EXPAND((MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__)) \
		return v;\
	}\
	template <typename Stream> \
	inline packer<Stream>& operator<< (packer<Stream>& o, const external& v) \
	{ \
		o.pack_array(MSGPACK_VA_NUM_ARGS(__VA_ARGS__)); \
		MSGPACK_EXTERNAL_PACK MSGPACK_EXPAND((MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__)) \
		return o; \
	} \
	}

#else
#define MSGPACK_DEFINE_EXTERNAL(external, ...) \
	namespace msgpack { MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) { namespace adaptor { \
	template<> struct convert<external> { \
		msgpack::object const& operator()(msgpack::object const& o, external& v) const { \
			if(o.type != type::ARRAY) { throw type_error(); } \
			if(o.via.array.size != MSGPACK_VA_NUM_ARGS(__VA_ARGS__)) { throw type_error(); } \
			MSGPACK_EXTERNAL_UNPACK MSGPACK_EXPAND((MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__)) \
			return o; \
		} \
	}; \
	template<> struct pack<external> { \
	template <typename Stream> \
		packer<Stream>& operator()(msgpack::packer<Stream>& o, external const& v) const { \
			o.pack_array(MSGPACK_VA_NUM_ARGS(__VA_ARGS__)); \
			MSGPACK_EXTERNAL_PACK MSGPACK_EXPAND((MSGPACK_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__)) \
			return o; \
		} \
	}; \
	}}}

#endif


#endif


#endif
