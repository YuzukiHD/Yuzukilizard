// Packbit algorithm, make sure the _countof(Pack[*]) >= (count + count/128 +2);
// return _countof(Pack[*]) actual count
// AVC_FMAC
int  tiff6_PackBits(unsigned char array[], int count, unsigned char Pack[]);
// return _countof(array[*]);
// AVC_FMAC
int  tiff6_unPackBits(const char Pack[], int count, unsigned char array[] /*= NULL*/);
