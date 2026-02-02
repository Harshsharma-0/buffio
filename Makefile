DEBUG = BUFDEBUG 

main:main.cpp *.hpp
	 g++ -std=c++20 -O3 -g main.cpp -o main -lpthread -D${DEBUG}

run: main
	./main

test: main buffioservertest
	 ./main & ./buffioservertest
