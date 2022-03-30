compile:
	@mkdir bin || true
	@mkdir logs || true
	@g++ -o bin/Broker.o Broker/driver.cpp -pthread
	@g++ -o bin/Publisher.o Clients/Publisher.cpp -g
	@g++ -o bin/Subscriber.o Clients/Subscriber.cpp -g

info:
	@netstat -a -o -n

clean:
	@rm bin/Broker.o || true
	@rm bin/Publisher.o || true
	@rm bin/Subscriber.o || true
	@rm -r logs || true
	@rm -r bin || true
