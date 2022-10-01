//
// Created by chengzi on 2022/8/9.
//
// A thread-safe counter

#include <string>
#include <stdio.h>
#include <boost/noncopyable.hpp>

class Counter : boost::noncopyable
{

};

class A{

private:
    int age;
    std::string name;
public:
    A(const int &age_): age(age_) {}
    A(const std::string& ss): name(ss){}
};

void printA(const A& a)
{
    //pringing
}

int main()
{
    printA(std::string("chat"));


    return 0;
}