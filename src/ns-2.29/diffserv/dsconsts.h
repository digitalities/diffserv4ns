/* $id$
* The dsRED class supports the creation of up to MAX_QUEUES physical queues at
* each network device, with up to MAX_PREC virtual queues in each queue. */ 
#define MAX_QUEUES  9           // maximum number of physical RED queues
#define MAX_PREC    3	        // maximum number of virtual RED queues in one physical queue
#define MAX_CP     64	        // maximum number of code points in a simulation
#define MEAN_PKT_SIZE 1000 	// default mean packet size, in bytes, needed for RED calculations

#define PKT_MARKED   3
#define PKT_EDROPPED 2
#define PKT_ENQUEUED 1
#define PKT_DROPPED  0
