main:main.cpp buffio.hpp buffiolog.hpp
	 g++ -std=c++20 main.cpp -o main

run: main
	./main
