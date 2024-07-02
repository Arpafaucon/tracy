#include <chrono>
#include <iostream>
#include <syncstream>
#include <thread>
#include "tracy/Tracy.hpp"

using namespace std::chrono_literals;

int print_thread()
{
    ZoneScoped;
    std::thread::id this_id = std::this_thread::get_id();

    std::osyncstream(std::cout) << "thread " << this_id << " sleeping...\n";

    std::this_thread::sleep_for(2500ms);
    return 1;
}


int main()
{
    print_thread();
    std::jthread t1{print_thread};
    std::jthread t2{print_thread};
}
