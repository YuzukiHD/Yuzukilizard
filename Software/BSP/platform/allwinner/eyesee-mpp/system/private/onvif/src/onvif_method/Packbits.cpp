#include "Packbits.h"
#include <string.h>

static signed char* Pack_init(unsigned char Pack[], unsigned char Byte);
static signed char* Pack_byte(signed char* Count, unsigned char Byte);
static unsigned char* End_byte(signed char* Count);

int tiff6_PackBits(unsigned char array[], int count, unsigned char Pack[]) {
	int i = 0;
	signed char* Count = Pack_init(Pack, array[i]);
	i++;
	for (; i < count; i++)
		Count = Pack_byte(Count, array[i]);
	unsigned char* End = End_byte(Count);
	*End = '\0';
	return (End - Pack);
}

static int unPack_count(const char Pack[], int count);

int tiff6_unPackBits(const char Pack[], int count, unsigned char array[] /*= NULL*/) {
	if (!array)
		return unPack_count(Pack, count);
	int nRes = 0;
	signed char* Count = (signed char*) Pack;
	while ((char*) Count < (Pack + count)) {
		int c = *Count;
		if (c < 0) {
			int n = (1 - c);
			memset(&(array[nRes]), Count[1], n);
			nRes += n;
		} else {
			int n = (1 + c);
			memcpy(&(array[nRes]), &Count[1], n);
			nRes += n;
		}
		Count = (signed char*) End_byte(Count);
	}
	return nRes;
}

int unPack_count(const char Pack[], int count) {
	int nRes = 0;
	signed char* Count = (signed char*) Pack;
	while ((char*) Count < (Pack + count)) {
		int c = *Count;
		if (c < 0)
			nRes += (1 - c);
		else
			nRes += (1 + c);
		Count = (signed char*) End_byte(Count);
	}
	return nRes;
}

signed char* Pack_init(unsigned char Pack[], unsigned char Byte) {
	signed char* Cnt = (signed char*) Pack;
	*Cnt = 0;
	Pack[1] = Byte;
	return Cnt;
}

unsigned char* End_byte(signed char* Count) {
	unsigned char* Pack = (unsigned char*) (Count + 1);
	signed char c = *Count;
	if (c > 0)
		Pack = &(Pack[c + 1]);
	else
		Pack = &(Pack[1]);
	return Pack;
}

signed char* Pack_byte(signed char* Count, unsigned char Byte) {
	signed char c = *Count;
	unsigned char* End = End_byte(Count);
	if (127 == c || c == -127)
		return Pack_init(End, Byte);
	else {
		unsigned char* Pack = End - 1;
		if (*(Pack) == Byte) {
			if (c > 0) {
				(*Count) = c - 1;
				Count = Pack_byte(Pack_init(Pack, Byte), Byte);
			} else
				(*Count)--;
		} else {
			if (c >= 0) {
				*End = Byte;
				(*Count)++;
			} else
				Count = Pack_init(End, Byte);
		}
	}
	return Count;
}
