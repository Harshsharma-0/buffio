main:main.cpp *.hpp
	 g++ -std=c++23 -O4 main.cpp -o main

run: main
	./main

test: main buffioservertest
	 ./main & ./buffioservertest
