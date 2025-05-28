# Scheduler_SFML
Schedular, with 4 queues and different algorithms, simulation


---


### Provided an exe as well, incase want to run directly, of course will require to download SFML



---
#### data.txt format

4                    // Number of processes

2                    // Time quantum

0 5 1               // Process 1: AT=0, BT=5, Priority=1

1 3 2               // Process 2: AT=1, BT=3, Priority=2

2 8 1               // Process 3: AT=2, BT=8, Priority=1

3 6 3               // Process 4: AT=3, BT=6, Priority=3

0 1 2 3             // Queue sequence (FCFS, Priority, SJF, RR)

---



## g++ -o sch schedule.cpp -lsfml-graphics -lsfml-window -lsfml-system

## ./sch

---
