#include "AdaptiveThreadPool.h"
#include <functional>
#include <unistd.h>
using namespace std;
void foo(int i)
{
    std::cout << "hello " << i << std::endl;
    sleep(1);
}

int main(){

    AdaptiveThreadPool adthread(2, 30, 2.0);
    for(int i=0;i<300;i++){
        adthread.enqueue([i] { foo(i); });
        
    }
    
    return 0;
}