#include <iostream>
#include <functional>
#include <pthread.h>
#include <chrono>
#include <cstring>

int user_main(int argc, char **argv);

/* Demonstration on how to pass lambda as parameter.
 * "&&" means r-value reference. You may read about it online.
 */
void demonstration(std::function<void()> && lambda) {
    lambda();
}

struct thread1D {
    int start, end;
    std::function<void(int)> func1D;
};

struct thread2D {
    int row0, row1;
    int col0, col1;
    std::function<void(int,int)> func2D;
};

//record current time
inline std::chrono::steady_clock::time_point TIME() {
    return std::chrono::steady_clock::now();
}

//size of each part
inline int part_size(int total, int numThreads, int t) {
    int min = total / numThreads;
    int extra = total % numThreads;
    return min + (t<extra ? 1:0);
}

//split the range
inline void split_range(int low, int high, int numThreads, int* starts, int* ends) {
    int total = high - low;
    int current = low;

    for (int t = 0; t < numThreads; t++) {
        int size = part_size(total, numThreads, t);
        starts[t] = current;
        ends[t] = current + size;
        current += size;
    }
}


static void* start1D(void *arg) {
    thread1D *THREAD = (thread1D*)arg;
    for (int i = THREAD->start; i < THREAD->end; i++)
        THREAD->func1D(i);
    return nullptr;
}

static void* start2D(void *arg) {
    thread2D *THREAD = (thread2D*)arg;
    for (int row = THREAD->row0; row < THREAD->row1; row++)
        for (int col = THREAD->col0; col < THREAD->col1; col++)
            THREAD->func2D(row, col);
    return nullptr;
}

//create and join thread (works for both 1D and 2D)
template<typename T>
void create_and_join(int helperCount, pthread_t* ids, T* tasks, void* (*func)(void*)) {
    for (int t = 0; t < helperCount; t++) {
        if (pthread_create(&ids[t], nullptr, func, (void*)&tasks[t]) != 0) {
            perror("pthread_create failed");
            exit(1);
        }
    }

    for (int t = 0; t < helperCount; t++) {
        if (pthread_join(ids[t], nullptr) != 0) {
            perror("pthread_join failed");
            exit(1);
        }
    }
}


//assign 1D tasks
void tasks1D(thread1D* tasks, int low, int high, int numThreads, std::function<void(int)> lambda)
{
    int starts[numThreads];
    int ends[numThreads];

    split_range(low, high, numThreads, starts, ends);

    for (int t = 0; t < numThreads; t++) {
        tasks[t].start = starts[t];
        tasks[t].end = ends[t];
        tasks[t].func1D = lambda;
    }
}

//assign 2D tasks
void tasks2D(thread2D* tasks, int low1, int high1, int low2, int high2, int numThreads, std::function<void(int,int)> lambda)
{
    int starts[numThreads];
    int ends[numThreads];

    split_range(low1, high1, numThreads, starts, ends);

    for (int t = 0; t < numThreads; t++) {
        tasks[t].row0 = starts[t];
        tasks[t].row1 = ends[t];
        tasks[t].col0 = low2;
        tasks[t].col1 = high2;
        tasks[t].func2D = lambda;
    }
}


//1D
void parallel_for(int low, int high, std::function<void(int)> &&lambda, int numThreads)
{
    if (numThreads <= 0 || low > high) {
        std::cerr << "Invalid parallel_for parameters\n";
        std::exit(1);
    }

    if (high == low) return;   //empty loop

    auto T_start = TIME();

    int total = high - low;
    int helper_threads = numThreads - 1;

    pthread_t thread_ids[helper_threads];
    thread1D tasks[numThreads];

    tasks1D(tasks, low, high, numThreads, lambda);
    create_and_join(helper_threads, thread_ids, tasks, start1D);

    // Main thread handles last part
    start1D(&tasks[numThreads - 1]);

    auto T_end = TIME();
    std::chrono::duration<double> diff = T_end - T_start;
    std::cout << "parallel_for total execution time: "
              << diff.count() << " seconds\n";
}

//2D
void parallel_for(int low1, int high1, int low2, int high2, std::function<void(int,int)> &&lambda, int numThreads)
{
    if (numThreads <= 0 || low1 > high1 || low2 > high2) {
        std::cerr << "Invalid parallel_for parameters\n";
        std::exit(1);
    }

    if (high1 == low1 || high2 == low2) return;   //empty loop

    auto T_start = TIME();

    int helper_threads = numThreads - 1;

    pthread_t thread_ids[helper_threads];
    thread2D tasks[numThreads];

    tasks2D(tasks, low1, high1, low2, high2, numThreads, lambda);
    create_and_join(helper_threads, thread_ids, tasks, start2D);

    // Main thread handles last part
    start2D(&tasks[numThreads-1]);

    auto T_end = TIME();
    std::chrono::duration<double> dt = T_end - T_start;

    std::cout << "parallel_for total execution time: "
              << dt.count() << " seconds\n";
}


int main(int argc, char **argv) {
  /* 
   * Declaration of a sample C++ lambda function
   * that captures variable 'x' by value and 'y'
   * by reference. Global variables are by default
   * captured by reference and are not to be supplied
   * in the capture list. Only local variables must be 
   * explicity captured if they are used inside lambda.
   */
  int x=5,y=1;
  // Declaring a lambda expression that accepts void type parameter
  auto /*name*/ lambda1 = /*capture list*/[/*by value*/ x, /*by reference*/ &y](void) {
    /* Any changes to 'x' will throw compilation error as x is captured by value */
    y = 5;
    std::cout<<"====== Welcome to Assignment-"<<y<<" of the CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  // Executing the lambda function
  demonstration(lambda1); // the value of x is still 5, but the value of y is now 5

  int rc = user_main(argc, argv);
 
  auto /*name*/ lambda2 = [/*nothing captured*/]() {
    std::cout<<"====== Hope you enjoyed CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  demonstration(lambda2);
  return rc;
}

#define main user_main