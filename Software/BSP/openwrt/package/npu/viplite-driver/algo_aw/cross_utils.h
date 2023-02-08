#ifndef __CROSS_UTILS_
#define __CROSS_UTILS_
#ifdef __cplusplus
       extern "C" {
#endif

/*
 * x1: 线段起点x坐标
 * y1: 线段起点y坐标
 * x2: 线段终点x坐标
 * y2: 线段终点y坐标
 * x: 检测点x坐标
 * y: 检测点y坐标
 * return: 0检测点在线段左边且在线段范围内，1右边范围内，2左边范围外，3右边范围外
 **/
int line_side(int x1, int y1, int x2, int y2, int x, int y);

/*
 * nvert: 多边形边数
 * vertx: 多边形顶点x坐标数组
 * verty: 多边形顶点y坐标数组
 * testx: 检测点x坐标
 * testy: 检测点y坐标
 * return: 点在多边形区域内:0,多边形区域外:1
 **/
int pnpoly(int nvert, int *vertx, int *verty, int testx, int testy);

#ifdef __cplusplus
}
#endif

#endif