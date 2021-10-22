#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/wait.h>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

#define MAX_PIPE_SIZE 65536

int main(int argc, char *argv[]){
	int opt;
	int p = 1;
	double t = -1;
	int e = -1;
	string filename = "";
	int buffercapacity = MAX_MESSAGE;
	string bcapstring = "";
	bool isNewChan = false;
	bool reqFile = false;
	struct timeval start, end;
	double timeTaken;

	// take all the arguments first because some of these may go to the server
	// p->patient #, t->time (0-60), e->ecg val (1 or 2), f->
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				reqFile = true;
				std::cout << "File name: " << filename << endl;
				break;
			case 'p':
				p = atoi (optarg);
				std::cout << "Patient number: " << p << endl;
				break;
			case 't':
				t = atof(optarg);
				std::cout << "time: " << t << endl;
				break;
			case 'e':
				e = atoi (optarg);
				std::cout << "ecg number: " << e << endl;
				break;
			case 'm':
				bcapstring = optarg;
				buffercapacity = atoi (optarg);
				std::cout << "buffer cap: " << buffercapacity << endl;
				break;
			case 'c':
				isNewChan = true;
				std::cout << "Requesting New Channel" << endl;
				break;
		}
	}

	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char* args[] = {"./server", "-m", (char*)to_string(buffercapacity).c_str(), nullptr};
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}

	FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
	if (t == -1 && e == -1 && filename == "") {
		gettimeofday(&start, NULL);
		string contents;
		ostringstream ostr;
		for (int i = 0; i < 1000; i++) {
			double time = i*0.004;
			DataRequest d(p, time, 1);
			chan.cwrite (&d, sizeof (DataRequest)); // question
			double reply;
			chan.cread (&reply, sizeof(double)); //answer
			if (!isValidResponse(&reply)){
				std::cout << "Invalid Response 1" << endl;
				exit (0);
			}

			DataRequest d2(p, time, 2);
			chan.cwrite (&d2, sizeof (DataRequest));
			double reply2;
			chan.cread(&reply2, sizeof(double));
			if (!isValidResponse(&reply2)) {
				std::cout << "Invalid Response 2" << endl;
				exit(0);
			}

			// print out output for testing
			// std::cout << "time: " << time << " -> " << reply << " " << reply2 << endl;
			
			// write to file
			ostr << time << "," << reply << "," << reply2 << endl;
		}
		contents = ostr.str();
		fstream file;
		string fname = to_string(p) + "_first_1000.csv";
		file.open(fname, fstream::out);
		file << contents;

		gettimeofday(&end, NULL);
		timeTaken = (((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) /1000000.0);
		std::cout << "time elapsed while transferring first 1000: " << timeTaken << endl;
	} else if ((t != -1) && (e != -1) && (filename == "")) {
		gettimeofday(&start, NULL);
		DataRequest dCheck(10, 4, 1);
		chan.cwrite (&dCheck, sizeof (DataRequest)); // question
		double replyCheck;
		chan.cread (&replyCheck, sizeof(double)); //answer
		if (!isValidResponse(&replyCheck)){
			std::cout << "Invalid Response 1" << endl;
			exit (0);
		}
		std::cout << "For patient " << p << 
		", at time " << t << "ms, ECG num " <<
		e << "is " << replyCheck << endl;

		gettimeofday(&end, NULL);
		timeTaken = (((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) /1000000.0);
		std::cout << "time elapsed while transferring 1 data point: " << timeTaken << endl;
	}

	/* this section shows how to get the length of a file
	you have to obtain the entire file over multiple requests 
	(i.e., due to buffer space limitation) and assemble it
	such that it is identical to the original*/
	if (reqFile) {
		gettimeofday(&start, NULL);
		FileRequest fm (0,0);
		int len = sizeof (FileRequest) + filename.size()+1;
		char buf2 [len];
		memcpy (buf2, &fm, sizeof (FileRequest));
		strcpy (buf2 + sizeof (FileRequest), filename.c_str());
		chan.cwrite (buf2, len);  
		int64 filelen;
		chan.cread (&filelen, sizeof(int64));
		if (isValidResponse(&filelen)){
			std::cout << "File length is: " << filelen << " bytes" << endl;
		}
	
		// original if statemnt content
		int64 rem = filelen;
		FileRequest* f = (FileRequest*) buf2;	
		string filenameComplete = "received/" + filename;
		ofstream of (filenameComplete);
		std::cout << f->length << " " << f->offset << endl;
		while (rem > 0) {
			f->length = min(rem, (int64)buffercapacity);
			chan.cwrite (buf2, len);

			// write a loop that keeps cread()ing until the return value
			// of it (indicating the number of bytes read) equals 0
			//int bytesLeft = chan.cread(recvBuf, buffercapacity)
			/*
			char recvBuf[f->length];
			chan.cread(recvBuf, f->length);
			of.write(recvBuf, f->length);
			f->offset += f->length;
			*/
			
			int bytesLeft = f->length;
			// char recvBuf [f->length];
			while (bytesLeft != 0) {
				char recvBuf[min(f->length, MAX_PIPE_SIZE)];
				int bytesRead = chan.cread(recvBuf, bytesLeft);
				of.write(recvBuf, min(bytesLeft, MAX_PIPE_SIZE));
				std::cout << "bytesRead: " << bytesRead << endl;
				f->offset += bytesRead;
				bytesLeft -= bytesRead;
				std::cout << "bytesLeft: " << bytesLeft << endl;
			}
			std::cout << "offset: " << f->offset << endl;
			std::cout << "--------------------------------------" << endl;
			rem -= 	f->length;
		}
		gettimeofday(&end, NULL);
		timeTaken = (((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) /1000000.0);
		std::cout << "time elapsed while writing file: " << timeTaken << endl;
	}

	if (isNewChan) {
		Request nc (NEWCHAN_REQ_TYPE);
		chan.cwrite(&nc, sizeof(nc));
		char chanName [1024];
		chan.cread(chanName, sizeof(chanName));

		FIFORequestChannel newchan (chanName, FIFORequestChannel::CLIENT_SIDE);
		std::cout << "New channel created..." << chanName << endl;
		
		// test new channel
		DataRequest dCheck(10, 4, 1);
		chan.cwrite (&dCheck, sizeof (DataRequest)); // question
		double replyCheck;
		chan.cread (&replyCheck, sizeof(double)); //answer
		if (!isValidResponse(&replyCheck)){
			std::cout << "Invalid Response" << endl;
			exit (0);
		}
		std::cout << "For patient 10, at time 4ms, ECG num 1 is " << replyCheck << endl;

		Request q (QUIT_REQ_TYPE);
		newchan.cwrite (&q, sizeof(Request));
		std::cout << "New channel closed..." << endl;
	}

	// closing the channel    
    Request q (QUIT_REQ_TYPE);
    chan.cwrite (&q, sizeof (Request));

	// client waiting for the server process, which is the child, to terminate
	wait(0);
	std::cout << "Client process exited" << endl;
}
