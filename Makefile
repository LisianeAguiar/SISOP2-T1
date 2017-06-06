CC = gcc
INC_DIR = ./include
SRC_DIR = ./src
BIN_DIR = ./bin
CLI_DIR = ./dropboxClient
SVR_DIR = ./dropboxServer

all: utils client server

utils:	$(SRC_DIR)/dropboxUtil.c 
	$(CC) -c $(SRC_DIR)/dropboxUtil.c && mv dropboxUtil.o $(BIN_DIR)

client:	$(SRC_DIR)/dropboxClient.c
	$(CC) -o $(CLI_DIR)/dropboxClient $(SRC_DIR)/dropboxClient.c $(BIN_DIR)/dropboxUtil.o -pthread

server: $(SRC_DIR)/dropboxServer.c
	$(CC) -o $(SVR_DIR)/dropboxServer $(SRC_DIR)/dropboxServer.c $(BIN_DIR)/dropboxUtil.o -pthread

