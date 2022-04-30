compile:
	g++ -o shell.o -g Terminal.cpp structures/Job.cpp structures/Process.cpp structures/Shell.cpp Parsing/Lexer.cpp Parsing/Parser.cpp Parsing/AST.cpp -fsanitize=address

clean:
	rm shell.o
