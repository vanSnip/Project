#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>

using namespace std;

int main()
{
    /* comment block


    */
    long seconds = time(nullptr);
    srand(seconds);

    short number = rand() % 100; // random number between 0 and 99
    int new_val{number};
    cout << number << endl;
    return 0;
}