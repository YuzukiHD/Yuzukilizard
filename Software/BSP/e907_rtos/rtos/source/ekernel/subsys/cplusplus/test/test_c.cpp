#include "../ekernel/core/rt-thread/include/rtthread.h"

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <forward_list>
#include <set>
#include <map>

#include <numeric>
#include <algorithm>

#include <memory>

//#include <thread>
//#include <mutex>

using namespace std;

class Test;

class rtt
{
    public:
        weak_ptr<Test> testptr;

        rtt() {
            i = 1;
            cout << "rtt create" << endl;
        }
        rtt(int x) {
            i = x;
            cout << "rtt create, i=" << i << endl;
        }
        void print(void) {
            i++;
            cout << "++i = " << i << endl;
        }
        ~rtt() {
            cout << "rtt delete" << endl;
        }
    private:
        int i;
};


class Test
{
    public:
        shared_ptr<rtt> rttptr;
        Test(string s) {
            str = s;
            cout << "Test creat" << endl;
        }
        ~Test() {
            cout << "Test delete:" << str << endl;
        }
        string &getStr() {
            return str;
        }
        void setStr(string s) {
            str = s;
        }
        void print() {
            cout << str << endl;
        }
    private:
        string str;
};

template <typename T> void showvector(vector<T> v)
{
    for (typename vector<T>::iterator it = v.begin(); it != v.end(); it++)
    {
        cout << *it << " ";
    }
    cout << endl;
}

template <typename T> void showlist(list<T> v)
{
    for (typename list<T>::iterator it = v.begin(); it != v.end(); it++)
    {
        cout << " " << *it;
    }
    cout << endl;
}

template <typename T> void showforwardlist(forward_list<T> v)
{
    for (typename forward_list<T>::iterator it = v.begin(); it != v.end(); it++)
    {
        cout << " " << *it;
    }
    cout << endl;
}

template <typename T> void showset(set<T> v)
{
    for (typename set<T>::iterator it = v.begin(); it != v.end(); it++)
    {
        cout << *it << " ";
    }
    cout << endl;
}

void showmap(map<string, int> v)
{
    for (map<string, int>::iterator it = v.begin(); it != v.end(); it++)
    {
        cout << it->first << "  " << it->second << endl;
    }
    cout << endl;
}

static void ctors_test(void)
{
    int *p = new int[5];
    int i;
    for (i = 0; i < 5; i++)
    {
        p[i] = i + 1;

        cout << "p[i] = " << p[i] << endl;
    }
    delete[] p;

    rtt test2;

    rtt *test3 = new rtt();
    if (test3 != NULL)
    {
        delete test3;
    }
    cout << "delete test3" << endl;

    cout << "new and delete rtt[5]---------" << endl;
    cout << "new rtt[5]" << endl;
    rtt *test4 = new rtt[5];
    cout << "delete rtt[5]" << endl;
    delete []test4;
}

static void string_test(void)
{
    cout << "string------------" << endl;
    string s1("hello");
    string s2 = s1;
    string s3 = "world";
    string s4(5, 'a');
    string s5 = s2 + "my cpp test";
    cout << "s1 " << s1 << endl;
    cout << "s2 " << s2 << endl;
    cout << "s3 " << s3 << endl;
    cout << "s4 " << s4 << endl;
    cout << "s5:" << s5 << "length:" << s5.length() << "size:" << s5.size() << endl;

    for (string::iterator it = s1.begin(); it != s1.end(); it++)
    {
        *it = toupper(*it);
    }
    cout << "s1 toupper " << s1 << endl;

    if (s5.find("pp", 0) != string::npos)
    {
        cout << "s5 find pp: " << s5.find("pp", 0) << endl;
    }
}

static void vec_test(void)
{
    cout << "vectot ----------------" << endl;
    vector<string> vstr = {"this", " is", " test", " for", " vector", " em....."};
    showvector(vstr);
    vstr.resize(5);
    showvector(vstr);

    vector<int> vint = {1, 2, 3, 4, 5};
    showvector(vint);
    cout << "vint.front:" << vint.front() << " vint.back:" << vint.back() << endl;
    cout << "sum:" << accumulate(vint.begin(), vint.end(), 0) << endl;

    reverse(vint.begin(), vint.end());
    showvector(vint);
    vint.pop_back();
    showvector(vint);
    vint.push_back(6);
    showvector(vint);
    vint.insert(vint.begin() + 1, 2, 18);
    showvector(vint);
    vint.erase(vint.begin() + 3);
    showvector(vint);
    sort(vint.begin(), vint.end());
    showvector(vint);
    vint.clear();
    showvector(vint);
}

static void list_test(void)
{
    cout << "list--------------" << endl;
    list<int> lint{5, 88, 13, 105, 64, 77};
    showlist(lint);
    lint.sort();
    showlist(lint);
}

static void forwardlist_test(void)
{
    cout << "forward_list--------------" << endl;
    forward_list<int> list1 = { 1, 2, 3, 4 };
    forward_list<int> list2 = { 77, 88, 99 };

    list2.insert_after(list2.before_begin(), 99);
    list2.push_front(10);
    showforwardlist(list2);
    list2.sort();
    list2.unique();
    list1.merge(list2);
    showforwardlist(list1);
}

static void set_test(void)
{
    cout << "set-------------------" << endl;
    set<int> sint{91, 8, 341, 3, 3, 4, 3, 3, 4, 5, 66, 6, 1, 5, 5, 6, 7, 7};
    showset(sint);
    sint.insert(66);
    showset(sint);
    sint.insert(888);
    showset(sint);
}

static void map_test(void)
{
    cout << "map--------------" << endl;
    map<string, int> si;
    si["year"] = 2019;
    si["month"] = 12;
    si["day"] = 16;
    showmap(si);
    si.insert(pair<string, int>("hour", 17));
    showmap(si);
    si.erase("day");
    showmap(si);
    if (si.count("year"))
    {
        cout << "year is in map" << endl;
    }
    else
    {
        cout << "year is not in map" << endl;
    }
    si.clear();
    showmap(si);
}


static unique_ptr<Test> fun()
{
    return unique_ptr<Test>(new Test("789"));
}

static void uniqueptr_test(void)
{
    cout << "unique_ptr------------" << endl;
    unique_ptr<Test> ptest(new Test("123"));
    unique_ptr<Test> ptest2(new Test("456"));
    ptest->print();
    ptest2 = move(ptest);
    if (ptest == NULL)
    {
        cout << "ptest = NULL" << endl;
    }
    Test *p = ptest2.release();
    p->print();
    ptest.reset(p);
    ptest->print();
    ptest2 = fun();
    ptest2->print();
}

static void shareptr_test(void)
{
    cout << "share_ptr------------" << endl;
    shared_ptr<Test> ptest(new Test("123"));
    shared_ptr<Test> ptest2(new Test("456"));
    cout << ptest2->getStr() << endl;
    cout << "ptest2.use_count:" << ptest2.use_count() << endl;
    ptest = ptest2;
    ptest->print();
    cout << "ptest2.use_count:" << ptest2.use_count() << endl;
    cout << "ptest.use_count:" << ptest.use_count() << endl;
    ptest.reset();
    ptest2.reset();
}

static void weakptr_test(void)
{
    cout << "weak_ptr------------" << endl;
    shared_ptr<Test> tp(new Test("haha"));
    shared_ptr<rtt> rp(new rtt(66));
    tp->rttptr = rp;
    rp->testptr = tp;
    cout << "tp->rttptr.use_count:" << tp->rttptr.use_count() << endl;
    cout << "rp->testptr.use_count:" << rp->testptr.use_count() << endl;

    shared_ptr<int> sptr;
    sptr.reset(new int);
    *sptr = 10;
    weak_ptr<int> weak1 = sptr;
    sptr.reset(new int);
    *sptr = 5;
    weak_ptr<int> weak2 = sptr;
    // weak1 is expired!
    if (auto tmp = weak1.lock())
    {
        cout << "weak1:" << *tmp << '\n';
    }
    else
    {
        cout << "weak1 is expired\n";
    }
    // weak2 points to new data (5)
    if (auto tmp = weak2.lock())
    {
        cout << "weak2:" << *tmp << '\n';
    }
    else
    {
        cout << "weak2 is expired\n";
    }
}



static void cpptest(void)
{
    cout << "welcome to the world of cplus!" << endl;

    ctors_test();

    string_test();

    vec_test();

    list_test();

    forwardlist_test();

    set_test();

    map_test();

    uniqueptr_test();

    shareptr_test();

    weakptr_test();


    cout << "exit main------------" << endl;
}

FINSH_FUNCTION_EXPORT_ALIAS(cpptest, __cmd_cpptest, cpp test command);
