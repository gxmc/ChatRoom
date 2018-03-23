#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "../src/Client.h"
#include "../src/Common.h"

using std::cout;
using std::cin;
using std::endl;
using std::string;
using std::vector;
using std::stringstream;

using chat::Client;

void getIntoService(Client& client);
void sgchat(Client& client, vector<string>& command);
void gpchat(Client& client, vector<string>& command);
void mkroom(Client& client, vector<string>& command);
void cdroom(Client& client, vector<string>& command);
void qtroom(Client& client, vector<string>& command);
void getmsg(Client& client);
void lsuser(Client& client);
void lsroom(Client& client);
void quit(Client& client);
void help();
void inputError();

int main() {
    Client client;
    string userInput;
    bool ret;

    ret = client.connectServer("127.0.0.1", 5000);
    if (!ret) {
        cout << "can't connect to server"<< endl; 
        return 0;
    }

    getIntoService(client);

    while (true) {
        cout << "$";
        getline(cin, userInput);

        if (userInput.empty()) {
            inputError(); 
            continue;
        }

        vector<string> command;
        string arg;
        stringstream ss(userInput);

        while (ss >> arg)
            command.push_back(arg); 

        if (command[0] == "sgchat" && command.size() >= 4) {
            sgchat(client, command); 
        } else if (command[0] == "gpchat" && command.size() >= 4) {
            gpchat(client, command); 
        } else if (command[0] == "mkroom" && command.size() >= 2) {
            mkroom(client, command); 
        } else if (command[0] == "cdroom" && command.size() >= 2) {
            cdroom(client, command); 
        } else if (command[0] == "qtroom" && command.size() >= 2) {
            qtroom(client, command); 
        } else if (command[0] == "getmsg") {
            getmsg(client); 
        } else if (userInput == "lsuser") {
            lsuser(client); 
        } else if (userInput == "lsroom") {
            lsroom(client); 
        } else if (userInput == "quit") {
            quit(client);
            return 0;
        } else if (userInput == "help" || userInput == "h") {
            help();
        } else {
            inputError();
        }

        cout << endl;
    }

    return 0;
}

void getIntoService(Client& client) {
    while (true) {
        string userInput, name, password;
        int ret;

        cout << "signup or signin ? ";
        getline(cin, userInput);

        if (userInput.empty() 
            || (userInput != "signup" && userInput != "signin")) {
            inputError();
            continue;
        }

        cout << "Username : ";
        getline(cin, name);
        cout << "Password : ";
        getline(cin, password);

        if (name.empty() || password.empty()) {
            inputError();
            continue;
        }

        if (userInput == "signup") {
            ret = client.signUp(name, password);

            if (ret == SIGN_UP_SUCCESS) {
                client.signIn(name, password);
                return ;
            } else if (ret == SIGN_UP_FAIL) {
                cout << "sign up fail" << endl; 
            } else
                cout << "internal error" << endl;
        } else {
            ret = client.signIn(name, password);
            if (ret == SIGN_IN_SUCCESS)
                return ;
            else if (ret == SIGN_IN_ACCOUNT_NOT_EXISTENT)
                cout << "username not existent" << endl;
            else if (ret == SIGN_IN_PASSWORD_ERROR)
                cout << "password error" << endl;
            else
                cout << "internal error" << endl;
        }
    }
}

void sgchat(Client& client, vector<string>& command) {
    size_t i;
    string name, message;

    for (i = 1; i < command.size(); i++) {
        if (command[i] != "-m") {
            name += command[i];
            name += " ";
        }
        else
            break;
    }
    name.pop_back();

    if (i < command.size() - 1 && command[i] == "-m") {
        for (i = i + 1; i < command.size(); i++) {
            message += command[i];
            message += " ";
        }
        message.pop_back();
        client.singleChat(name, message);
    } else {
        inputError(); 
    }
}

void gpchat(Client& client, vector<string>& command) {
    size_t i;
    string name, message;

    for (i = 1; i < command.size(); i++) {
        if (command[i] != "-m") {
            name += command[i];
            name += " ";
        }
        else
            break;
    }
    name.pop_back();

    if (i < command.size() - 1 && command[i] == "-m") {
        for (i = i + 1; i < command.size(); i++) {
            message += command[i];
            message += " ";
        }
        message.pop_back();
        client.groupChat(name, message);
    } else {
        inputError(); 
    }
}

void mkroom(Client& client, vector<string>& command) {
    string name;

    for (size_t i = 1; i < command.size(); i++) {
        name += command[i];
        name += " ";
    }
    name.pop_back();
    client.mkRoom(name);
}

void cdroom(Client& client, vector<string>& command) {
    string name;

    for (size_t i = 1; i < command.size(); i++) {
        name += command[i];
        name += " ";
    }
    name.pop_back();
    client.cdRoom(name);
}

void qtroom(Client& client, vector<string>& command) {
    string name;

    for (size_t i = 1; i < command.size(); i++) {
        name += command[i];
        name += " ";
    }
    name.pop_back();
    client.qtRoom(name);
}

void getmsg(Client& client) {
    vector<string>& message = client.getMessage();
    for (size_t i = 0; i < message.size(); i++)
        cout << message[i] << endl;
    message.clear();
}

void lsuser(Client& client) {
    vector<string> ret;
    
    ret = client.lsUsers();
    for (size_t i = 0; i < ret.size(); i++)
        cout << ret[i] << endl;
}

void lsroom(Client& client) {
    vector<string> ret;
    
    ret = client.lsRooms();
    for (size_t i = 0; i < ret.size(); i++)
        cout << ret[i] << endl;
}

void quit(Client& client) {
    client.exit();
}

void help() {
    cout << endl;
    cout << "sgchat [user name] -m [chat message]"<< endl;
    cout << "Talk to \"user name\" with chat message \"chat message\"" 
         << endl << endl;

    cout << "gpchat [room name] -m [chat message]"<< endl;
    cout << "Chat with the group \"room name\" with chat message"
         << "\"chat message\"" << endl << endl;

    cout << "mkroom [room name]"<< endl;
    cout << "Create a new chat room with name \"root name\"" << endl << endl;

    cout << "cdroom [room name]"<< endl;
    cout << "User enter the chat room \"room name\"" << endl << endl;

    cout << "qtroom [room name]"<< endl;
    cout << "User quit the chat room \"room name\"" << endl << endl;

    cout << "getmsg" << endl;
    cout << "Get messages from other users or groups" << endl << endl;

    cout << "lsuser" << endl;
    cout << "List all the online users on the chat server" << endl << endl;

    cout << "lsroom" << endl;
    cout << "List all the chat rooms on the chat server" << endl << endl;

    cout << "quit"<< endl;
    cout << "User quit client" << endl << endl;
}

void inputError() {
    cout << "Command error, use \"help\" or \"h\" for help."<< endl;
} 
