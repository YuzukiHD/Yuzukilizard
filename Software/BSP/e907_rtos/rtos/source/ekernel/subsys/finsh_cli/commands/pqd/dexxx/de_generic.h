/*
Copyright (c) 2020-2030 allwinnertech.com JetCui<cuiyuntao@allwinnertech.com>

*/
enum {
	PQ_SET_REG = 0x1,
	PQ_GET_REG = 0x2,
	PQ_ENABLE = 0x3,
	PQ_COLOR_MATRIX = 0x4,
};

struct trans_base {
	int id;
	int xxx;
}__attribute__ ((packed));

struct ic_base {
	int id;
	int ic;
	int de;
}__attribute__ ((packed));

struct Regiser_pair{
	int offset;
	int value;
}__attribute__ ((packed));

struct Regiser_single {
	int id;
	struct Regiser_pair reg;
}__attribute__ ((packed));

struct Regiser_set {
	int id;
	int count;
	struct Regiser_pair set[0];
}__attribute__ ((packed));

struct capture {
	int id;
	int cap_formate;
	int size;
	char data[0];
}__attribute__ ((packed));


