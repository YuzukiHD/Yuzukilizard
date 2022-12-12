#ifndef AW_IMAGE_H
#define AW_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*
* A struct that stores a bounding box with its coordinate points, width, height, etc.
*
* tl_x:    the horizontal ordinate of top left point.
* tl_y:    the vertical ordinate of top left point.
* br_x:    the horizontal ordinate of bottom right point.
* br_y:    the vertical ordinate of bottom right point.
* width:   the width of the bounding box.
* height:  the height of the bounding box.
* area:    the area of the bounding box.
*/
typedef struct _aw_box AW_Box;
struct _aw_box
{
    int tl_x;   // top_left_x
    int tl_y;   // top_left_y
    int br_x;   // bottom_right_x
    int br_y;   // bottom_right_y

    int width;  // width = br_x - tl_x
    int height; // height = br_y - tl_y

    int area;   // area = width * height
};

/*
* A struct that stores the landmark points of a face.
*
* num:   the number of landmark points.
* cx:    the horizontal ordinate of landmark points.
* cy:    the vertical ordinate of landmark points.
*/
typedef struct _aw_face_landmarks AW_LDMK;
struct _aw_face_landmarks
{
    int num;
    short *cx;
    short *cy;
};

/*
* A enum that stores the color space of an AW_Img.
*/
typedef enum
{
    NONE_SPACE, // Default is none.
    GRAY,
    GRAY3, // Which gray image is duplicated to 3 channels.
    BGR,
    RGB,
    HSV,
    YCRCB
} AW_COLOR_SPACE;

/*
* A enum that stores the YUV types.
*/
typedef enum
{
    NONE_YUV, // Default is none.
    YUV420P_I420,
    YUV420P_YV12,
    YUV420SP_NV12,
    YUV420SP_NV21,
    YUV422
} AW_YUV_TYPE;

/*
* A struct that stores the YUV buffer infomation.
*/
typedef struct _aw_yuv_buf AW_Yuv_Buf;
struct _aw_yuv_buf
{
    AW_YUV_TYPE fmt;
    unsigned int h;
    unsigned int w;
    unsigned int c;
    unsigned int phy_addr[3];
    unsigned int vir_addr[3];
    char *str;
};

/*
* A struct that stores the image data(unsigned char)
along with its width, height, channel, color space, etc.
*
* w:        the width of an image.
* h:        the height of an image.
* c:        the channel of an image.
* data:     the unsigned char data of an image.
* c_space:  the color space of an image.
*/
typedef struct _aw_image AW_Img;
struct _aw_image
{
    int w;
    int h;
    int c;

    unsigned char *data;
    AW_COLOR_SPACE c_space;
};

/*
* A struct that stores the image data(float)
along with its width, height, channel, color space, etc.
*
* w:        the width of an image.
* h:        the height of an image.
* c:        the channel of an image.
* data:     the float data of an image.
* c_space:  the color space of an image.
*/
typedef struct _aw_image_float AW_Img_Ft;
struct _aw_image_float
{
    int w;
    int h;
    int c;

    float *data;
    AW_COLOR_SPACE c_space;
};

/***************************************************/
// Make functions.
/***************************************************/
// Make a new AW_LDMK and allocate memory for it.
AW_LDMK aw_make_ldmk(int num_ldmk);
// Make a new AW_IMG and allocate memory for it.
AW_Img aw_make_blank_image(int w, int h, int c);
// Make a new AW_Img_Ft and allocate memory for it.
AW_Img_Ft aw_make_blank_image_ft(int w, int h, int c);
// Make a RGB AW_Img and copy data from an array.
AW_Img aw_make_rgb_image(const unsigned char *rgb_buffer, int w, int h, int c);
// Duplicate an AW_Img from an existing AW_Img.
AW_Img aw_copy_image(const AW_Img *src_img);
// Duplicate an AW_Img_Ft from an existing AW_Img_Ft.
AW_Img_Ft aw_copy_image_ft(const AW_Img_Ft *src_img);

/***************************************************/
// Image operation functions.
/***************************************************/
// Convert YUV buffer to a RGB AW_Img.
AW_Img aw_yuv2rgb(const unsigned char *yuv_buffer, int w, int h, AW_YUV_TYPE yuv_format);
// Convert AW_Img according to the giving AW_COLOR_SPACE.
AW_Img aw_convert_image(const AW_Img *src_img, AW_COLOR_SPACE dst_space);
// Transpose the given AW_Img and generate a new transposed AW_Img.
AW_Img aw_transpose_image(const AW_Img *src_img);
// Resize the given AW_Img and generate a new resized AW_Img.
AW_Img aw_resize_image(const AW_Img *src_img, int out_w, int out_h);
// Crop the given AW_Img and generate a new cropped AW_Img.
AW_Img aw_crop_image(const AW_Img *src_img, const AW_Box *box);

// Normalize the given AW_Img to AW_Img_Ft.
AW_Img_Ft aw_normalize_image(const AW_Img *src_img); // Normalize image data to [0, 1]
// Resize the given AW_Img_Ft and generate a new resized AW_Img_Ft.
AW_Img_Ft aw_resize_image_ft(const AW_Img_Ft *src_img, int out_w, int out_h);

// Reorder AW_Img from pixel-wise to channel-wise buffer.
void aw_pixel_to_channel(const AW_Img *src_img, unsigned char *dst_buffer);

//Resize the width and height of an image according to the reference size.
void aw_resize_advisor(const int w, const int h, const int reference_size, int *w_new, int *h_new);

/***************************************************/
// Load and save.
/***************************************************/
// Save [w, h, c] and data of an AW_Img(RGB) to binary file.
void aw_save_image_bin(const AW_Img *img, const char *file_name);
// Load AW_Img(RGB) from binary file.
AW_Img aw_load_image_bin(const char *file_name);
// Load image from file(e.g. xxx.jpg),
// accepted as RGB(channels = 3) or Gray(channels = 1) AW_Img.
AW_Img aw_load_image_stb(const char *file_name, int channels);
//Load image from buffer, support JPEG
AW_Img aw_load_image_buffer(const unsigned char *img_buffer, const int buffer_size);
/***************************************************/
// Free memory.
/***************************************************/
// Free an AW_LDMK.
void aw_free_ldmk(AW_LDMK *ldmk);
// Free an AW_Img.
void aw_free_image(AW_Img *img);
// Free an AW_Img_Ft.
void aw_free_image_ft(AW_Img_Ft *img);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !AW_IMAGE_H