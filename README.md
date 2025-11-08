# Dungeon Matchmaker - Compilation Instructions

## Requirements
- C++11 or higher
- macOS/Linux/Unix system with pthread support

## Compilation
```bash
g++ -std=c++11 -pthread main.cpp -o main
```

Or with clang:
```bash
clang++ -std=c++11 -pthread main.cpp -o main
```

## Running
```bash
./main
```

## Input Format
- Maximum concurrent instances (n)
- Number of tank players (t)
- Number of healer players (h)
- Number of DPS players (d)
- Minimum clear time in seconds (t1)
- Maximum clear time in seconds (t2)