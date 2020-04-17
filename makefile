ping_cf:ping.o main.o
	g++ -o ping_cf ping.o main.o -lm
ping.o:ping.cpp
	g++ -c ping.cpp -o ping.o -lm
main.o:main.cpp
	g++ -c main.cpp -o main.o -lm
.PHONY:clean
clean:
	rm -rf ping.o main.o ping_cf
