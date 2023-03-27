#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/fb.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>

struct framebuffer_info {
    uint32_t bits_per_pixel;
    uint32_t xres_virtual;
};

struct framebuffer_info get_framebuffer_info(const char* framebuffer_device_path)
{
    struct framebuffer_info info;
    struct fb_var_screeninfo screen_info;
    int fd = -1;
    fd = open(framebuffer_device_path, O_RDWR);
    if (fd >= 0) {
        if (!ioctl(fd, FBIOGET_VSCREENINFO, &screen_info)) {
            info.xres_virtual = screen_info.xres_virtual;
            info.bits_per_pixel = screen_info.bits_per_pixel;
        }
    }
    return info;
};

int main(int, char**)
{
    const int frame_width = 240;
    const int frame_height = 320;
    const int frame_rate = 10;
    framebuffer_info fb_info = get_framebuffer_info("/dev/fb0");
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Could not open video device." << std::endl;
        return 1;
    } else {
        std::cout << "Successfully opened video device." << std::endl;
        cap.set(cv::CAP_PROP_FRAME_WIDTH, frame_width);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, frame_height);
        cap.set(cv::CAP_PROP_FPS, frame_rate);
        std::ofstream ofs("/dev/fb0");
        cv::Mat frame;
        cv::Mat rotation_fream;
        while (true) {
            cap >> frame;
            if (frame.depth() != CV_8U) {
                std::cerr << "Not 8 bits per pixel and channel." << std::endl;
            } else if (frame.channels() != 3) {
                std::cerr << "Not 3 channels." << std::endl;
            } else {
                cv::transpose(frame, rotation_fream);
                cv::flip(rotation_fream, frame, 0);
                int framebuffer_width = fb_info.xres_virtual;
                int framebuffer_depth = fb_info.bits_per_pixel;
                cv::Size2f frame_size = frame.size();
                cv::Mat framebuffer_compat;
                switch (framebuffer_depth) {
                case 16:
                    cv::cvtColor(frame, framebuffer_compat, cv::COLOR_BGR2BGR565);
                    for (int y = 0; y < frame_size.height; y++) {
                        ofs.seekp(y * framebuffer_width * 2);
                        ofs.write(reinterpret_cast<char*>(framebuffer_compat.ptr(y)), frame_size.width * 2);
                    }
                    break;
                case 32: {
                    std::vector<cv::Mat> split_bgr;
                    cv::split(frame, split_bgr);
                    split_bgr.push_back(cv::Mat(frame_size, CV_8UC1, cv::Scalar(255)));
                    cv::merge(split_bgr, framebuffer_compat);
                    for (int y = 0; y < frame_size.height; y++) {
                        ofs.seekp(y * framebuffer_width * 4);
                        ofs.write(reinterpret_cast<char*>(framebuffer_compat.ptr(y)), frame_size.width * 4);
                    }
                } break;
                default:
                    std::cerr << "Unsupported depth of framebuffer." << std::endl;
                }
            }
        }
    }
}