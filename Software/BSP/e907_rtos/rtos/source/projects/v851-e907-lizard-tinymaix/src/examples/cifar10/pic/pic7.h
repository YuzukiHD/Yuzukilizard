const unsigned char cifar10_pic[32*32*3]={\
 46, 48, 56,  63, 66, 71,  84, 88, 89,  96,103, 96,  98,108, 92, 113,125,107, 128,137,127, 200,208,208, 242,248,253, 235,244,248, 240,248,248, 226,234,233, 219,225,224, 218,223,221, 211,215,209, 197,203,192, 186,195,192, 169,177,176, 160,166,165, 174,180,179, 203,208,207, 200,202,202, 204,206,206, 204,204,204, 196,196,196, 206,206,206, 209,210,208, 210,211,209, 157,158,154,  60, 64, 59,  49, 53, 48,  40, 44, 39, 
  9, 11, 11,  17, 19, 19,  14, 18, 13,  35, 42, 27,  55, 64, 43,  93,105, 83, 133,144,128, 175,184,181, 178,186,186, 168,178,178, 161,172,169, 146,157,154, 136,145,142, 141,148,143, 131,141,129, 127,137,121, 122,130,129, 127,133,132, 126,132,131, 166,171,170, 227,232,231, 233,235,235, 239,241,241, 241,241,241, 247,247,247, 241,241,241, 246,247,245, 253,254,252, 165,166,162,  27, 31, 26,  21, 25, 20,  21, 25, 20, 
 35, 38, 23,  36, 39, 24,  31, 35, 16,  55, 64, 37, 101,112, 79, 113,126, 94, 127,139,117, 129,138,128, 141,150,147, 130,141,139, 124,138,134, 132,146,142, 132,146,142, 151,162,159, 191,205,194, 200,215,201, 208,214,213, 219,225,224, 187,193,192, 162,167,166, 241,246,245, 247,249,249, 247,249,249, 254,254,254, 253,253,253, 249,249,249, 251,252,250, 250,251,249, 162,163,159,  28, 32, 27,  19, 23, 18,  18, 22, 17, 
 74, 85, 53,  77, 88, 56,  66, 77, 44,  89,102, 64, 100,114, 72,  93,108, 70, 122,135,109, 204,214,201, 210,219,216, 133,141,140, 106,117,115,  95,106,104,  79, 88, 91, 126,135,138, 229,239,239, 243,254,251, 245,251,250, 246,252,251, 140,145,144, 118,123,122, 241,246,245, 242,244,244, 249,251,251, 247,249,249, 251,251,251, 248,248,248, 253,254,252, 255,255,254, 155,156,154,  26, 27, 23,  14, 18, 13,  15, 19, 14, 
 77, 95, 56,  81, 98, 61,  72, 89, 52,  84,102, 63,  97,115, 76,  93,109, 75, 120,132,110, 219,228,218, 199,205,204,  86, 92, 91,  86, 92, 91,  80, 85, 86,  72, 75, 83, 151,153,163, 241,243,253, 240,244,249, 244,249,248, 231,236,235, 100,105,104, 110,115,114, 230,232,232, 248,250,250, 244,246,246, 240,242,242, 241,241,241, 249,249,249, 247,248,246, 253,254,252, 159,160,158,  31, 32, 28,  13, 14, 10,  26, 27, 23, 
 70, 90, 61,  86,105, 78,  79, 98, 73, 101,117, 93, 160,176,152, 123,135,115, 112,121,111, 216,221,220, 207,210,214, 142,144,144, 143,147,142, 141,144,142, 144,146,147, 206,205,214, 245,244,253, 244,242,248, 245,247,247, 234,236,236, 172,174,174, 180,182,182, 233,235,235, 243,245,245, 201,203,204, 170,172,172, 207,209,209, 254,254,254, 250,250,250, 254,255,253, 153,154,152,  23, 24, 20,  27, 28, 24,  34, 35, 31, 
 76, 96, 77,  99,116,102, 106,122,111, 131,147,136, 160,174,163, 145,156,148, 127,133,132, 207,210,215, 237,238,242, 236,237,235, 235,236,227, 234,235,225, 237,236,232, 249,247,247, 248,245,247, 246,243,245, 238,238,238, 243,243,243, 240,240,240, 230,230,230, 189,191,191, 158,160,160, 149,151,151, 138,140,140, 161,163,163, 175,177,177, 200,202,202, 204,205,203, 126,127,125,  35, 36, 34,  29, 30, 26,  31, 32, 28, 
 66, 83, 62, 105,121,104, 122,136,124, 130,144,132, 155,167,155, 148,156,149, 119,124,123, 205,206,210, 248,247,251, 246,245,241, 252,251,241, 246,245,235, 247,244,239, 246,244,244, 246,243,245, 247,244,246, 249,247,247, 240,238,238, 245,243,243, 202,202,202, 105,105,105,  80, 80, 80,  97, 99, 99, 130,132,132, 166,168,168, 196,198,198, 164,166,166, 161,162,160, 128,129,127,  37, 38, 36,  39, 40, 36,  35, 36, 32, 
 50, 65, 34,  88,103, 75,  89,104, 77,  85,100, 73, 117,132,105, 127,139,117, 115,122,109, 199,200,198, 247,245,245, 246,245,241, 246,244,236, 246,243,238, 246,244,244, 242,238,243, 245,244,248, 243,242,246, 249,244,243, 238,234,233, 178,174,173,  95, 93, 93,  59, 57, 57,  66, 66, 66,  59, 59, 59,  96, 98, 98, 187,189,189, 234,236,236, 170,172,172, 166,168,168, 129,130,128,  47, 48, 46,  51, 52, 50,  46, 44, 43, 
 93,104, 71, 107,118, 85, 102,116, 82,  98,113, 75, 118,137, 94, 117,135, 96, 107,118, 92, 193,197,185, 244,243,239, 249,246,242, 241,238,234, 235,231,230, 244,239,240, 248,245,247, 245,245,245, 247,248,246, 219,212,209, 135,127,127,  73, 68, 67,  54, 49, 48,  53, 49, 48,  48, 46, 46,  47, 47, 47,  80, 82, 82, 184,186,186, 146,148,148, 133,135,135, 140,142,142,  84, 84, 84,  65, 66, 64,  67, 65, 64,  52, 50, 49, 
102,108, 83, 117,126, 99,  95,109, 75,  99,118, 75, 117,138, 89, 110,131, 82, 105,119, 83, 192,198,179, 245,243,235, 179,174,171, 135,128,125, 133,126,123, 149,141,141, 180,173,170, 199,197,187, 164,165,149,  99, 87, 85,  67, 55, 53,  54, 45, 42,  50, 43, 40,  47, 42, 41,  49, 47, 46,  99, 99, 99,  95, 97, 97, 163,165,165,  75, 77, 77, 146,148,148, 125,127,127,  53, 53, 53,  62, 62, 62,  76, 74, 74,  58, 56, 55, 
120,125,110, 146,155,135,  77, 91, 63,  73, 93, 50,  86,110, 56,  89,113, 59,  95,113, 72, 183,191,168, 176,175,165,  94, 85, 82,  77, 65, 63,  70, 57, 55,  73, 61, 57,  74, 63, 55,  67, 59, 42,  56, 52, 28,  57, 42, 39,  64, 52, 48,  65, 54, 50,  59, 50, 47,  52, 45, 42,  90, 86, 85, 188,186,185, 123,123,123, 113,115,115,  51, 53, 53, 118,120,120,  77, 79, 79,  42, 42, 42,  46, 46, 46,  76, 76, 76,  57, 55, 55, 
116,121,106, 147,156,136,  75, 90, 59,  66, 86, 43,  78,105, 49,  86,111, 55,  93,112, 69, 157,166,140, 114,112,101,  76, 64, 62,  71, 56, 54,  56, 38, 39,  64, 43, 45,  76, 57, 54,  54, 38, 26,  43, 29, 11,  51, 35, 29,  75, 59, 53,  79, 64, 61,  65, 54, 50,  58, 51, 48, 150,145,142, 221,219,218, 107,108,106,  80, 82, 82,  62, 64, 64,  68, 70, 70,  53, 55, 55,  33, 35, 35,  30, 30, 30,  64, 64, 64,  52, 50, 50, 
102,105, 83, 116,123, 96,  82, 93, 60,  66, 86, 39,  74, 99, 43,  77,102, 46,  88,108, 63, 121,130,103,  70, 69, 55,  57, 47, 40,  51, 36, 33,  57, 36, 38,  55, 32, 36,  70, 48, 50,  65, 44, 42,  43, 24, 16,  51, 32, 27,  84, 65, 60,  92, 76, 70,  63, 51, 47,  84, 75, 71, 218,213,210, 231,229,228, 150,151,149, 147,149,149,  66, 68, 68,  33, 35, 35,  42, 44, 44,  29, 31, 32,  49, 48, 50,  65, 64, 66,  45, 42, 44, 
105,104, 76, 103,105, 75, 100,107, 72,  93,108, 64,  97,117, 64,  90,113, 59,  99,116, 73,  96,104, 74,  38, 38, 20,  46, 38, 25,  60, 48, 38,  49, 33, 26,  50, 30, 29,  69, 48, 50,  52, 32, 31,  38, 22, 16,  45, 26, 19,  70, 51, 44,  89, 73, 67,  73, 59, 53, 123,112,108, 234,227,224, 226,223,219, 219,220,218, 168,170,170,  40, 42, 42,  27, 32, 31,  39, 41, 42,  37, 39, 40,  57, 56, 58,  60, 59, 61,  41, 38, 40, 
129,122, 95, 119,116, 88, 112,115, 83, 118,128, 86, 119,135, 87, 105,124, 75, 101,115, 74,  66, 74, 44,  34, 36, 14,  40, 36, 17,  62, 55, 36,  43, 32, 18,  46, 30, 23,  49, 32, 29,  41, 27, 21,  33, 20, 12,  44, 22, 16,  59, 40, 33,  86, 68, 61,  74, 60, 54, 142,131,127, 231,225,220, 223,220,216, 207,208,206, 136,138,138,  34, 36, 36,  33, 38, 37,  35, 37, 38,  42, 44, 45,  32, 31, 33,  45, 44, 46,  47, 44, 46, 
115,134, 85, 114,133, 82, 117,137, 84, 116,135, 84, 115,130, 86, 111,119, 88, 118,120,100,  68, 67, 53,  37, 32, 23,  42, 37, 28,  54, 48, 37,  49, 46, 32,  53, 49, 38,  40, 37, 29,  20, 15, 16,  16,  9, 16,  24, 19, 10,  37, 31, 26,  58, 50, 50,  57, 51, 52, 134,129,130, 214,212,211, 152,153,151, 103,102,104,  72, 70, 76,  33, 30, 39,  39, 37, 43,  50, 47, 49,  71, 69, 69,  31, 26, 27,  36, 29, 34,  40, 32, 39, 
117,140, 86, 116,139, 84, 109,133, 75, 114,135, 80, 121,139, 92,  98,109, 76,  74, 79, 58,  42, 42, 30,  41, 38, 30,  40, 37, 29,  75, 70, 61, 155,153,142, 186,184,174, 161,158,154, 118,114,119,  93, 87, 98,  73, 68, 59,  41, 37, 32,  49, 44, 43,  54, 49, 50,  84, 82, 81, 104,105,101,  62, 63, 59,  41, 44, 42,  33, 35, 36,  28, 29, 33,  35, 34, 38,  56, 56, 56,  68, 66, 66,  23, 21, 21,  30, 24, 29,  34, 26, 36, 
117,142, 84, 121,146, 88, 123,149, 89, 121,146, 88, 112,133, 84,  93,110, 73,  70, 79, 58,  41, 45, 34,  42, 41, 37,  49, 46, 41,  53, 52, 42,  97, 97, 85, 133,134,124, 161,160,156, 179,178,182, 187,186,195, 123,121,113,  45, 42, 37,  43, 39, 38,  45, 43, 43,  51, 52, 50,  37, 41, 35,  42, 46, 40,  29, 36, 29,  27, 32, 30,  30, 35, 34,  36, 41, 39,  33, 36, 34,  41, 45, 40,  48, 48, 48,  51, 50, 54,  37, 34, 43, 
120,148, 89, 111,139, 80, 115,143, 84, 119,144, 86, 114,138, 84, 122,140, 99, 106,119, 95,  49, 54, 45,  30, 32, 26,  49, 51, 39,  53, 58, 37,  25, 31,  8,  19, 26,  5,  27, 34, 21,  46, 50, 44,  63, 69, 64,  63, 61, 53,  45, 44, 40,  35, 33, 33,  34, 33, 35,  38, 40, 41,  45, 48, 46,  37, 43, 38,  14, 21, 16,  17, 24, 21,  20, 26, 25,  29, 36, 33,  38, 45, 38,  72, 79, 72,  94,100, 95,  84, 86, 86,  75, 76, 80, 
120,145, 89, 117,142, 86, 120,145, 89, 119,144, 86, 115,140, 84, 115,135, 90,  99,112, 86,  37, 43, 32,  31, 35, 24,  66, 71, 50, 111,120, 87,  99,110, 70,  83, 95, 59,  57, 69, 39,  36, 47, 21,  20, 33,  7,  24, 25, 15,  39, 39, 33,  41, 39, 39,  50, 49, 51,  43, 45, 46,  36, 38, 39,  26, 31, 30,  19, 24, 25,  15, 20, 23,   9, 14, 17,  23, 29, 28,  81, 88, 83, 109,117,107, 119,127,117, 126,133,126, 116,121,119, 
128,153, 93, 128,152, 94, 128,151, 96, 128,153, 93, 129,154, 94, 127,146, 95, 103,116, 84,  36, 41, 26,  36, 39, 24,  95,100, 71, 130,141, 95, 127,143, 89, 122,137, 86, 110,124, 82,  91,107, 66,  75, 92, 49,  64, 66, 46,  47, 48, 32,  52, 53, 43,  58, 57, 53,  41, 42, 40,  21, 24, 22,  33, 35, 35,  35, 38, 42,  22, 24, 32,  19, 20, 30,  33, 36, 41,  98,103,102, 114,120,115, 119,126,119, 124,131,124, 128,132,127, 
137,158, 95, 134,157, 95, 135,157, 98, 133,157, 93, 127,151, 86, 130,149, 94, 102,111, 78,  36, 39, 23,  43, 44, 28, 114,119, 87, 130,142, 90, 127,142, 84, 122,136, 82, 111,124, 78, 104,117, 71,  99,115, 67, 101,106, 74,  62, 67, 38,  56, 59, 37,  83, 87, 68,  73, 76, 60,  51, 56, 41,  34, 38, 27,  46, 49, 47,  33, 33, 39,  21, 20, 30,  85, 84, 94, 134,134,140, 131,133,134, 133,136,134, 127,129,129, 126,128,128, 
136,156, 91, 137,155, 94, 137,157, 98, 136,157, 94, 127,149, 84, 129,146, 89,  93,102, 69,  46, 46, 32,  63, 62, 48, 117,122, 90, 126,138, 86, 120,134, 76, 118,130, 78, 118,128, 85, 114,127, 83, 106,119, 73, 107,116, 76,  72, 80, 43,  83, 90, 55, 109,116, 81, 104,114, 78, 100,109, 76,  70, 80, 50,  51, 57, 38,  24, 28, 22,  31, 32, 36, 133,132,141, 147,146,155, 140,141,145, 140,142,143, 136,137,141, 135,136,140, 
141,159, 98, 137,153, 99, 134,150, 97, 127,146, 89, 127,148, 86, 126,142, 89,  95,103, 72,  61, 61, 47,  81, 81, 67, 104,109, 78, 111,122, 72, 114,127, 71, 113,125, 75, 109,122, 78, 111,124, 80, 102,118, 71,  99,106, 71,  79, 90, 52,  73, 84, 44,  97,110, 66, 100,114, 66, 100,117, 66, 100,116, 69,  78, 92, 56,  31, 40, 19,  77, 81, 75, 161,163,164, 154,158,159, 143,147,148, 139,143,144, 142,145,149, 140,143,147, 
133,150, 99, 130,145,101, 131,145,103, 130,149,100, 133,153,100, 128,146, 99,  99,109, 79,  65, 71, 54,  69, 72, 56,  91,100, 67, 102,118, 65, 107,125, 66, 111,127, 74, 110,126, 78, 109,128, 79, 105,125, 72,  97,107, 77,  76, 87, 55,  65, 79, 43,  94,111, 67, 107,128, 73, 102,124, 65, 100,122, 64, 101,121, 74,  82, 97, 66, 139,149,133, 165,172,167, 153,160,157, 146,153,150, 143,150,147, 141,147,146, 144,149,150, 
125,142, 98, 128,145,102, 132,150,109, 131,151,106, 125,146, 97, 113,133, 88,  88,104, 70,  69, 79, 56,  86, 97, 71, 105,119, 78, 115,134, 77, 114,137, 75, 117,139, 81, 116,139, 85, 115,138, 83, 112,137, 79, 106,120, 92,  83, 98, 67,  85,102, 65, 106,126, 81, 106,129, 74, 109,134, 74, 111,136, 78, 105,127, 79, 132,148,117, 175,186,170, 170,180,174, 157,165,164, 140,148,147, 136,147,145, 137,148,146, 137,147,147, 
122,144, 92, 125,145, 98, 131,152,107, 128,150,102, 122,145, 93, 117,139, 90, 113,133, 90, 112,130, 93, 113,131, 92, 120,141, 92, 124,149, 89, 121,146, 84, 121,147, 87, 122,147, 91, 116,143, 87, 117,145, 85, 114,135, 96, 113,133, 91, 114,134, 91, 112,134, 85, 115,139, 85, 126,151, 93, 124,147, 95, 120,139,100, 155,169,145, 165,176,166, 170,178,177, 161,168,171, 142,151,154, 140,149,152, 143,152,156, 138,147,151, 
129,153, 95, 128,151, 96, 128,152, 98, 125,150, 94, 120,147, 91, 118,145, 89, 118,141, 89, 122,144, 96, 122,144, 95, 125,148, 94, 125,150, 90, 120,146, 86, 122,147, 91, 122,148, 94, 119,145, 91, 121,148, 92, 119,143, 89, 115,139, 85, 115,138, 86, 107,131, 77, 109,133, 79, 117,140, 86, 118,138, 91, 123,139,105, 168,179,159, 163,171,164, 163,168,169, 156,161,164, 150,157,160, 147,153,158, 145,151,158, 144,149,158, 
130,152, 93, 129,151, 92, 127,151, 93, 125,149, 91, 124,149, 91, 123,148, 90, 119,144, 86, 122,145, 90, 132,156, 98, 132,156, 98, 127,151, 93, 126,149, 94, 127,150, 96, 124,147, 95, 120,146, 93, 121,147, 94, 125,150, 88, 115,139, 81, 119,142, 87, 117,137, 84, 112,132, 79, 113,132, 81, 119,137, 90, 124,136,100, 141,150,124, 150,157,142, 153,158,149, 164,168,162, 164,171,164, 162,168,163, 155,160,159, 149,154,157, 
130,148, 95, 131,149, 96, 129,147, 94, 122,141, 90, 122,141, 90, 121,144, 90, 121,144, 89, 128,150, 91, 137,159,100, 138,160,101, 131,153, 95, 130,151, 96, 129,149, 96, 120,143, 89, 119,142, 88, 119,142, 90, 121,140, 83, 127,145, 92, 126,142, 94, 119,135, 87, 129,143, 95, 127,142, 91, 126,138, 90, 131,141, 98, 125,132, 97, 136,142,111, 136,143,116, 150,157,130, 154,160,135, 161,168,147, 168,173,158, 170,174,163, 
130,143, 97, 136,149,103, 138,151,107, 132,147,103, 129,146,102, 128,147, 98, 123,143, 90, 124,146, 87, 133,154, 92, 136,156, 97, 128,147, 90, 129,148, 93, 129,148, 93, 118,139, 84, 119,142, 87, 121,144, 90, 125,140, 89, 128,141, 95, 129,141, 99, 125,135, 93, 129,139, 96, 132,142, 95, 133,144, 94, 128,139, 89, 135,145, 99, 136,145,102, 135,144,101, 135,145,102, 141,151,109, 141,148,113, 139,147,117, 145,151,126, 
};