compile:
	g++ -o Broker.o Broker/driver.cpp -pthread
	g++ -o Producer.o Clients/Producer.cpp -g

info:
	netstat -a -o -n

run:
	./Broker.o $(count) $(start_port)

clean:
	rm Broker.o
	rm Producer.o
