#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/wait.h>
using namespace std;

int main(int argc, char *argv[]){

	int opt;
	int p = 0;
	double t = -0.1;
	int e = -1;
	string filename = "\0";
	int buffercapacity = MAX_MESSAGE; //maximum bc we don't want to use uneccessary requests
	string bcapstring = "";
	bool isNewChan = false;
	// take all the arguments first because some of these may go to the server
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c::")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				break;
			case 'p':
				p = atoi(optarg); //"1"
				break;
			case 't':
				t = atof(optarg);
				break;
			case 'e':
				e = atoi(optarg);
				break;
			case 'm':
				bcapstring = optarg;
				buffercapacity = atoi(optarg);
				break;
			case 'c':
				isNewChan = true;
				break;
		}
	}

	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char* args[] = {"./server", "-m", (char*)bcapstring.c_str(), nullptr};
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}
	FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
	if(t == -0.1 && e == -1 && p!=0) {
		for(int i = 0; i < 1000; i++) {
			DataRequest d (p, i*0.004, 1);
			chan.cwrite (&d, sizeof (DataRequest)); // question
			double reply;
			chan.cread (&reply, sizeof(double)); //answer
			cout << "For person " << p <<", at time " << i*0.004 << ", the value of ecg "<< 1 <<" is " << reply << endl;

			if (!isValidResponse(&reply)){
				exit(0);
			}
			
		}
		for(int i = 0; i < 1000; i++) {
			DataRequest d (p, i*0.004, 2);
			chan.cwrite (&d, sizeof (DataRequest)); // question
			double reply;
			chan.cread (&reply, sizeof(double)); //answer
			cout << "For person " << p <<", at time " << i*0.004 << ", the value of ecg "<< 2 <<" is " << reply << endl;

			if (!isValidResponse(&reply)){
				exit(0);
			}
		}
	}else if(p!=0){
	DataRequest d (p, t, e);
	chan.cwrite (&d, sizeof (DataRequest)); // question
	double reply;
	chan.cread (&reply, sizeof(double)); //answer
	if (isValidResponse(&reply)){
		cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
	}
	}

	

	
	/* this section shows how to get the length of a file
	you have to obtain the entire file over multiple requests 
	(i.e., due to buffer space limitation) and assemble it
	such that it is identical to the original*/
	if(filename != "\0") {
		FileRequest fm (0,0);
		int len = sizeof (FileRequest) + filename.size()+1;
		char buf2 [len];
		memcpy (buf2, &fm, sizeof (FileRequest));
		strcpy (buf2 + sizeof (FileRequest), filename.c_str());
		chan.cwrite (buf2, len);  
		int64 filelen;
		chan.cread (&filelen, sizeof(int64));
		if (isValidResponse(&filelen)){
			cout << "File length is: " << filelen << " bytes" << endl;
		}

		int64 rem = filelen;
		FileRequest* f = (FileRequest*) buf2;
		int of =open(("recieved/"+filename).c_str(),O_CREAT | O_WRONLY);
		char recvbuf[buffercapacity];
		while (rem > 0) { //while the remaining is greater than 0
			f->length = min(rem, (int64)buffercapacity);
			chan.cwrite(buf2, len);
			chan.cread(recvbuf, buffercapacity);
			ftruncate(of, f->offset);
			rem -= f->length;
			f->offset+=f->length;
		}
		close(of);
	}
	
	if(isNewChan) {
		Request nc (NEWCHAN_REQ_TYPE);
		chan.cwrite(&nc, sizeof(nc));
		char chanName[1024];
		chan.cread(chanName, sizeof(chanName));

		FIFORequestChannel newchan (chanName, FIFORequestChannel::CLIENT_SIDE);

		Request q (QUIT_REQ_TYPE);
    	chan.cwrite (&q, sizeof (Request));
	}

	// closing the channel    
    Request q (QUIT_REQ_TYPE);
    chan.cwrite (&q, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	cout << "Client process exited" << endl;

}
