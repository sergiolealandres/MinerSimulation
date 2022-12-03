EXE = monitor miner pirata negacionista vago

all : $(EXE)

clean :
	rm -f *.o core $(EXE) 

reset :
	rm -f *.o $(EXE) /dev/mqueue/mq_queue /dev/shm/netdata /dev/shm/block

miner:
	gcc -o miner miner.c block.c -pthread -lrt

monitor:
	gcc -o monitor monitor.c block.c -pthread -lrt

pirata:
	gcc -o pirata pirata.c block.c -pthread -lrt

negacionista:

	gcc -o negacionista pirataNegacionista.c block.c -pthread -lrt

vago:
	gcc -o vago negacionistaVago.c block.c -pthread -lrt