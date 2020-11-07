#include<iostream>
#include <sys/time.h>
#include <stdio.h>
#include<string>
class Date
{
  public:
     Date()
     {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int64_t seconds = tv.tv_sec;
        microSeconds_=seconds*kMicroSecondsPerSecond+tv.tv_usec;
        time_t seconds2 = static_cast<time_t>(microSeconds_ / kMicroSecondsPerSecond);
        struct tm tm_time;
        localtime_r(&seconds2, &tm_time);
        year=tm_time.tm_year + 1900;
        month=tm_time.tm_mon + 1;
        day=tm_time.tm_mday;
        hours=tm_time.tm_hour;
        minute=tm_time.tm_min;
        second_=tm_time.tm_sec;
        std::cout<<year<<"."<<month<<"."<<day<<" "<<hours<<":"<<minute<<":"<<second_<<std::endl;
        
     }
    Date& operator<<(const std::string s)
    {
        std::cout<<year<<"."<<month<<"."<<day<<" "<<hours<<":"<<minute<<":"<<second_<<a<<s<<std::endl; 
    }
    Date& operator>>(const std::string s)
    {
         a+=s;
    }
    void operator+(int n)
    {
        minute+=n;
        while(minute>60)
        {
            hours++;
            minute-=60;
        }
        while(hours>24)
        {
            day++;
            hours-=24;
        }
        /*.....*/
    }
    private:
    int year;
    int month;
    int day;
    int hours;
    int minute;
    int second_;
    std::string a;
    static const int kMicroSecondsPerSecond = 1000 * 1000;
    int64_t microSeconds_;
};
int main()
{
    Date a;
    a<<" now";
    a>>" will +1 minute";
    a<<"";
    a+(1);
    a>>" done";
    a<<"";
}
