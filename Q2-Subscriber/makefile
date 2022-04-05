compile:
	@rm -r bin || true
	@mkdir bin || true
	@rm -r logs || true
	@mkdir logs || true
	@g++ -o bin/Broker.o Broker.cpp helpers/ThreadPool.cpp -pthread -fpic -fsanitize=address -static-libasan
	@g++ -o bin/Publisher.o Publisher.cpp -g -fpic
	@g++ -o bin/Subscriber.o Subscriber.cpp -g -fpic || true

info:
	@netstat -a -o -n

clean:
	@rm bin/Broker.o || true
	@rm bin/Publisher.o || true
	@rm bin/Subscriber.o || true
	@rm -r logs || true
	@rm -r bin || true
