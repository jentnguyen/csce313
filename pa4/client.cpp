#include "common.h"
#include "FIFOreqchannel.h"
#include "BoundedBuffer.h"
#include "HistogramCollection.h"
#include <sys/wait.h>
using namespace std;

void patient_thread_function(int p, int n, BoundedBuffer* requestBuffer){
    /* What will the patient threads do? */
	for(int i = 0; i < 1000; i++) {
		DataRequest r (p, i*0.004, 1);
		vector<char> v = vector<char>((char*)&r, (char*)&r + sizeof(DataRequest));
		requestBuffer->push(v);
	}
}

void worker_thread_function(BoundedBuffer* requestBuf, BoundedBuffer* responseBuf, FIFORequestChannel* chan){
    /*
		Functionality of the worker threads	
    */
}
void histogram_thread_function (HistogramCollection* histCol, BoundedBuffer* histBuf){
    /*
		Functionality of the histogram threads	
    */
}
void file_thread_function(string filename, int64 filelen, int buffercapacity, BoundedBuffer* requestBuf) {
	//FileRequest f = *(FileRequest*) requestBuf;	
	int64 rem = filelen;
	FileRequest fm (0,0);
	int len = sizeof (FileRequest) + filename.size()+1;
	char buf2 [len];
	memcpy (buf2, &fm, sizeof (FileRequest));
	strcpy (buf2 + sizeof (FileRequest), filename.c_str());
	FileRequest* f = (FileRequest*) buf2;
	char recvbuf[buffercapacity];
	while (rem > 0) { //while the remaining is greater than 0; 
		f->length = min(rem, (int64)buffercapacity); //this line updates the length
		vector<char> v = vector<char> ((char*)&buf2, (char*)&buf2 + len);
		requestBuf->push(v);
		rem -= f->length; //updates while loop
		f->offset+=f->length; //updates another parameter in file request packet

	}
}
int main(int argc, char *argv[]){

	int opt;
	int p = 1; //number of patient threads
	double t = -0.1;
	int e = -1;
	int n = 1; //number of data points you want to take
	int m = 100; //buffer capacity
	int h = 1; //number of histogram threads
	int w = 1; //worker threads
	string filename = "";
	int b = 100; // size of bounded buffer, note: this is different from another variable buffercapacity/m
	// take all the arguments first because some of these may go to the server
	while ((opt = getopt(argc, argv, "f:p:n:b:w:m:h:")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				break;
			case 'p':
				p = atoi(optarg); 
				break;
			case 'n':
				n = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 'w':
				w = atoi(optarg);
				break;
			case 'm':
				m = atoi(optarg);
				break;
			case 'h':
				h = atoi(optarg);
				break;
		}
	}

	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char* args[] = {"./server", nullptr};
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}
	FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
	BoundedBuffer request_buffer(b);
	HistogramCollection hc;


	struct timeval start, end;
    gettimeofday (&start, 0);

    /* Start all threads here */
	FileRequest fm (0,0);
	int len = sizeof (FileRequest) + filename.size()+1;
	char buf2 [len];
	memcpy (buf2, &fm, sizeof (FileRequest));
	strcpy (buf2 + sizeof (FileRequest), filename.c_str());
	chan.cwrite (&buf2, len);  
	int64 filelen;
	chan.cread (&filelen, sizeof(int64));

	/* Join all threads here */
    gettimeofday (&end, 0);

    // print the results and time difference
	hc.print ();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
	
	// closing the channel    
    Request q (QUIT_REQ_TYPE);
    chan.cwrite (&q, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	cout << "Client process exited" << endl;

}
