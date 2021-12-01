#include "common.h"
#include "FIFOreqchannel.h"
#include "BoundedBuffer.h"
#include "HistogramCollection.h"
#include <sys/wait.h>
#include <thread>
using namespace std;

struct response {
	int p;
	double value;
	response(int _p, double _v) {
		p = _p;
		value = _v;
	}
};

void patient_thread_function(int p, int n, BoundedBuffer* requestBuffer){
    /* What will the patient threads do? */
	DataRequest r (p, 0.0, 1);
	for(int i = 0; i < n; i++) {
		// cout << "time is " << r.seconds << endl;
		vector<char> v = vector<char>((char*)&r, (char*)&r + sizeof(DataRequest));
		requestBuffer->push(v);
		r.seconds += 0.004;
	}
	// cout << "for loop done" << endl;
}

void worker_thread_function(BoundedBuffer* requestBuf, BoundedBuffer* responseBuf, FIFORequestChannel* chan, int buffercapacity){
    /*
		Functionality of the worker threads	
    */
   while(true) {
		vector <char> rt = requestBuf->pop();
		char* data = rt.data();
		Request* r = (Request*)data;

	   if(r->getType() == DATA_REQ_TYPE) {
		   DataRequest* req = (DataRequest*) data;
		//    cout << req->person << " " << req->seconds << " " << req->ecgno << endl;
		   chan->cwrite(req, sizeof(DataRequest));
		   double reply;
		   chan->cread(&reply, sizeof(double));
		//    cout << "reply is " << reply << endl;
		   
		   response res(req->person, reply);
		   vector<char> v = vector<char>((char*)&res, (char*)&res + sizeof(response));
		   responseBuf->push(v);
	   } 
	   else if(r->getType() == FILE_REQ_TYPE) {
		    char recvbuf[buffercapacity];
			FileRequest* f = (FileRequest*) r;
			string name = f->getFileName();
			int len = sizeof (FileRequest) + name.size()+1;

			chan->cwrite (data, len);
			chan->cread(recvbuf, buffercapacity);

			FILE * file = fopen(("received/" + name).c_str(), "r+");
			fseek(file, f->offset, SEEK_SET);
			int write = fwrite(recvbuf, 1, f->length, file);
			fclose(file);

	   }
	   else if(r->getType() == QUIT_REQ_TYPE) {
		   chan->cwrite(r, sizeof(r));
		   break;
	   }
   }
   
}
void histogram_thread_function (HistogramCollection* histCol, BoundedBuffer* histBuf){
	//read from request buffer
	while(true) {
		vector <char> rt = histBuf->pop();
		char* data = rt.data();
		response* r = (response*)data;
		if(r->p == 0) {
			break;
		}
		histCol->update(r->p, r->value);
	}
	
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
	cout << "fileLenght: " << filelen << endl;

	FILE* of = fopen(("received/"+filename).c_str(), "w");
	int write = fseek(of, filelen+1, SEEK_SET);
	fclose(of);
	while (rem > 0) { //while the remaining is greater than 0; 
		f->length = min(rem, (int64)buffercapacity); //this line updates the length
		vector<char> v = vector<char> ((char*)&buf2, (char*)&buf2 + len);
		requestBuf->push(v);
		rem -= f->length; //updates while loop
		f->offset+=f->length; //updates another parameter in file request packet
	}
}

HistogramCollection hc;
bool finish = false;

void signal_handler(int sgno) {
	system("clear");
	hc.print();
	if(!finish) {
		alarm(2);
	}
}

int main(int argc, char *argv[]){

	int opt;
	int p = 0; //number of patient threads
	double t = -0.1;
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
				finish = true;
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
	FIFORequestChannel** wchans = new FIFORequestChannel*[w];
	BoundedBuffer request_buffer(b);
	BoundedBuffer response_buffer(b);
	//HistogramCollection hc;

	for(int i = 0; i < p; i++) {
		Histogram* hist = new Histogram(10, -2, 2);
		hc.add(hist);
	}


	struct timeval start, end;
    gettimeofday (&start, 0);

	if(filename == "") {
		signal(SIGALRM, signal_handler);
		alarm(2);
	}

    /* Start all threads here */

	//create threads
	vector<thread> patients;
	vector<thread> workers;
	vector<thread> histograms;
	for(int i = 1; i <= p; i++) {
		// thread patient_thread(patient_thread_function, p, n, &request_buffer);
		// patients.push_back(patient_thread);
		// thread patient_thread(patient_thread_function, p, n, &request_buffer);
		patients.push_back(thread(patient_thread_function, i, n, &request_buffer));
		
	}
	for(int j = 0; j < w; j++) {
		char chanName[1024];
		Request newchan(NEWCHAN_REQ_TYPE);
		chan.cwrite(&newchan, sizeof(newchan));
		chan.cread(chanName, sizeof(chanName));
		wchans[j] = new FIFORequestChannel(chanName, FIFORequestChannel::CLIENT_SIDE);

		// thread worker_thread(worker_thread_function, &request_buffer, &response_buffer, wchans[j], m);
		workers.push_back(thread(worker_thread_function, &request_buffer, &response_buffer, wchans[j], m));
	}
	for(int k = 0; k < h; k++) {
		// thread histogram_thread(histogram_thread_function, &hc, &response_buffer);
		histograms.push_back(thread(histogram_thread_function, &hc, &response_buffer));
	}
	//there will only be one file thread
	if(filename != "") {

		//find the length of the file
		FileRequest fm (0,0);
		int len = sizeof (FileRequest) + filename.size()+1;
		char buf2 [len];
		memcpy (buf2, &fm, sizeof (FileRequest));
		strcpy (buf2 + sizeof (FileRequest), filename.c_str());
		chan.cwrite (&buf2, len);  
		int64 filelen;
		chan.cread (&filelen, sizeof(int64));
		thread file_thread(file_thread_function, filename, filelen, m, &request_buffer);
		file_thread.join();
	}


	/* Join all threads here */
	//join patient and file threads
	for(int i = 0; i < patients.size(); i++) {
		patients.at(i).join();
	}	

	cout << "patients joined" << endl;

	//push w quit messages to request buffer
	Request q (QUIT_REQ_TYPE);
	vector<char> quit_message = vector<char>((char*)&q, (char*)&q + sizeof(Request));
	for(int j = 0; j < w; j++) {
		request_buffer.push(quit_message);
	}

	cout << "quit requests sent" << endl;

	//join worker threads
	for(int k = 0; k < w; k++) {
		workers.at(k).join();
	}

	cout << "workers joined" << endl;

	//push special packets to response buffer to indicate that histogram threads can join
	response res(0, 4);
	vector<char> v = vector<char>((char*)&res, (char*)&res + sizeof(response));
	for(int i = 0; i < h; i++) {
		response_buffer.push(v);
	}

	for(int j = 0;j < histograms.size(); j++) {
		histograms.at(j).join();
	}

	cout << "histograms joined" << endl;

	finish = true;

    gettimeofday (&end, 0);

    // print the results and time difference
	hc.print ();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// delete all dynamic channels
	for (int i = 0; i < w; ++i) {
	 	delete wchans[i];
	 }
	
	// closing the channel    
    Request quit (QUIT_REQ_TYPE);
    chan.cwrite (&quit, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	cout << "Client process exited" << endl;

}
