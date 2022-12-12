#ifndef _SERVER_API_H_V100_
#define _SERVER_API_H_V100_


/*
 * socket communication
 * init server, run server and handle client connection events
 * author: clarkyy
 * date: 20161110
 */

/*
 * init server
 * returns 0 if init success, <0 if something went wrong
 */
int init_server(int port);

/*
 * run server
 * this function will be running until get some exit message from client/server
 * returns 0 if exit normal, <0 if something went wrong
 */
int run_server();

/*
 * exit server
 * clean and exit
 */
void exit_server();


#endif /* _SERVER_API_H_V100_ */