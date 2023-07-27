CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server
OBJS = ../src/log_system/*.cpp ../src/sql_connection_pool/*.cpp ../src/timer/*.cpp \
       ../src/http/*.cpp ../src/server/*.cpp \
       ../src/data_buffer/*.cpp ../src/main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)




