shower: shower.cpp
	g++ $< -o $@ -lpistache -lcrypto -lssl -lpthread
