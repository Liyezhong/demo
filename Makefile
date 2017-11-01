all:
	g++ -g -Wall schedule.cc -o schedule -I. -std=c++1y
run: all
	./schedule
clean:
	rm  -f schedule
