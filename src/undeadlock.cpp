/*
 * undeadlock.cpp
 *
 *  Created on: Jul 29, 2017
 *      Author: nullifiedcat
 */

#include "ipcb.hpp"
#include "cathookipc.hpp"

int main()
{
    auto peer =
        std::make_unique<cat_ipc::Peer<server_data_s, user_data_s>>("cathook_followbot_server", false, false, true);
    peer->Connect();
    peer->memory->mutex.unlock();
}
