compile:
	@g++ -o Broker.o Broker/driver.cpp -pthread
	@g++ -o Producer.o Clients/Producer.cpp -g
	@g++ -o Subscriber.o Clients/Subscriber.cpp -g

info:
	@netstat -a -o -n

clean:
	@rm Broker.o || true
	@rm Producer.o || true
	@rm Subscriber.o || true
