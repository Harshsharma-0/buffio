DEBUG = BUFDEBUG 

main:main.cpp *.hpp
	 g++ -std=c++20  main.cpp -O3 -o main -lpthread -D${DEBUG}

run: main
	./main

test: main buffioservertest
	 ./main & ./buffioservertest
