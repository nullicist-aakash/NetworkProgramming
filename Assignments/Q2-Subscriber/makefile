compile:
	rm -r bin || true
	mkdir bin || true
	rm -r logs || true
	mkdir logs || true
	g++ -o bin/Broker.o Broker.cpp helpers/ThreadPool.cpp helpers/Database.cpp helpers/Time.cpp helpers/SocketLayer.cpp -pthread
	g++ -o bin/Publisher.o Publisher.cpp helpers/SocketLayer.cpp helpers/Time.cpp
	g++ -o bin/Subscriber.o Subscriber.cpp helpers/SocketLayer.cpp helpers/Time.cpp

info:
	@netstat -a -o -n

clean:
	@rm bin/Broker.o || true
	@rm bin/Publisher.o || true
	@rm bin/Subscriber.o || true
	@rm -r logs || true
	@rm -r bin || true
