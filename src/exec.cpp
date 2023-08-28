/*
 * exec.cpp
 *
 *  Created on: Jul 29, 2017
 *      Author: nullifiedcat
 */

#include "ipcb.hpp"
#include "cathookipc.hpp"

#include <string>
#include <iostream>

void ReplaceString(std::string& input, const std::string& what, const std::string& with_what)
{
    size_t index;
    index = input.find(what);
    while (index != std::string::npos)
    {
        input.replace(index, what.size(), with_what);
        index = input.find(what, index + with_what.size());
    }
}

int main(int argc, const char** argv)
{
    std::string cmd;
    if (argc < 2)
        return 1;
    char* endptr = nullptr;

    int target_id;
    try
    {
        target_id = std::stoi(argv[1]);
    }
    catch (const std::invalid_argument& ia)
    {
        std::cerr << "Invalid argument: The provided string does not contain a valid integer.\n";
        return EXIT_FAILURE;
    }
    catch (const std::out_of_range& oor)
    {
        std::cerr << "Out of range: The provided integer is out of range for int type.\n";
        return EXIT_FAILURE;
    }

    if (endptr == argv[1])
    {
        std::cerr << "Target id is NaN!\n";
        return EXIT_FAILURE;
    }

    if (target_id > cat_ipc::max_peers - 1)
    {
        std::cerr << "Invalid target id: " << target_id << '\n';
        return EXIT_FAILURE;
    }

    for (unsigned int i = 2; i < argc; ++i)
        cmd += std::string(argv[i]) + " ";

    auto peer = std::make_unique<cat_ipc::Peer<server_data_s, user_data_s>>("cathook_followbot_server", false, false);
    peer->Connect();

    std::cout << target_id << "] " << cmd << std::endl;

    {
        if (peer->memory->peer_data[target_id].free)
        {
            std::cerr << "Trying to send command to a dead peer\n";
            return EXIT_FAILURE;
        }
    }

    ReplaceString(cmd, " && ", " ; ");
    if (cmd.length() >= 63)
        peer->SendMessage(nullptr, target_id, ipc_commands::execute_client_cmd_long, cmd.c_str(), cmd.length() + 1);
    else
        peer->SendMessage(cmd.c_str(), target_id, ipc_commands::execute_client_cmd, nullptr, 0);
}
