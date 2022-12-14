/*
 * ===========================================================================================
 *
 *       Filename:  foobar.cpp
 *
 *    Description:  just test for support of cpp compile in kbuild.
 *
 *        Version:  Melis3.0
 *         Create:  2019-12-04 17:27:00
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-03-05 15:12:46
 *
 * ===========================================================================================
 */
//C++ file
class foolbar
{
    private:
        int a, b;
    public:
        foolbar(void) {
            a = b = 1;
        }

        foolbar(int m, int n) {
            a = b = 1;
        }
};


int test_func(void)
{
    foolbar obj;
    foolbar *pobj = new foolbar(0, 1);
    return 0;
}
