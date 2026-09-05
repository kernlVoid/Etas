# Etas

A high-performance, lightweight, and header-only C++17 utility library optimized for text analysis, statistical data processing, and ultra-fast thread-safe logging.

##  Features

The library is split into three main core modules:

*   **Text Analysis (`TextAnalyzer`)**: High-performance, zero-copy string parsing, tokenization, and multi-pattern search leveraging `std::string_view`.
*   **Data & Statistics (`data_utils.hpp`)**: Essential mathematical utilities including sample variance, standard deviation, quantiles, linear regression, and Spearman rank correlation.
*   **Logging Framework (`Dbg`)**: An ultra-fast, thread-safe Meyers Singleton logger designed to handle concurrent asynchronous logging to both standard error and file streams.

---

##  Module Overview & Usage

### 1. TextAnalyzer
Optimized for processing large strings without unnecessary reallocations.

```cpp
#include "TextAnalyzer.hpp"
#include <iostream>

int main() {
    std::string text = "Song1 - Title (Remix) 2:27 | Song2 - Title 1:45";
    Etas::TextAnalyzer analyzer(text);

    auto tokens = analyzer.tokenizeByWhitespace();
    std::string cleaned = analyzer.replaceAll(" - ", " [SEP] ");
    std::cout << cleaned << "\n";
}
```

### 2. Statistical Utilities (`data_utils.hpp`)
Header-only math functions for quickly analyzing datasets and identifying outliers.

```cpp
#include "data_utils.hpp"
#include <vector>
#include <iostream>

int main() {
    std::vector<double> dataset = {10.0, 12.0, 11.5, 14.2, 98.1};

    double avg = Etas::mean(dataset);
    double med = Etas::median(dataset);
    auto outliers = Etas::find_iqr_outliers(dataset);
    
    std::cout << "Mean: " << avg << " | Median: " << med << "\n";
}
```

### 3. Thread-Safe Logger (`Dbg`)
A high-throughput logging utility using atomic state checks and minimal critical sections.

```cpp
#include "Dbg.hpp"

void doSomething() {
    auto& logger = Etas::Dbg::instance();
    logger.log(Etas::Dbg::LogLevel::INFO, "doSomething", "Operation started successfully.");
}
```

---

##  Requirements

*   **Compiler**: C++17 compliant compiler (GCC 7+, Clang 5+, or MSVC 2017+).
*   **Platforms**: Fully compatible with both **Windows** and **Linux**.
*   **Installation**: Since this library is header-only, simply copy the include files into your project directory.

##  License

This project is open-source. Feel free to use and modify it for your own applications.
