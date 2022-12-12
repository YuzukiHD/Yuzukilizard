#ifndef _REGISTER_OPT_H_V100_
#define _REGISTER_OPT_H_V100_

/*
 * register operation
 * set register config fiirst, then read or write registers
 */

/*
 * set register config
 * returns 0 if OK, others if something went wrong
 */
int set_reg_opt(const char *sensor_name, int addr_width, int data_width);
/*
 * read register
 * returns data, -1 if something went wrong
 */
int read_reg(int reg_addr, int data_width);
/*
 * write register
 * returns 0 if OK, others if something went wrong
 */
int write_reg(int reg_addr, int reg_data);


#endif /* _REGISTER_OPT_H_V100_ */

