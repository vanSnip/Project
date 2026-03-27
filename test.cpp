#include <iostream>
#include <vector>
#include <string>

// Function example
int add(int a, int b)
{
    return a + b;
}

int main()
{
    // =========================
    // 1. Text output
    // =========================
    std::cout << "Hello, welcome to basic C++!" << std::endl;

    // =========================
    // 2. Variables
    // =========================
    int x = 10;
    int y = 3;
    double price = 99.95;
    char grade = 'A';
    bool isActive = true;
    std::string company = "ASML";

    std::cout << "\n--- Variables ---" << std::endl;
    std::cout << "x = " << x << ", y = " << y << std::endl;
    std::cout << "price = " << price << std::endl;
    std::cout << "grade = " << grade << std::endl;
    std::cout << "isActive = " << isActive << std::endl;
    std::cout << "company = " << company << std::endl;

    // =========================
    // 3. Arithmetic
    // =========================
    std::cout << "\n--- Arithmetic ---" << std::endl;
    std::cout << "x + y = " << x + y << std::endl;
    std::cout << "x - y = " << x - y << std::endl;
    std::cout << "x * y = " << x * y << std::endl;
    std::cout << "x / y = " << x / y << " (integer division)" << std::endl;
    std::cout << "x % y = " << x % y << " (remainder)" << std::endl;
    std::cout << "price / y = " << price / y << " (decimal division)" << std::endl;

    // =========================
    // 4. Comparisons + if/else
    // =========================
    std::cout << "\n--- Comparisons ---" << std::endl;
    if (x > y)
    {
        std::cout << "x is greater than y" << std::endl;
    }
    else if (x == y)
    {
        std::cout << "x is equal to y" << std::endl;
    }
    else
    {
        std::cout << "x is less than y" << std::endl;
    }

    // =========================
    // 5. Function call
    // =========================
    std::cout << "\n--- Function ---" << std::endl;
    int result = add(x, y);
    std::cout << "add(x, y) = " << result << std::endl;

    // =========================
    // 6. For loop
    // =========================
    std::cout << "\n--- For Loop ---" << std::endl;
    for (int i = 1; i <= 5; i++)
    {
        std::cout << "For loop iteration: " << i << std::endl;
    }

    // =========================
    // 7. While loop
    // =========================
    std::cout << "\n--- While Loop ---" << std::endl;
    int counter = 1;
    while (counter <= 3)
    {
        std::cout << "While loop counter: " << counter << std::endl;
        counter++;
    }

    // =========================
    // 8. Vector (dynamic array)
    // =========================
    std::cout << "\n--- Vector ---" << std::endl;
    std::vector<int> numbers = {10, 20, 30, 40, 50};

    std::cout << "Numbers in vector: ";
    for (int num : numbers)
    {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    // =========================
    // 9. Sum values in vector
    // =========================
    int sum = 0;
    for (int num : numbers)
    {
        sum += num;
    }
    std::cout << "Sum of vector values = " << sum << std::endl;

    // =========================
    // 10. User input
    // =========================
    std::cout << "\n--- User Input ---" << std::endl;
    std::string name;
    std::cout << "Enter your name: ";
    std::cin >> name;

    std::cout << "Hello, " << name << "!" << std::endl;

    // =========================
    // 11. Final message
    // =========================
    std::cout << "\nProgram finished successfully." << std::endl;

    return 0;
}