#include <vector>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
using namespace std;


string trim(string input) {

}
char** vec_to_char_array(vector<string>& parts) {
    char ** result = new char * [parts.size() + 1]; //add 1 for the 
    for(int i = 0; i<parts.size(); i++) {
        result[i] = (char*) parts[i].c_str();
    }
    result[parts.size()] = NULL;
    return result;
}
vector<string> split(string line, string separator = " ") {
    line = trim(line);
    vector<string> input;
    size_t start, end = 0;
    while((start = line.find_first_not_of(separator, end)) != string::npos) {   //matches
        end = line.find(separator, start);
        input.push_back(line.substr(start, end-start));     //gets the substring
    }
    return input;
}

int main () {
    vector<int> bgs; //list of bgs
    while (true) {
        for(int i = 0; i<bgs.size(); i++) {
            if(waitpid (bgs[i], 0, WNOHANG) == bgs[i]) {
                cout << "Process: " << bgs[i] << " ended" << endl;
                bgs.erase(bgs.begin() + i);
                i--;
            }
        }

        cout << "My Shell$ ";
        string inputline;
        getline (cin, inputline); //get a line from standard input
        if(inputline == string("exit")) {
            cout << "Bye!! End of shell" << endl;
            break;
        }
        int pid = fork();
        bool bg = false;
        inputline = trim(inputline);
        if(inputline[inputline.size()-1] == '&') {
            //cout << "Big process found" << endl;
            bg = true;
            inputline = inputline.substr(0, inputline.size() - 1);
        }
        if (pid ==0) { //child process
            vector<string> parts = split(inputline);
            char** args = vec_to_char_array(parts);
            execvp (args[0],args);
        } else {
            if(!bg) {
                waitpid(pid, 0, 0); //wait for the child process
            }
            else {
                bgs.push_back(pid);
            }
        }
    }
}