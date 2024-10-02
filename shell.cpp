#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    // use dup(...) to perserve standard in and out

    int in = dup(0);
    int out = dup(1);

    // get the name of the user 
    //      use getenv(...)
    char * username  = getenv("USER");  
    char cwd[PATH_MAX];

    // get the previous working directory
    //      use getcwd(...)

    // create an array or vector to store the pids of the background processes
    // create an array or vector to store the pids of interrmediate processes
    vector<int> pids_bg;
    vector<int> pids_im;

    while (true) {
        // need date/time, username, and absolute path to current dir
        //      time(...)
        //      ctime (...)
        //      getcwd(...)
        time_t currtime = time(nullptr);
        if (getcwd(cwd, sizeof(cwd)) == nullptr) { // Use the buffer instead of NULL
            perror("getcwd failed");
            continue;
        }

        // Store the cwd in a std::string for later use
        std::string currentPath(cwd);

        cout << YELLOW << ctime(&currtime) << " " << username << ":" << cwd <<"$" << NC << " ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        // reap background processes
        //  foreach (pid in packgroundPids)
        //      if (waitpid(pid, nullptr, WNOHANG)) // using WNOHANG makes it not wait
        //          print output
        //          packgroundPids.remove(pid)
        auto itr = pids_bg.begin();
        for (auto pid : pids_bg) {
            if (waitpid( pid, nullptr, WNOHANG)) {
                cout << "Done:: [" << pid << "]" << endl;
                pids_bg.erase(itr);
            }
            itr++;
        }

        if (input == "exit") {  // print exit message and break out of infinite loop
            // reap all remaining background processes
            //  similar to previous reaping, but now wait for all to finish
            //      in loop use argument of 0 instead of WNOHANG
            itr = pids_bg.begin();
            for (auto pid : pids_bg) {
                if (waitpid( pid, nullptr, 0)) {
                    cout << "Done:: [" << pid << "]" << endl;
                    pids_bg.erase(itr);
                }
                itr++;
            }
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // [optional] do dollar sign expansion

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }

        // handle directory processing
        if (tknr.commands.size() == 1 && tknr.commands.at(0)->args.at(0) == "cd") {
            // use getcwd(...) and chdir(...) to change the path
            if (tknr.commands.at(0)->args.at(1) != "-" &&  tknr.commands.at(0)->args.at(1).find("..") == std::string::npos) {
                string dir = currentPath + tknr.commands.at(0)->args.at(1);
                cout << dir << endl;
                // cout << "im in the wrong spot" << endl;
                chdir(dir.c_str());
            }
            else if (tknr.commands.at(0)->args.at(1) == "-"){
                chdir("..");
            }
            else {
                chdir(tknr.commands.at(0)->args.at(1).c_str());
            }
            continue;
            // if the path is "-" then print the previous directory and go to it
            // otherwise just go to the specified directory
        }

        // loop over piped commands
        for (size_t i = 0; i < tknr.commands.size(); i++) {
            // get the current command
            Command* currentCommand = tknr.commands.at(i);

            // pipe if the command is piped
            //  (if the number of commands is > 1)
            bool has_pipe = false;
            int fd[2];
            if (tknr.commands.size() > 1) {
                if (pipe(fd) == -1)
                {
                    perror("pipe error");
                    exit(2);
                }   
                has_pipe = true;             
            }

            // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if (pid == 0) {  // if child, exec to run command
                // run single commands with no arguments
                // change to instead create an array from the vector of arguments
                if (i < tknr.commands.size()-1 && has_pipe) {
                    dup2(fd[1], STDOUT_FILENO);
                    close(fd[0]);
                }


    if (currentCommand->hasInput()) {
        int fd_in = open(currentCommand->in_file.c_str(), O_RDONLY);
        if (fd_in < 0) {
            perror("Failed to open input file");
            exit(1);
        }
        dup2(fd_in, STDIN_FILENO); // Redirect standard input to the input file
        close(fd_in); // Close the file descriptor as it's no longer needed
    }

    // Handle output redirection (if ">" is found)
    if (currentCommand->hasOutput()) {
        int fd_out = open(currentCommand->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0) {
            perror("Failed to open output file");
            exit(1);
        }
        dup2(fd_out, STDOUT_FILENO); // Redirect standard output to the output file
        close(fd_out); // Close the file descriptor as it's no longer needed
    }

                int tknr_size = currentCommand->args.size();

                const char** args = new const char* [tknr_size+1];
                for (int c = 0; c < tknr_size; c++) {
                    args[c] = tknr.commands.at(i)->args.at(c).c_str();
                }
                args[tknr_size] = nullptr;

                if (execvp(args[0], (char **) args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
                // exit(0);

            }
            else {  // if parent, wait for child to finish
                // if the current command is a background process then store it in the vector
                //  can use currentCommand.isBackground()
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);
                int status = 0;
                if (pid > 1) {
                    waitpid(pid, &status, 0);
                    if (status > 1) {  // exit if child didn't exec properly
                        exit(status);
                    }
                }
                if (currentCommand->isBackground())
                {
                    pids_bg.push_back(pid);
                }
                // else if the last command, wait on the last command pipline
                else if(i == tknr.commands.size()-1) { 
                    wait(0); 
                }
                else {
                    pids_im.push_back(pid);
                }

                
                // else add the process to the intermediate processes          
                // dup standardin
            }
            close(fd[0]);
            close(fd[1]);
        }


        dup2(in, 0);
        dup2(out, 1);


        // restore standard in and out using dup2(...)

        // print out the reaped background processes
            itr = pids_im.begin();
            for (auto pid : pids_im) {
                if (waitpid( pid, nullptr, 0)) {
                    cout << "Done:: [" << pid << "]" << endl;
                    pids_im.erase(itr);
                }
                else {
                    itr++;
                }
            }

        // reap intermediate processes
    }
}
