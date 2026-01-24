main:main.cpp *.hpp
	 g++ -std=c++20 -g  main.cpp -o main

run: main
	./main

test: main buffioservertest
	 ./main & ./buffioservertest
